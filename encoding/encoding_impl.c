#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

#include "alloc.h"
#include "fatal_error.h"
#include "encoding_impl.h"

#define MAX_SAMPLES 2048

#define FATAL_AV_ERROR(msg, err) do { \
	char err_str[AV_ERROR_MAX_STRING_SIZE]; \
	av_make_error_string(msg, AV_ERROR_MAX_STRING_SIZE, err); \
	FATAL_ERROR(msg ": %s", err_str); \
} while (0)

static const struct timespec encoding_impl_sleep_dur = { .tv_sec = 0, .tv_nsec = 5000000 }; // 5ms
static const AVRational encoding_impl_audio_rate = { .num = 1, .den = 44100 };

typedef struct {
	AVStream* stream;
	AVCodecContext* codec;
} encoding_impl_av_t;

typedef struct {
	AVFrame* video;
	AVFrame* audio;
	atomic_bool active;
	mtx_t lock;
} encoding_impl_av_frame_t;

struct encoding_impl_t {
	AVFormatContext* output_format;
	AVPacket* packet;
	AVFrame* prescaled_frame;
	struct SwsContext* sws;
	encoding_impl_av_t video;
	encoding_impl_av_t audio;
	encoding_impl_av_frame_t* frames;
	uint32_t width, height;
	uint64_t abs_sample_ts;
	uint32_t num_frames;
	uint32_t position;
	thrd_t worker;
	atomic_bool exit;
};

static void encoding_impl_log_callback(void* avcl, int level, const char* fmt, va_list vl) {
	(void)avcl;
#ifdef DEBUG_ENCODING
	if (level >= AV_LOG_PANIC && level <= AV_LOG_DEBUG) {
#else
	if (level >= AV_LOG_PANIC && level <= AV_LOG_ERROR) {
#endif
		const char* level_str;
		switch (level) {
			case AV_LOG_PANIC:
				level_str = "[AV_LOG_PANIC] ";
				break;
			case AV_LOG_FATAL:
				level_str = "[AV_LOG_FATAL] ";
				break;
			case AV_LOG_ERROR:
				level_str = "[AV_LOG_ERROR] ";
				break;
			case AV_LOG_WARNING:
				level_str = "[AV_LOG_WARNING] ";
				break;
			case AV_LOG_INFO:
				level_str = "[AV_LOG_INFO] ";
				break;
			case AV_LOG_VERBOSE:
				level_str = "[AV_LOG_VERBOSE] ";
				break;
			case AV_LOG_DEBUG:
				level_str = "[AV_LOG_DEBUG] ";
				break;
			default:
				level_str = "[AV_LOG_UNKNOWN] ";
				break;
		}

		char* msg = salloc(strlen(level_str) + strlen(fmt) + 1);
		strcpy(msg, level_str);
		strcat(msg, fmt);
		vprintf(msg, vl); 
		free(msg);
	}
}

static void encoding_impl_process_packets(AVFormatContext* output_format, AVPacket* packet, encoding_impl_av_t* av) {
	int err = 0;
	while (true) {
		err = avcodec_receive_packet(av->codec, packet);

		if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
			break;
		}

		if (err) {
			FATAL_AV_ERROR("Error receiving packet", err);
		}

		av_packet_rescale_ts(packet, av->codec->time_base, av->stream->time_base);
		packet->stream_index = av->stream->index;

		err = av_interleaved_write_frame(output_format, packet);

		if (err) {
			FATAL_AV_ERROR("Error writing packet", err);
		}
	}
}

static int encoding_impl_worker_thread(void* arg) {
	encoding_impl_t* impl = arg;
	encoding_impl_av_frame_t* av_frame = impl->frames;
	uint32_t pos = 0;
	int err = 0;

	while (!atomic_load_explicit(&impl->exit, memory_order_relaxed)) {
		if (!atomic_load_explicit(&av_frame->active, memory_order_relaxed)) {
			thrd_sleep(&encoding_impl_sleep_dur, NULL);
			continue;
		}

		mtx_lock(&av_frame->lock);

		err = avcodec_send_frame(impl->video.codec, av_frame->video);
		if (err) {
			FATAL_AV_ERROR("Error while encoding video", err);
		}

		encoding_impl_process_packets(impl->output_format, impl->packet, &impl->video);

		err = avcodec_send_frame(impl->audio.codec, av_frame->audio);
		if (err) {
			FATAL_AV_ERROR("Error while encoding audio", err);
		}

		encoding_impl_process_packets(impl->output_format, impl->packet, &impl->audio);

		atomic_store_explicit(&av_frame->active, false, memory_order_relaxed);
		mtx_unlock(&av_frame->lock);

		pos = (pos + 1) % impl->num_frames;
		av_frame = &impl->frames[pos];
	}

	return 0;
}

encoding_impl_t* encoding_impl_create(const char* path, const char* extension, const char* video_codec_name,
	uint32_t bitrate_kbps, uint32_t width, uint32_t height, uint32_t fps_num, uint32_t fps_den, uint32_t frames_to_buffer) {
#ifdef DEBUG_ENCODING
	av_log_set_level(AV_LOG_DEBUG);
#else
	av_log_set_level(AV_LOG_ERROR);
#endif
	av_log_set_callback(encoding_impl_log_callback);

	encoding_impl_t* impl = zalloc(sizeof(encoding_impl_t));

	AVOutputFormat* output_format = av_guess_format(extension, path, NULL);
	if (!output_format) {
		FATAL_ERROR("Invalid format %s", extension);
	}

	if (avformat_alloc_output_context2(&impl->output_format, output_format, NULL, path) < 0) {
		FATAL_ERROR("Failed to allocate output context");
	}

	const AVCodecDescriptor* video_codec_desc = avcodec_descriptor_get_by_name(video_codec_name);
	if (!video_codec_desc) {
		FATAL_ERROR("Invalid video codec %s", video_codec_name);
	}

	AVCodec* video_codec = avcodec_find_encoder(video_codec_desc->id);
	if (!video_codec) {
		FATAL_ERROR("Could not find video codec");
	}

	impl->video.codec = avcodec_alloc_context3(video_codec);
	if (!impl->video.codec) {
		FATAL_ERROR("Could not allocate video codec context");
	}

	if (video_codec->id == AV_CODEC_ID_MPEG4) {
		impl->video.codec->codec_tag = MKTAG('X', 'V', 'I', 'D');
	}

	AVRational video_timebase;
	av_reduce(&video_timebase.num, &video_timebase.den, fps_den, fps_num, INT_MAX);

	impl->video.codec->codec_type = AVMEDIA_TYPE_VIDEO;
	impl->video.codec->bit_rate = bitrate_kbps * 1024;
	impl->video.codec->width = width * 4;
	impl->video.codec->height = height * 4;

	impl->video.codec->time_base = video_timebase;
	impl->video.codec->gop_size = 30;
	impl->video.codec->level = 0;
	impl->video.codec->thread_type = FF_THREAD_SLICE;

	switch (impl->video.codec->codec_id) {
		case AV_CODEC_ID_FFV1:
			impl->video.codec->pix_fmt = AV_PIX_FMT_BGR0;
			break;
		case AV_CODEC_ID_UTVIDEO:
			impl->video.codec->pix_fmt = AV_PIX_FMT_GBRP;
			av_opt_set_int(impl->video.codec->priv_data, "pred", 3, 0);
			break;
		default:
			impl->video.codec->pix_fmt = AV_PIX_FMT_YUV420P;
			break;
	}

	impl->prescaled_frame = av_frame_alloc();

	impl->sws = sws_getCachedContext(NULL, width, height, AV_PIX_FMT_BGR0,
		width * 4, height * 4, impl->video.codec->pix_fmt, SWS_POINT, NULL, NULL, NULL);
	if (!impl->sws) {
		FATAL_ERROR("Failed to allocate sws context");
	}

	if (output_format->flags & AVFMT_GLOBALHEADER) {
		impl->video.codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(impl->video.codec, video_codec, NULL) < 0) {
		FATAL_ERROR("Failed to open video codec");
	}

	AVCodec* audio_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
	if (!audio_codec) {
		FATAL_ERROR("Failed to find audio codec");
	}

	impl->audio.codec = avcodec_alloc_context3(audio_codec);
	if (!impl->audio.codec) {
		FATAL_ERROR("Failed to allocate audio codec");
	}

	impl->audio.codec->codec_type = AVMEDIA_TYPE_AUDIO;
	impl->audio.codec->time_base = encoding_impl_audio_rate;
	impl->audio.codec->sample_rate = 44100;
	impl->audio.codec->sample_fmt = AV_SAMPLE_FMT_S16;
	impl->audio.codec->level = 1;
	impl->audio.codec->frame_size = 0;
	impl->audio.codec->channel_layout = AV_CH_LAYOUT_STEREO;

	if (output_format->flags & AVFMT_GLOBALHEADER) {
		impl->audio.codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(impl->audio.codec, audio_codec, NULL) < 0) {
		FATAL_ERROR("Failed to open audio codec");
	}

	impl->video.stream = avformat_new_stream(impl->output_format, video_codec);

	if (!impl->video.stream) {
		FATAL_ERROR("Failed to create video stream");
	}

	if (avcodec_parameters_from_context(impl->video.stream->codecpar, impl->video.codec) < 0) {
		FATAL_ERROR("Failed to init video stream");
	}

	impl->video.stream->time_base = impl->video.codec->time_base;

	impl->audio.stream = avformat_new_stream(impl->output_format, audio_codec);

	if (!impl->audio.stream) {
		FATAL_ERROR("Failed to create audio stream");
	}

	if (avcodec_parameters_from_context(impl->audio.stream->codecpar, impl->audio.codec) < 0) {
		FATAL_ERROR("Failed to init audio stream");
	}

	impl->audio.stream->time_base = impl->audio.codec->time_base;

	if (avio_open(&impl->output_format->pb, path, AVIO_FLAG_WRITE) < 0) {
		FATAL_ERROR("Failed to open %s", path);
	}

	if (avformat_write_header(impl->output_format, NULL)) {
		FATAL_ERROR("Failed to write header");
	}

	impl->packet = av_packet_alloc();

	if (!impl->packet) {
		FATAL_ERROR("Failed to allocate packet");
	}

	impl->width = width;
	impl->height = height;

	impl->frames = zalloc(sizeof(encoding_impl_av_frame_t) * frames_to_buffer);
	impl->num_frames = frames_to_buffer;
	for (uint32_t i = 0; i < frames_to_buffer; i++) {
		encoding_impl_av_frame_t* av_frame = &impl->frames[i];

		av_frame->video = av_frame_alloc();
		av_frame->video->format = impl->video.codec->pix_fmt;
		av_frame->video->width = width * 4;
		av_frame->video->height = height * 4;

		if (av_frame_get_buffer(av_frame->video, sizeof(uint32_t))) {
			FATAL_ERROR("Failed to allocate video frame");
		}

		av_frame->audio = av_frame_alloc();
		av_frame->audio->format = AV_SAMPLE_FMT_S16;
		av_frame->audio->nb_samples = MAX_SAMPLES;
		av_frame->audio->channel_layout = AV_CH_LAYOUT_STEREO;

		if (av_frame_get_buffer(av_frame->audio, sizeof(int16_t))) {
			FATAL_ERROR("Failed to allocate audio frame");
		}

		mtx_init(&av_frame->lock, mtx_plain);
	}

	thrd_create(&impl->worker, encoding_impl_worker_thread, impl);
	return impl;
}

void encoding_impl_destroy(encoding_impl_t* impl) {
	for (uint32_t i = 0; i < impl->num_frames; i++) {
		encoding_impl_av_frame_t* av_frame = &impl->frames[i];
		while (atomic_load_explicit(&av_frame->active, memory_order_relaxed)) {
			thrd_sleep(&encoding_impl_sleep_dur, NULL);
		}
		mtx_lock(&av_frame->lock);
		av_frame_free(&av_frame->video);
		av_frame_free(&av_frame->audio);
		mtx_unlock(&av_frame->lock);
		mtx_destroy(&av_frame->lock);
	}

	atomic_store_explicit(&impl->exit, true, memory_order_relaxed);
	thrd_join(impl->worker, NULL);

	avcodec_send_frame(impl->video.codec, NULL);
	encoding_impl_process_packets(impl->output_format, impl->packet, &impl->video);
	avcodec_send_frame(impl->audio.codec, NULL);
	encoding_impl_process_packets(impl->output_format, impl->packet, &impl->audio);
	av_write_trailer(impl->output_format);

	avio_closep(&impl->output_format->pb);
	avformat_free_context(impl->output_format);
	av_packet_free(&impl->packet);
	av_frame_free(&impl->prescaled_frame);
	sws_freeContext(impl->sws);
	avcodec_free_context(&impl->video.codec);
	avcodec_free_context(&impl->audio.codec);
	free(impl->frames);
	free(impl);
}

void encoding_impl_push_frame(encoding_impl_t* impl, void* video, uint32_t pitch, void* audio, uint32_t num_samples) {
	if (num_samples > MAX_SAMPLES) {
		FATAL_ERROR("Too many samples! Maximum is %d, got %d", MAX_SAMPLES, num_samples);
	}

	encoding_impl_av_frame_t* av_frame = &impl->frames[impl->position];

	while (atomic_load_explicit(&av_frame->active, memory_order_relaxed)) {
		thrd_sleep(&encoding_impl_sleep_dur, NULL);
	}

	mtx_lock(&av_frame->lock);

	impl->prescaled_frame->data[0] = video;
	impl->prescaled_frame->linesize[0] = pitch;
	sws_scale(impl->sws, (void*)impl->prescaled_frame->data, impl->prescaled_frame->linesize, 0, impl->height, av_frame->video->data, av_frame->video->linesize);
	av_frame->video->pts = av_rescale_q(impl->abs_sample_ts, encoding_impl_audio_rate, impl->video.codec->time_base);

	memcpy(av_frame->audio->data[0], audio, num_samples * 2 * sizeof(int16_t));
	av_frame->audio->nb_samples = num_samples;
	av_frame->audio->pts = impl->abs_sample_ts;

	atomic_store_explicit(&av_frame->active, true, memory_order_relaxed);
	mtx_unlock(&av_frame->lock);

	impl->abs_sample_ts += num_samples;
	impl->position = (impl->position + 1) % impl->num_frames;
}

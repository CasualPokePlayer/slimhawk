#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>

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

typedef struct {
	AVStream* stream;
	AVCodecContext* codec;
	AVFrame* frame;
} encoding_impl_av_t;

typedef struct {
	void* video;
	void* audio;
	uint32_t num_samples;
	uint64_t abs_sample_ts;
	atomic_bool active;
	mtx_t lock;
} encoding_impl_av_frame_t;

struct encoding_impl_t {
	AVFormatContext* output_format;
	AVPacket* packet;
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
	AVRational audio_rate;
	audio_rate.num = 1;
	audio_rate.den = 44100;
	int err = 0;

	while (!atomic_load_explicit(&impl->exit, memory_order_relaxed)) {
		if (!atomic_load_explicit(&av_frame->active, memory_order_relaxed)) {
			thrd_sleep(&encoding_impl_sleep_dur, NULL);
			continue;
		}

		mtx_lock(&av_frame->lock);

		impl->video.frame->pts = av_rescale_q(av_frame->abs_sample_ts, audio_rate, impl->video.codec->time_base);
		impl->video.frame->data[0] = av_frame->video;
		impl->video.frame->linesize[0] = impl->width * sizeof(uint32_t);
		impl->video.frame->format = AV_PIX_FMT_BGR0;
		impl->video.frame->width = impl->width;
		impl->video.frame->height = impl->height;		

		err = avcodec_send_frame(impl->video.codec, impl->video.frame);
		if (err) {
			FATAL_AV_ERROR("Error while encoding video", err);
		}

		encoding_impl_process_packets(impl->output_format, impl->packet, &impl->video);

		impl->audio.frame->pts = av_frame->abs_sample_ts;
		impl->audio.frame->data[0] = av_frame->audio;
		impl->audio.frame->nb_samples = av_frame->num_samples;

		err = avcodec_send_frame(impl->audio.codec, impl->audio.frame);
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

	int num, den;
	av_reduce(&num, &den, fps_den, fps_num, INT_MAX);

	impl->video.codec->codec_type = AVMEDIA_TYPE_VIDEO;
	impl->video.codec->bit_rate = bitrate_kbps;
	impl->video.codec->width = width;
	impl->video.codec->height = height;

	impl->video.codec->time_base.num = num;
	impl->video.codec->time_base.den = den;
	impl->video.codec->gop_size = 1;
	impl->video.codec->level = 1;
	impl->video.codec->pix_fmt = AV_PIX_FMT_BGR0;

	if (impl->video.codec->codec_id == AV_CODEC_ID_UTVIDEO) {
		av_opt_set_int(impl->video.codec->priv_data, "pred", 3, 0);
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
	impl->audio.codec->time_base.num = 1;
	impl->audio.codec->time_base.den = 44100;
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

	impl->video.frame = av_frame_alloc();

	impl->video.frame->format = AV_PIX_FMT_BGR0;
	impl->video.frame->width = width;
	impl->video.frame->height = height;

	if (av_frame_get_buffer(impl->video.frame, 1)) {
		FATAL_ERROR("Failed to allocate video frame");
	}

	impl->audio.frame = av_frame_alloc();

	impl->audio.frame->format = AV_SAMPLE_FMT_S16;
	impl->audio.frame->nb_samples = MAX_SAMPLES;
	impl->audio.frame->channel_layout = AV_CH_LAYOUT_STEREO;

	if (av_frame_get_buffer(impl->audio.frame, 1)) {
		FATAL_ERROR("Failed to allocate audio frame");
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
		impl->frames[i].video = salloc(width * height * sizeof(uint32_t));
		impl->frames[i].audio = salloc(MAX_SAMPLES * 2 * sizeof(int16_t));
		mtx_init(&impl->frames[i].lock, mtx_plain);
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
		free(av_frame->video);
		free(av_frame->audio);
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
	avcodec_free_context(&impl->video.codec);
	av_frame_free(&impl->video.frame);
	avcodec_free_context(&impl->audio.codec);
	av_frame_free(&impl->audio.frame);
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

	uint8_t* vps = video;
	uint32_t* vpd = av_frame->video;
	for (uint32_t i = 0; i < impl->height; i++) {
		memcpy(vpd, vps, impl->width * sizeof(uint32_t));
		vps += pitch;
		vpd += impl->width;
	}
	memcpy(av_frame->audio, audio, num_samples * 2 * sizeof(int16_t));
	av_frame->num_samples = num_samples;
	av_frame->abs_sample_ts = impl->abs_sample_ts;
	atomic_store_explicit(&av_frame->active, true, memory_order_relaxed);

	mtx_unlock(&av_frame->lock);

	impl->abs_sample_ts += num_samples;
	impl->position = (impl->position + 1) % impl->num_frames;
}

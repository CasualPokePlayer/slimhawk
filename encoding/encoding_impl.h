#ifndef _ENCODING_IMPL_H_
#define _ENCODING_IMPL_H_

struct encoding_impl_t;
typedef struct encoding_impl_t encoding_impl_t;

encoding_impl_t* encoding_impl_create(const char* path, const char* extension, const char* video_codec_name,
	uint32_t bitrate_kbps, uint32_t width, uint32_t height, uint32_t fps_num, uint32_t fps_den, uint32_t frames_to_buffer);
void encoding_impl_destroy(encoding_impl_t* impl);
void encoding_impl_push_frame(encoding_impl_t* impl, void* video, uint32_t pitch, void* audio, uint32_t num_samples);

#endif

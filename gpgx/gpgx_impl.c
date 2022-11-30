#include "alloc.h"
#include "fatal_error.h"
#include "stub.h"
#include "gpgx_api.h"
#include "core.h"

typedef struct {
	core_t core;
	wbx_impl_t* wbx;
	gpgx_api_t* api;
	core_file_t* rom;
	disc_impl_t* disc;
	gpgx_api_cd_data_t* toc;
	core_file_t* firmware;
	void* load_archive_cb_stub;
	gpgx_api_load_archive_cb_t load_archive_cb;
	void* cd_read_cb_stub;
	gpgx_api_cd_read_cb_t cd_read_cb;
	uint32_t* video_buffer;
	uint32_t video_buffer_size;
	int16_t* audio_buffer;
	uint32_t audio_buffer_size;
} gpgx_impl_t;

static int32_t gpgx_impl_load_archive_callback(const char* filename, void* buffer, uint32_t max_size, void* userdata) {
	if (!buffer) {
		fprintf(stderr, "Could not satify firmware request for %s as buffer is NULL\n", filename);
		return 0;
	}

	gpgx_impl_t* impl = (gpgx_impl_t*)userdata;
	core_file_t src;

	if (!strcmp(filename, "PRIMARY_ROM")) {
		if (!impl->rom) {
			fprintf(stderr, "Could not satify firmware request for PRIMARY_ROM as none was provided.\n");
			return 0;
		}

		src.data = impl->rom->data;
		src.length = impl->rom->length;
	} else if (!strcmp(filename, "PRIMARY_CD") || !strcmp(filename, "SECONDARY_CD")) {
		if (impl->rom && !strcmp(filename, "PRIMARY_CD")) {
			fprintf(stderr, "Declined to satisfy firmware request PRIMARY_CD because PRIMARY_ROM was provided.\n");
			return 0;
		} else {
			if (!impl->disc) {
				fprintf(stderr, "Couldn't satisfy firmware request %s because none was provided.\n", filename);
				return 0;
			}

			src.data = (uint8_t*)impl->toc;
			src.length = sizeof(gpgx_api_cd_data_t);

			if (src.length != max_size) {
				fprintf(stderr, "Couldn't satisfy firmware request %s because of struct size.\n", filename);
				return 0;
			}
		}
	} else {
		if (strcmp(filename, "CD_BIOS_EU") && strcmp(filename, "CD_BIOS_JP") && strcmp(filename, "CD_BIOS_US")) {
			fprintf(stderr, "Unrecognized firmware request %s\n", filename);
			return 0;
		}

		if (!impl->firmware) {
			fprintf(stderr, "Frontend couldn't satisfy firmware request GEN:%s\n", filename);
			return 0;
		}

		src.data = impl->firmware->data;
		src.length = impl->firmware->length;
	}

	if (src.length > max_size) {
		fprintf(stderr, "Couldn't satisfy firmware request %s because %d > %d", filename, src.length, max_size);
		return 0;
	}


	memcpy(buffer, src.data, src.length);
	printf("Firmware request %s satisfied at size %d\n", filename, src.length);
	return src.length;
}

static void gpgx_impl_cd_read_callback(int32_t lba, void* dst, bool audio, void* userdata) {
	gpgx_impl_t* impl = (gpgx_impl_t*)userdata;
	if (audio) {
		if (lba < impl->toc->end) {
			disc_impl_read_lba_2352(impl->disc, lba, dst);
		} else {
			memset(dst, 0, 2352);
		}
	} else {
		disc_impl_read_lba_2048(impl->disc, lba, dst);
	}
}

static void gpgx_impl_init(core_t* core, core_files_t* files) {
	gpgx_impl_t* impl = (gpgx_impl_t*)core;
	wbx_impl_enter(impl->wbx);

	// create callback stubs
	impl->load_archive_cb_stub = stub_create(gpgx_impl_load_archive_callback, impl, 3);
	impl->cd_read_cb_stub = stub_create(gpgx_impl_cd_read_callback, impl, 3);

	// register callbacks
	wbx_impl_register_callback(impl->wbx, impl->load_archive_cb_stub);
	wbx_impl_register_callback(impl->wbx, impl->cd_read_cb_stub);

	impl->load_archive_cb = wbx_impl_get_callback_addr(impl->wbx, impl->load_archive_cb_stub);
	impl->cd_read_cb = wbx_impl_get_callback_addr(impl->wbx, impl->cd_read_cb_stub);

	// default settings more or less
	gpgx_api_init_settings_t settings;
	settings.backdrop_color = 0xFFFF00FF;
	settings.region = 0; // autodetect
	settings.low_pass_range = 0x6666;
	settings.low_freq = 880;
	settings.high_freq = 5000;
	settings.low_gain = 100;
	settings.mid_gain = 100;
	settings.high_gain = 100;
	settings.filter = 1; // low pass
	settings.input_system_a = 1; // SYSTEM_MD_GAMEPAD
	settings.input_system_b = 0; // NONE
	settings.six_button = false;
	settings.force_sram = false; // CHECKME

	if (files->num_roms) {
		impl->rom = files->roms[0];
		files->roms[0] = NULL;
	}

	if (files->num_discs) {
		impl->disc = files->discs[0];
		files->discs[0] = NULL;
	
		impl->api->gpgx_set_cdd_callback(impl->cd_read_cb);
		impl->toc = zalloc(sizeof(gpgx_api_cd_data_t));
		disc_impl_toc_t* toc = disc_impl_get_toc(impl->disc);;
		for (uint32_t i = 0; i < 99; i++) {
			impl->toc->tracks[i].start = toc->tracks[i + 1].lba;
			impl->toc->tracks[i].end = toc->tracks[i + 2].lba;
			if (!toc->tracks[i + 2].valid) {
				impl->toc->end = toc->tracks[100].lba;
				impl->toc->last = i + 1;
				impl->toc->tracks[i].end = impl->toc->end;
				break;
			}
		}
	}

	if (files->num_firmwares) {
		impl->firmware = files->firmwares[0];
		files->firmwares[0] = NULL;
	}

	if (!impl->api->gpgx_init("GEN", impl->load_archive_cb, &settings)) {
		FATAL_ERROR("gpgx_init failed!");
	}

	impl->api->gpgx_set_cdd_callback(NULL);
	wbx_impl_seal(impl->wbx);
	impl->api->gpgx_set_cdd_callback(impl->cd_read_cb);
	wbx_impl_exit(impl->wbx);
}

static void gpgx_impl_destroy(core_t* core) {
	gpgx_impl_t* impl = (gpgx_impl_t*)core;
	wbx_impl_destroy(impl->wbx);
	free(impl->api);
	free(impl->rom);
	if (impl->disc) {
		disc_impl_destroy(impl->disc);
	}
	free(impl->toc);
	free(impl->firmware);
	stub_destroy(impl->load_archive_cb_stub);
	stub_destroy(impl->cd_read_cb_stub);
	free(impl->video_buffer);
	free(impl->audio_buffer);
	free(impl);
}

static void gpgx_impl_frame_advance(core_t* core, void* controller, bool render_video, bool render_sound) {
	(void)core;
	(void)controller;
	(void)render_video;
	(void)render_sound;
}

static uint32_t* gpgx_impl_get_video(core_t* core, uint32_t* width, uint32_t* height) {
	(void)core;
	(void)width;
	(void)height;
	return NULL;
}

static int16_t* gpgx_impl_get_audio(core_t* core, uint32_t* num_samps) {
	(void)core;
	(void)num_samps;
	return NULL;
}

static uint8_t gpgx_impl_peek_byte(core_t* core, uint32_t addr) {
	(void)core;
	(void)addr;
	return 0;
}

static void gpgx_impl_poke_byte(core_t* core, uint32_t addr, uint8_t val) {
	(void)core;
	(void)addr;
	(void)val;
}

core_t* gpgx_impl_create(void) {
	gpgx_impl_t* impl = zalloc(sizeof(gpgx_impl_t));
	impl->core.init = gpgx_impl_init;
	impl->core.destroy = gpgx_impl_destroy;
	impl->core.frame_advance = gpgx_impl_frame_advance;
	impl->core.get_video = gpgx_impl_get_video;
	impl->core.get_audio = gpgx_impl_get_audio;
	impl->core.peek_byte = gpgx_impl_peek_byte;
	impl->core.poke_byte = gpgx_impl_poke_byte;
	impl->wbx = wbx_impl_create("gpgx.wbx", 512, 4 * 1024, 4 * 1024, 34 * 1024, 1 * 1024);
	impl->api = gpgx_api_create(impl->wbx);
	return &impl->core;
}
/*
#define DRIFT_ADDR 0x6FFA
#define BAD_DRIFT 0xA0

#define DISTANCE_ADDR 0x6FDC
#define TARGET_DISTANCE 0x9E340
#define READ_DISTANCE() ((uint32_t)(*(uint16_t*)&m68k_ram[DISTANCE_ADDR] << 16) | *(uint16_t*)&m68k_ram[DISTANCE_ADDR + 2])

#define SPEED_ADDR 0x6FEA

#define SCORE_TIMER_ADDR 0x7139

#define MOVIE_BUFFER_SIZE 1073741824

#define ADD_MOVIE_INPUT() do { \
	movie_buffer[movie_buffer_pos++] = input.pad[0]; \
	if (__builtin_expect(movie_buffer_pos == MOVIE_BUFFER_SIZE, false)) { \
		fwrite(movie_buffer, sizeof(uint8_t), MOVIE_BUFFER_SIZE, movie_file); \
		movie_buffer_pos = 0; \
	} \
} while (0)

#include "intro_inputs.h"

int main(int argc, char* argv[]) {
	gpgx_impl_t* impl = (gpgx_impl_t*)core_parse_cli(argc, argv);
	wbx_impl_enter(impl->wbx);

	FILE* movie_file = fopen("movie_out.bin", "wb");
	if (!movie_file) {
		FATAL_ERROR("Could not open movie file");
	}
	uint8_t* movie_buffer = salloc(MOVIE_BUFFER_SIZE); // 1 GiB buffer
	uint32_t movie_buffer_pos = 0;

	uint8_t* m68k_ram = NULL;
	int32_t size = 0;
	const char* name = impl->api->gpgx_get_memdom(0, &m68k_ram, &size);
	if (!m68k_ram || size != 0x10000 || !name || strcmp("68K RAM", name)) {
		FATAL_ERROR("Interop error in gpgx_get_memdom");
	}

	gpgx_api_input_data_t input;
	if (!impl->api->gpgx_get_control(&input, sizeof(gpgx_api_input_data_t))) {
		FATAL_ERROR("Interop error in gpgx_get_control");
	}

	for (uint32_t i = 0; i < sizeof(intro_inputs); i++) {
		input.pad[0] = intro_inputs[i];
		ADD_MOVIE_INPUT();
		impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));
		impl->api->gpgx_advance();
	}

	for (uint32_t i = 0; i < 98; i++) {
		// drive until distance is the target distance (at which point, the game awards a point)
		while (READ_DISTANCE() < TARGET_DISTANCE) {
			if (m68k_ram[DRIFT_ADDR ^ 1] > BAD_DRIFT) {
				input.pad[0] = 0x44; // A+L
			} else {
				input.pad[0] = 0x40; // A
			}

			ADD_MOVIE_INPUT();
			impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));
			impl->api->gpgx_advance();
		}

		// reset input
		input.pad[0] = 0;
		impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));

		// wait until the score timer is at 1
		while (m68k_ram[SCORE_TIMER_ADDR ^ 1] != 1) {
			ADD_MOVIE_INPUT();
			impl->api->gpgx_advance();
		}

		input.pad[0] = 0x80; // Start
		impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));

		// press start for 2 frames (to start the next trip)
		ADD_MOVIE_INPUT();
		impl->api->gpgx_advance();
		ADD_MOVIE_INPUT();
		impl->api->gpgx_advance();

		printf("Scored point - %d / 99\n", i + 1);
		fflush(stdout);
	}

	// final point, but we don't want to start up another driving session after this
	// also, we want to end input as soon as possible
	// with no input, the bus slows down. if it's a bit in the left side of the road,
	// it will stop before hitting the mud on the right (< 0x50 drift should be good here)
	// you'll get 0x181 distance by my testing, but this could be off by one
	// depending on sub-distance count. also, we need some time to get to the left side
	// of the road. too far right, and we'll hit the mud and slow down further than we want
	// for this, we'll use a buffer space of 0x400 distance, which should be plenty here
	// we'll savestate, then test if ending input completes the game
	// if it doesn't, loadstate, frame advance (pressing left if needed), repeat
	// probably not super efficient, but these are the last few frames here
	// so it doesn't really matter

	while (READ_DISTANCE() < (TARGET_DISTANCE - 0x400)) {
		if (m68k_ram[DRIFT_ADDR ^ 1] > 0x50) {
			input.pad[0] = 0x44; // A+L
		} else {
			input.pad[0] = 0x40; // A
		}

		ADD_MOVIE_INPUT();
		impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));
		impl->api->gpgx_advance();
	}

	while (true) {
		// save state
		uintptr_t state_len;
		void* state = wbx_impl_save_state(impl->wbx, &state_len);

		// reset input
		input.pad[0] = 0;
		impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));

		while (READ_DISTANCE() < TARGET_DISTANCE) {
			if (!*(uint16_t*)&m68k_ram[SPEED_ADDR]) {
				// we stopped, try again
				break;
			}
			impl->api->gpgx_advance();
		}

		if (READ_DISTANCE() < TARGET_DISTANCE) {
			wbx_impl_load_state(impl->wbx, state, state_len);
			free(state);

			if (m68k_ram[DRIFT_ADDR ^ 1] > 0x50) {
				input.pad[0] = 0x44; // A+L
			} else {
				input.pad[0] = 0x40; // A
			}

			ADD_MOVIE_INPUT();
			impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));
			impl->api->gpgx_advance();
		} else {
			// stop movie
			fwrite(movie_buffer, sizeof(uint8_t), movie_buffer_pos, movie_file);
			fclose(movie_file);
			free(movie_buffer);
			free(state); // make sure to free the state
			break;
		}
	}

	puts("Scored final point! - 99 / 99");
	fflush(stdout);

	wbx_impl_exit(impl->wbx);
	gpgx_impl_destroy(&impl->core);

	return 0;
}
*/

#include "file.h"
#include "encoding_impl.h"

#define VIDEO_CHUNK_LEN 2588602
#define VIDEO_NUM 2
#define VIDEO_FILE "desert_bus_2.avi"
#define STATE_FILE "state_2.bin"

int main(int argc, char* argv[]) {
	gpgx_impl_t* impl = (gpgx_impl_t*)core_parse_cli(argc, argv);
	wbx_impl_enter(impl->wbx);

	uint8_t* movie_buffer = NULL;
	size_t movie_len = read_entire_file("movie_out.bin", &movie_buffer);
	if (movie_len != 171285255) {
		FATAL_ERROR("Wrong movie len (expected 171285255, got %ld", movie_len);
	}

	gpgx_api_input_data_t input;
	if (!impl->api->gpgx_get_control(&input, sizeof(gpgx_api_input_data_t))) {
		FATAL_ERROR("Interop error in gpgx_get_control");
	}

	int32_t fps_num, fps_den;
	impl->api->gpgx_get_fps(&fps_num, &fps_den);

	encoding_impl_t* encoder = encoding_impl_create(VIDEO_FILE, "avi", "h264", 1024 * 12, 320, 224, fps_num, fps_den, 1024);
	uint32_t* video_buffer;
	int32_t pitch;
	impl->api->gpgx_get_video(NULL, NULL, &pitch, &video_buffer);
	int16_t* audio_buffer;
	impl->api->gpgx_get_audio(NULL, &audio_buffer);
	int32_t num_samples;

	void* state = NULL;
	uintptr_t state_len = read_entire_file(STATE_FILE, &state);
	wbx_impl_load_state(impl->wbx, state, state_len);
	free(state);
	impl->api->gpgx_set_cdd_callback(impl->cd_read_cb);
	impl->api->gpgx_invalidate_pattern_cache();

	_Pragma("GCC unroll 8") for (uint32_t i = (VIDEO_NUM * VIDEO_CHUNK_LEN); i < ((VIDEO_NUM + 1) * VIDEO_CHUNK_LEN); i++) {
		input.pad[0] = movie_buffer[i];
		impl->api->gpgx_put_control(&input, sizeof(gpgx_api_input_data_t));
		impl->api->gpgx_advance();
		impl->api->gpgx_get_audio(&num_samples, NULL);
		encoding_impl_push_frame(encoder, video_buffer, pitch, audio_buffer, num_samples); 
	}

	encoding_impl_destroy(encoder);

	wbx_impl_exit(impl->wbx);
	gpgx_impl_destroy(&impl->core);
	free(movie_buffer);

	return 0;
}
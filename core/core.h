#ifndef _CORE_H_
#define _CORE_H_

#include "disc_impl.h"

typedef struct {
	uint8_t* data;
	uint32_t length;
} core_file_t;

typedef struct {
	core_file_t** roms;
	uint32_t num_roms;
	disc_impl_t** discs;
	uint32_t num_discs;
	core_file_t** firmwares;
	uint32_t num_firmwares;
} core_files_t;

struct core_t;
typedef struct core_t core_t;

struct core_t {
	void (*init)(core_t* core, core_files_t* files);
	void (*destroy)(core_t* core);
	void (*frame_advance)(core_t* core, void* controller, bool render_video, bool render_sound);
	uint32_t* (*get_video)(core_t* core, uint32_t* width, uint32_t* height);
	int16_t* (*get_audio)(core_t* core, uint32_t* num_samps);
	uint8_t (*peek_byte)(core_t* core, uint32_t addr);
	void (*poke_byte)(core_t* core, uint32_t addr, uint8_t val);
};

core_t* core_parse_cli(int argc, char* argv[]);

#endif

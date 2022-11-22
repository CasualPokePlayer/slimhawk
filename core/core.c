#include "alloc.h"
#include "fatal_error.h"
#include "file.h"
#include "core.h"
#include "gpgx_impl.h"

static void add_core_file(core_file_t*** files, uint32_t* num_files, const char* path) {
	++(*num_files);
	*files = ralloc(*files, sizeof(core_file_t*) * (*num_files));
	core_file_t* file = salloc(sizeof(core_file_t));
	file->length = read_entire_file(path, &file->data);
	(*files)[*num_files - 1] = file;
}

static void add_disc_file(disc_impl_t*** discs, uint32_t* num_discs, const char* path) {
	++(*num_discs);
	*discs = ralloc(*discs, sizeof(disc_impl_t*) * (*num_discs));
	(*discs)[*num_discs - 1] = disc_impl_create(path);
}

core_t* core_parse_cli(int argc, char* argv[]) {
	core_files_t files;
	memset(&files, 0, sizeof(core_files_t));
	core_t* (*core_create)(void) = NULL;

	// todo: actually parse cli
	(void)argc;
	(void)argv;
	/*for (int i = 0; i < argc; i++) {
		
	}*/

	// hardcoded for now
	core_create = gpgx_impl_create;
	add_core_file(&files.firmwares, &files.num_firmwares, "scd_bios.bin");
	add_disc_file(&files.discs, &files.num_discs, "ptsm1.cue");

	if (!core_create) {
		FATAL_ERROR("Could not determine core to create");
	}

	core_t* core = core_create();
	core->init(core, &files);

	// cleanup
	for (uint32_t i = 0; i < files.num_roms; i++) {
		free(files.roms[i]);
	}
	free(files.roms);
	for (uint32_t i = 0; i < files.num_discs; i++) {
		if (files.discs[i]) {
			disc_impl_destroy(files.discs[i]);
		}
	}
	free(files.discs);
	for (uint32_t i = 0; i < files.num_firmwares; i++) {
		free(files.firmwares[i]);
	}
	free(files.firmwares);

	return core;
}
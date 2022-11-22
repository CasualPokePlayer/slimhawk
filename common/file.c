#include "alloc.h"
#include "fatal_error.h"

size_t read_entire_file(const char* path, void* buffer) {
	FILE* f = fopen(path, "rb");
	if (!f) {
		FATAL_ERROR("Could not open file %s", path);
	}

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	void* p = salloc(len);
	fseek(f, 0, SEEK_SET);
	fread(p, 1, len, f);
	fclose(f);

	*(void**)buffer = p;
	return len;
}


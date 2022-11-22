#include "alloc.h"
#include "fatal_error.h"

void* salloc(size_t size) {
	void* ret = malloc(size);
	if (!ret) {
		FATAL_ERROR("Failed to satify allocation of %ld bytes", size);
	}
	return ret;
}

void* zalloc(size_t size) {
	void* ret = salloc(size);
	memset(ret, 0, size);
	return ret;
}

void* ralloc(void* p, size_t size) {
	void* ret = realloc(p, size);
	if (!ret) {
		FATAL_ERROR("Failed to satify reallocation of %ld bytes", size);
	}
	return ret;
}

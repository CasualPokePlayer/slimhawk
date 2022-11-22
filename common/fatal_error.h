#ifndef _FATAL_ERROR_H_
#define _FATAL_ERROR_H_

#include <stdio.h>
#include <stdlib.h>

#define FATAL_ERROR(...) do { \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "%s", "\n"); \
	abort(); \
} while (0)

#endif

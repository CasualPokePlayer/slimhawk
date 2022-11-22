#ifndef _ALLOC_H_
#define _ALLOC_H_

#include <stdlib.h>
#include <string.h>

void* salloc(size_t size); // safe alloc, fatal error on failure
void* zalloc(size_t size); // safe zero clear alloc, fatal error on failure
void* ralloc(void* p, size_t size); // safely realloc, fatal error on failure

#endif

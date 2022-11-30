#ifndef _STUB_H_
#define _STUB_H_

#include <stdint.h>

void* stub_create(void* target, void* userdata, uint32_t argc);
void stub_destroy(void* stub);

#endif

#ifndef __x86_64__
#error This file can only be compiled under x86-64
#endif

#ifndef __linux__
#error This file can only be compiled on Linux
#endif

#define _GNU_SOURCE
#include <sys/mman.h>
#undef _GNU_SOURCE

#include "fatal_error.h"
#include "stub.h"

#define STUB_SIZE 2 + 8 + 2 + 8 + 2

void* stub_create(void* target, void* userdata, uint32_t argc) {
	if (argc > 5) {
		FATAL_ERROR("Too many args! (got %d, expected at most 5)", argc);
	}

	uint8_t* stub = mmap(NULL, STUB_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (stub == MAP_FAILED) {
		FATAL_ERROR("Failed to map stub");
	}

	static const uint16_t mov_opcodes[] = { 0xBF48, 0xBE48, 0xBA48, 0xB948, 0xB849, 0xB949 };
	// mov ???, userdata
	// reg order is rdi, rsi, rdx, rcx, r8, r9
	*(uint16_t*)&stub[0] = mov_opcodes[argc];
	*(void**)&stub[2] = userdata;
	// mov rax, target
	*(uint16_t*)&stub[10] = 0xB848;
	*(void**)&stub[12] = target;
	// jmp rax
	*(uint16_t*)&stub[20] = 0xE0FF;

	if (mprotect(stub, STUB_SIZE, PROT_EXEC) == -1) {
		FATAL_ERROR("Failed to mark stub as executable");
	}

	return stub;
}

void stub_destroy(void* stub) {
	if (munmap(stub, STUB_SIZE) == -1) {
		FATAL_ERROR("Failed to unmap stub");
	}
}

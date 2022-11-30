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

#define STUB_SIZE 1 + 1 + 2 + 8 + 2 + 8 + 2 + 1 + 1 + 1

void* stub_create(void* target, void* userdata, uint32_t argc) {
	if (argc > 5) {
		FATAL_ERROR("Too many args! (got %d, expected at most 5)", argc);
	}

	void* stub = mmap(NULL, STUB_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (stub == MAP_FAILED) {
		FATAL_ERROR("Failed to map stub");
	}

	// x86-64 sysv abi reg order for passing parameters is rdi, rsi, rdx, rcx, r8, r9
	// note that the caller will not know if we are passing an additional parameter
	// so we need to push before we call our intended target, then pop and return
	static const uint8_t push_opcodes[] = { 0x57, 0x56, 0x52, 0x51, 0x50, 0x51 };
	static const uint16_t mov_opcodes[] = { 0xBF48, 0xBE48, 0xBA48, 0xB948, 0xB849, 0xB949 };
	static const uint8_t pop_opcodes[] = { 0x5F, 0x5E, 0x5A, 0x59, 0x58, 0x59 };

	uint8_t* opcodes = stub;

	// push r*
	if (argc > 3) {
		*opcodes++ = 0x41; // this is needed for r8 and r9
	}
	*opcodes++ = push_opcodes[argc];

	// mov r*, userdata
	*(uint16_t*)opcodes = mov_opcodes[argc];
	opcodes += 2;
	*(void**)opcodes = userdata;
	opcodes += 8;

	// mov rax, target
	*(uint16_t*)opcodes = 0xB848;
	opcodes += 2;
	*(void**)opcodes = target;
	opcodes += 8;

	// call rax
	*(uint16_t*)opcodes = 0xD0FF;
	opcodes += 2;

	// pop r*
	if (argc > 3) {
		*opcodes++ = 0x41; // same here
	}
	*opcodes++ = pop_opcodes[argc];

	// ret
	*opcodes++ = 0xC3;

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

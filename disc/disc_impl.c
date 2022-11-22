#include "alloc.h"
#include "fatal_error.h"
#include "disc_impl.h"

// mednadisc imports
void* mednadisc_LoadCD(const char* filename);
void mednadisc_ReadTOC(void* disc, disc_impl_toc_t* toc, disc_impl_track_t* tracks);	
int32_t mednadisc_ReadSector(void* disc, int32_t lba, void* buf_2448);
void mednadisc_CloseCD(void* disc);

struct disc_impl_t {
	void* ctx;
	disc_impl_toc_t toc;
	uint8_t buf_2442[2442];
};

disc_impl_t* disc_impl_create(const char* filename) {
	disc_impl_t* impl = zalloc(sizeof(disc_impl_t));
	impl->ctx = mednadisc_LoadCD(filename);
	if (!impl->ctx) {
		FATAL_ERROR("mednadisc rejected %s!", filename);
	}
	mednadisc_ReadTOC(impl->ctx, &impl->toc, impl->toc.tracks);
	return impl;
}

void disc_impl_destroy(disc_impl_t* impl) {
	mednadisc_CloseCD(impl->ctx);
	free(impl);
}

static void disc_impl_deinterleave(uint8_t* buffer) {
	uint8_t out_buf[96];
	memset(out_buf, 0, sizeof(out_buf));

	for (uint32_t ch = 0; ch < 8; ch++) {
		for (uint32_t i = 0; i < 96; i++) {
			out_buf[(ch * 12) + (i >> 3)] |= (((buffer[i] >> (7 - ch)) & 1) << (7 - (i & 7)));
		}
	}

	memcpy(buffer, out_buf, sizeof(out_buf));
}

void disc_impl_read_lba_2448(disc_impl_t* impl, int32_t lba, void* buffer, bool deinterlave) {
	memset(buffer, 0, 2352);
	mednadisc_ReadSector(impl->ctx, lba, buffer);
	if (deinterlave) {
		disc_impl_deinterleave((uint8_t*)buffer + 2352);
	}
}

void disc_impl_read_lba_2352(disc_impl_t* impl, int32_t lba, void* buffer) {
	memset(impl->buf_2442, 0, 2352);
	mednadisc_ReadSector(impl->ctx, lba, impl->buf_2442);
	memcpy(buffer, impl->buf_2442, 2352);
}

void disc_impl_read_lba_2048(disc_impl_t* impl, int32_t lba, void* buffer) {
	memset(impl->buf_2442, 0, 2072);
	mednadisc_ReadSector(impl->ctx, lba, impl->buf_2442);

	if (impl->buf_2442[15] == 1) {
		memcpy(buffer, &impl->buf_2442[16], 2048);
	} else if (impl->buf_2442[15] == 2) {
		if (impl->buf_2442[18] >> 5 & 1) {
			memset(buffer, 0, 2048);
			return;
		}
		memcpy(buffer, &impl->buf_2442[24], 2048);
	} else {
		memset(buffer, 0, 2048);
	}
}

disc_impl_toc_t* disc_impl_get_toc(disc_impl_t* impl) {
	return &impl->toc;
}

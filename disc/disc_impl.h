#ifndef _DISC_H_
#define _DISC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
	uint8_t adr;
	uint8_t control;
	uint32_t lba;
	bool valid;
} disc_impl_track_t;

typedef struct {
	uint8_t first_track;
	uint8_t last_track;
	uint8_t disc_type;
	disc_impl_track_t tracks[101];
} disc_impl_toc_t;

struct disc_impl_t;
typedef struct disc_impl_t disc_impl_t;

disc_impl_t* disc_impl_create(const char* filename);
void disc_impl_destroy(disc_impl_t* impl);
void disc_impl_read_lba_2448(disc_impl_t* impl, int32_t lba, void* buffer, bool deinterlave);
void disc_impl_read_lba_2352(disc_impl_t* impl, int32_t lba, void* buffer);
void disc_impl_read_lba_2048(disc_impl_t* impl, int32_t lba, void* buffer);
disc_impl_toc_t* disc_impl_get_toc(disc_impl_t* impl);

#endif

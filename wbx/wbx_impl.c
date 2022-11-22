#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
#include "fatal_error.h"
#include "file.h"
#include "min_max.h"
#include "wbx_impl.h"

typedef struct {
	uint8_t* buffer;
	uintptr_t size;
	uintptr_t pos;
} wbx_impl_reader_t;

// the same, for now...
typedef wbx_impl_reader_t wbx_impl_writer_t;

struct wbx_impl_t {
	void* ctx;
	int32_t enter_cnt;
	void** callbacks;
	uint32_t num_callbacks;
};

static uintptr_t wbx_impl_read_callback(void* userdata, void* data, uintptr_t size) {
	wbx_impl_reader_t* reader = userdata;

	uintptr_t len_rm = reader->size - reader->pos;
	if (len_rm == 0) {
		return 0;
	}

	uintptr_t len = MIN(size, len_rm);
	memcpy(data, &reader->buffer[reader->pos], len);
	reader->pos += len;
	return len;
}

static int32_t wbx_impl_write_callback(void* userdata, void* data, uintptr_t size) {
	wbx_impl_reader_t* writer = userdata;

	uintptr_t len_rm = writer->size - writer->pos;
	if (size > len_rm) {
		writer->size += size - len_rm;
		writer->buffer = ralloc(writer->buffer, writer->size);
	}

	memcpy(&writer->buffer[writer->pos], data, size);
	writer->pos += size;
	return 0;
}

wbx_impl_t* wbx_impl_create(const char* path, uintptr_t sbrk_size_kb, uintptr_t sealed_size_kb, uintptr_t invis_size_kb, uintptr_t plain_size_kb, uintptr_t mmap_size_kb) {
	wbx_api_memory_layout_template_t layout;
	layout.sbrk_size = sbrk_size_kb * 1024;
	layout.sealed_size = sealed_size_kb * 1024;
	layout.invis_size = invis_size_kb * 1024;
	layout.plain_size = plain_size_kb * 1024;
	layout.mmap_size = mmap_size_kb * 1024;

	wbx_impl_reader_t wbx_file;
	wbx_file.pos = 0;
	wbx_file.size = read_entire_file(path, &wbx_file.buffer);

	wbx_api_return_data_t ret;
	wbx_create_host(&layout, path, wbx_impl_read_callback, &wbx_file, &ret);
	free(wbx_file.buffer);

	wbx_impl_t* impl = zalloc(sizeof(wbx_impl_t));
	impl->ctx = wbx_api_get_data_or_abort(&ret);
	return impl;
}

void wbx_impl_destroy(wbx_impl_t* impl) {
	wbx_api_return_data_t ret;
	wbx_destroy_host(impl->ctx, &ret);
	wbx_api_get_data_or_abort(&ret);
	free(impl->callbacks);
	free(impl);
}

void* wbx_impl_get_proc_addr(wbx_impl_t* impl, const char* sym) {
	wbx_api_return_data_t ret;
	wbx_get_proc_addr(impl->ctx, sym, &ret);
	void* proc = wbx_api_get_data_or_abort(&ret);
	if (!proc) {
		FATAL_ERROR("Symbol was not exported from elf %s", sym);
	}

	return proc;
}

void wbx_impl_register_callback(wbx_impl_t* impl, void* cb) {
	impl->num_callbacks++;
	impl->callbacks = ralloc(impl->callbacks, sizeof(void*) * impl->num_callbacks);
	impl->callbacks[impl->num_callbacks - 1] = cb;
}

void* wbx_impl_get_callback_addr(wbx_impl_t* impl, void* cb) {
	for (uint32_t i = 0; i < impl->num_callbacks; i++) {
		if (impl->callbacks[i] == cb) {
			wbx_api_return_data_t ret;
			wbx_get_callback_addr(impl->ctx, cb, i, &ret);
			return wbx_api_get_data_or_abort(&ret);
		}
	}

	FATAL_ERROR("Callback was not registered");
}

void wbx_impl_seal(wbx_impl_t* impl) {
	wbx_api_return_data_t ret;
	wbx_seal(impl->ctx, &ret);
	wbx_api_get_data_or_abort(&ret);
	puts("wbx_impl sealed!");
}

void wbx_impl_add_readonly_file(wbx_impl_t* impl, void* data, uintptr_t length, const char* name) {
	wbx_impl_reader_t reader;
	reader.buffer = data;
	reader.pos = 0;
	reader.size = length;
	wbx_api_return_data_t ret;
	wbx_mount_file(impl->ctx, name, wbx_impl_read_callback, &reader, false, &ret);
	wbx_api_get_data_or_abort(&ret);
}

void wbx_impl_remove_readonly_file(wbx_impl_t* impl, const char* name) {
	wbx_api_return_data_t ret;
	wbx_unmount_file(impl->ctx, name, 0, 0, &ret);
	wbx_api_get_data_or_abort(&ret);
}

void wbx_impl_add_transient_file(wbx_impl_t* impl, void* data, uintptr_t length, const char* name) {
	wbx_impl_reader_t reader;
	reader.buffer = data;
	reader.pos = 0;
	reader.size = length;
	wbx_api_return_data_t ret;
	wbx_mount_file(impl->ctx, name, wbx_impl_read_callback, &reader, true, &ret);
	wbx_api_get_data_or_abort(&ret);
}

void* wbx_impl_remove_transient_file(wbx_impl_t* impl, uintptr_t* length, const char* name) {
	wbx_impl_writer_t writer;
	writer.buffer = NULL;
	writer.pos = 0;
	writer.size = 0;
	wbx_api_return_data_t ret;
	wbx_unmount_file(impl->ctx, name, wbx_impl_write_callback, &writer, &ret);
	wbx_api_get_data_or_abort(&ret);
	*length = writer.size;
	return writer.buffer;
}

void* wbx_impl_save_state(wbx_impl_t* impl, uintptr_t* length) {
	wbx_impl_writer_t writer;
	writer.buffer = NULL;
	writer.pos = 0;
	writer.size = 0;
	wbx_api_return_data_t ret;
	wbx_save_state(impl->ctx, wbx_impl_write_callback, &writer, &ret);
	wbx_api_get_data_or_abort(&ret);
	*length = writer.size;
	return writer.buffer;
}

void wbx_impl_load_state(wbx_impl_t* impl, void* data, uintptr_t length) {
	wbx_impl_writer_t reader;
	reader.buffer = data;
	reader.pos = 0;
	reader.size = length;
	wbx_api_return_data_t ret;
	wbx_load_state(impl->ctx, wbx_impl_read_callback, &reader, &ret);
	wbx_api_get_data_or_abort(&ret);
}

void wbx_impl_enter(wbx_impl_t* impl) {
	if (impl->enter_cnt == 0) {
		wbx_api_return_data_t ret;
		wbx_activate_host(impl->ctx, &ret);
		wbx_api_get_data_or_abort(&ret);
	}

	impl->enter_cnt++;
}

void wbx_impl_exit(wbx_impl_t* impl) {
	if (impl->enter_cnt <= 0) {
		FATAL_ERROR("Invalid enter count %d", impl->enter_cnt);
	} else if (impl->enter_cnt == 1) {
		wbx_api_return_data_t ret;
		wbx_deactivate_host(impl->ctx, &ret);
		wbx_api_get_data_or_abort(&ret);
	}

	impl->enter_cnt--;
}

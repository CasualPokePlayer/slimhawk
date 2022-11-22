#ifndef _WBX_API_H_
#define _WBX_API_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	char error_message[1024];
	void* data;
} wbx_api_return_data_t;

typedef struct {
	uintptr_t sbrk_size;
	uintptr_t sealed_size;
	uintptr_t invis_size;
	uintptr_t plain_size;
	uintptr_t mmap_size;
} wbx_api_memory_layout_template_t;

typedef uintptr_t (*wbx_api_read_callback_t)(void* userdata, void* data, uintptr_t size);
typedef int32_t (*wbx_api_write_callback_t)(void* userdata, void* data, uintptr_t size);

void wbx_create_host(wbx_api_memory_layout_template_t* layout, const char* module_name, wbx_api_read_callback_t wbx, void* userdata, wbx_api_return_data_t* ret);
void wbx_destroy_host(void* obj, wbx_api_return_data_t* ret);
void wbx_activate_host(void* obj, wbx_api_return_data_t* ret);
void wbx_deactivate_host(void* obj, wbx_api_return_data_t* ret);
void wbx_get_proc_addr(void* obj, const char* name, wbx_api_return_data_t* ret);
void wbx_get_callin_addr(void* obj, void* ptr, wbx_api_return_data_t* ret);
void wbx_get_proc_addr_raw(void* obj, const char* name, wbx_api_return_data_t* ret);
void wbx_get_callback_addr(void* obj, void* callback, int slot, wbx_api_return_data_t* ret);
void wbx_seal(void* obj, wbx_api_return_data_t* ret);
void wbx_mount_file(void* obj, const char* name, wbx_api_read_callback_t reader, void* userdata, bool writable, wbx_api_return_data_t* ret);
void wbx_unmount_file(void* obj, const char* name, wbx_api_write_callback_t writer, void* userdata, wbx_api_return_data_t* ret);
void wbx_save_state(void* obj, wbx_api_write_callback_t writer, void* userdata, wbx_api_return_data_t* ret);
void wbx_load_state(void* obj, wbx_api_read_callback_t reader, void* userdata, wbx_api_return_data_t* ret);
void wbx_set_always_evict_blocks(bool val);
void wbx_get_page_len(void* obj, wbx_api_return_data_t* ret);
void wbx_get_page_data(void* obj, uintptr_t index, wbx_api_return_data_t* ret);

void* wbx_api_get_data_or_abort(wbx_api_return_data_t* return_data);

#endif

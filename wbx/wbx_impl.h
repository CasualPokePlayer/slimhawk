#ifndef _WBX_IMPL_H_
#define _WBX_IMPL_H_

#include "wbx_api.h"

struct wbx_impl_t;
typedef struct wbx_impl_t wbx_impl_t;

wbx_impl_t* wbx_impl_create(const char* path, uintptr_t sbrk_size_kb, uintptr_t sealed_size_kb, uintptr_t invis_size_kb, uintptr_t plain_size_kb, uintptr_t mmap_size_kb);
void wbx_impl_destroy(wbx_impl_t* impl);
void* wbx_impl_get_proc_addr(wbx_impl_t* impl, const char* sym);
void wbx_impl_register_callback(wbx_impl_t* impl, void* cb);
void* wbx_impl_get_callback_addr(wbx_impl_t* impl, void* cb);
void wbx_impl_seal(wbx_impl_t* impl);
void wbx_impl_add_readonly_file(wbx_impl_t* impl, void* data, uintptr_t length, const char* name);
void wbx_impl_remove_readonly_file(wbx_impl_t* impl, const char* name);
void wbx_impl_add_transient_file(wbx_impl_t* impl, void* data, uintptr_t length, const char* name);
void* wbx_impl_remove_transient_file(wbx_impl_t* impl, uintptr_t* length, const char* name);
void* wbx_impl_save_state(wbx_impl_t* impl, uintptr_t* length);
void wbx_impl_load_state(wbx_impl_t* impl, void* data, uintptr_t length);
void wbx_impl_enter(wbx_impl_t* impl);
void wbx_impl_exit(wbx_impl_t* impl);

#endif

#ifndef _GPGX_API_H_
#define _GPGX_API_H_

#include "wbx_impl.h"

typedef WBX_CALL int32_t (*gpgx_api_load_archive_cb_t)(const char* filename, void* buffer, uint32_t max_size);
typedef WBX_CALL void (*gpgx_api_input_cb_t)(void);
typedef WBX_CALL void (*gpgx_api_mem_cb_t)(uint32_t addr);
typedef WBX_CALL void (*gpgx_api_cd_cb_t)(int32_t addr, int32_t addr_type, int32_t flags);
typedef WBX_CALL void (*gpgx_api_cd_read_cb_t)(int32_t lba, void* dst, bool audio);

typedef struct {
	uint32_t backdrop_color;
	int32_t region;
	uint16_t low_pass_range;
	int16_t low_freq;
	int16_t high_freq;
	int16_t low_gain;
	int16_t mid_gain;
	int16_t high_gain;
	uint8_t filter;
	uint8_t input_system_a;
	uint8_t input_system_b;
	bool six_button;
	bool force_sram;
} gpgx_api_init_settings_t;

typedef struct {
	uint8_t system[2];
	uint8_t dev[8];
	uint16_t pad[8];
	int16_t analog[16];
	int32_t x_offset;
	int32_t y_offset;
} gpgx_api_input_data_t;

typedef struct {
	int32_t start;
	int32_t end;
} gpgx_api_cd_track_t;

typedef struct {
	int32_t end;
	int32_t last;
	gpgx_api_cd_track_t tracks[100];
} gpgx_api_cd_data_t;

typedef struct {
	int32_t width;
	int32_t height;
	int32_t base_addr;
} gpgx_api_vdp_name_table_t;

typedef struct {
	void* vram;
	void* pattern_cache;
	void* color_cache;
	gpgx_api_vdp_name_table_t nta;
	gpgx_api_vdp_name_table_t ntb;
	gpgx_api_vdp_name_table_t ntw;
} gpgx_api_vdp_view_t;

typedef struct {
	int32_t value;
	void* name;
} gpgx_api_register_info_t;

typedef struct {
	WBX_CALL void (*gpgx_get_video)(int32_t* w, int32_t* h, int32_t* pitch, uint32_t** buffer);
	WBX_CALL void (*gpgx_get_audio)(int32_t* n, int16_t** buffer);
	WBX_CALL void (*gpgx_advance)(void);
	WBX_CALL bool (*gpgx_init)(const char* rom_extension, gpgx_api_load_archive_cb_t load_archive_cb, gpgx_api_init_settings_t* settings);
	WBX_CALL void (*gpgx_get_fps)(int32_t* num, int32_t* den);
	WBX_CALL bool (*gpgx_get_control)(gpgx_api_input_data_t* dst, int32_t bytes);
	WBX_CALL bool (*gpgx_put_control)(gpgx_api_input_data_t* src, int32_t bytes);
	WBX_CALL void* (*gpgx_get_sram)(int32_t* size);
	WBX_CALL void (*gpgx_put_sram)(uint8_t* data, int32_t size);
	WBX_CALL void (*gpgx_clear_sram)(void);
	WBX_CALL const char* (*gpgx_get_memdom)(int32_t which, uint8_t** area, int32_t* size);
	WBX_CALL void (*gpgx_reset)(bool hard);
	WBX_CALL void (*gpgx_set_input_callback)(gpgx_api_input_cb_t cb);
	WBX_CALL void (*gpgx_set_mem_callback)(gpgx_api_mem_cb_t read, gpgx_api_mem_cb_t write, gpgx_api_mem_cb_t exec);
	WBX_CALL void (*gpgx_set_cd_callback)(gpgx_api_cd_cb_t cd);
	WBX_CALL void (*gpgx_set_cdd_callback)(gpgx_api_cd_read_cb_t cdd_cb);
	WBX_CALL void (*gpgx_swap_disc)(gpgx_api_cd_data_t* toc);
	WBX_CALL void (*gpgx_get_vdp_view)(gpgx_api_vdp_view_t* view);
	WBX_CALL void (*gpgx_poke_vram)(int32_t addr, uint8_t value);
	WBX_CALL void (*gpgx_flush_vram)(void);
	WBX_CALL void (*gpgx_invalidate_pattern_cache)(void);
	WBX_CALL int32_t (*gpgx_getmaxnumregs)(void);
	WBX_CALL int32_t (*gpgx_getregs)(gpgx_api_register_info_t* regs);
	WBX_CALL void (*gpgx_set_draw_mask)(int32_t mask);
	WBX_CALL void (*gpgx_write_m68k_bus)(uint32_t addr, uint8_t data);
	WBX_CALL void (*gpgx_write_s68k_bus)(uint32_t addr, uint8_t data);
	WBX_CALL uint8_t (*gpgx_peek_m68k_bus)(uint32_t addr);
	WBX_CALL uint8_t (*gpgx_peek_s68k_bus)(uint32_t addr);
} gpgx_api_t;

gpgx_api_t* gpgx_api_create(wbx_impl_t* wbx);

#endif

#include "alloc.h"
#include "gpgx_api.h"

gpgx_api_t* gpgx_api_create(wbx_impl_t* wbx) {
	gpgx_api_t* api = salloc(sizeof(gpgx_api_t));
	#define SET_API_FP(fp) *(void**)&api->fp = wbx_impl_get_proc_addr(wbx, #fp)
	SET_API_FP(gpgx_get_video);
	SET_API_FP(gpgx_get_audio);
	SET_API_FP(gpgx_advance);
	SET_API_FP(gpgx_init);
	SET_API_FP(gpgx_get_fps);
	SET_API_FP(gpgx_get_control);
	SET_API_FP(gpgx_put_control);
	SET_API_FP(gpgx_get_sram);
	SET_API_FP(gpgx_put_sram);
	SET_API_FP(gpgx_clear_sram);
	SET_API_FP(gpgx_get_memdom);
	SET_API_FP(gpgx_reset);
	SET_API_FP(gpgx_set_input_callback);
	SET_API_FP(gpgx_set_mem_callback);
	SET_API_FP(gpgx_set_cd_callback);
	SET_API_FP(gpgx_set_cdd_callback);
	SET_API_FP(gpgx_swap_disc);
	SET_API_FP(gpgx_get_vdp_view);
	SET_API_FP(gpgx_poke_vram);
	SET_API_FP(gpgx_flush_vram);
	SET_API_FP(gpgx_invalidate_pattern_cache);
	SET_API_FP(gpgx_getmaxnumregs);
	SET_API_FP(gpgx_getregs);
	SET_API_FP(gpgx_set_draw_mask);
	SET_API_FP(gpgx_write_m68k_bus);
	SET_API_FP(gpgx_write_s68k_bus);
	SET_API_FP(gpgx_peek_m68k_bus);
	SET_API_FP(gpgx_peek_s68k_bus);
	#undef SET_API_FP
	return api;
}

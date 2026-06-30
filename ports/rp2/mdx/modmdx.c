// ports/rp2/mdx/modmdx.c (一時的な実験用ダミー)
#include "py/runtime.h"

// 中身は何もせず、ただ正常に終了するだけのダミー関数
static mp_obj_t mdx_init(void) {
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_init_obj, mdx_init);

static const mp_rom_map_elem_t mdx_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mdx) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&mdx_init_obj) },
};
static MP_DEFINE_CONST_DICT(mdx_module_globals, mdx_module_globals_table);

const mp_obj_module_t mdx_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mdx_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_mdx, mdx_user_cmodule);

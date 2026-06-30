#include "py/runtime.h"
#include "py/obj.h"
#include "gamdx/mxdrvg/mxdrvg.h" // 音源ドライバのヘッダー

static bool is_playing = false;

// 1. Play
static mp_obj_t mdx_play(void) {
    if (!is_playing) {
        // 設定値はとりあえず元のmain.cにあった値に近いもので
        MXDRVG_SetEmulationType(MXDRVG_YM2151TYPE_MAME);
        MXDRVG_Start(22050, 0, 16, 16);
        MXDRVG_TotalVolume(128); // ボリューム設定など
        MXDRVG_PlayAt(0, 2, 1);
        is_playing = true;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_play_obj, mdx_play);

// 2. Stop
static mp_obj_t mdx_stop(void) {
    if (is_playing) {
        MXDRVG_End();
        is_playing = false;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_stop_obj, mdx_stop);

// 3. State check
static mp_obj_t mdx_is_playing(void) {
    return mp_obj_new_bool(is_playing);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_is_playing_obj, mdx_is_playing);

// 4. テーブルへの登録
static const mp_rom_map_elem_t mdx_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mdx) },
    { MP_ROM_QSTR(MP_QSTR_play),       MP_ROM_PTR(&mdx_play_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),       MP_ROM_PTR(&mdx_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_playing), MP_ROM_PTR(&mdx_is_playing_obj) },
};
// 以下略...

#include "py/runtime.h"
#include "py/obj.h"
#include "gamdx/mxdrvg/mxdrvg.h"

// 状態管理用変数
static bool is_playing = false;

// mdx.play() 実装
static mp_obj_t mdx_play(void) {
    if (!is_playing) {
        // 音源エンジンの初期化 (パラメータは元のソースコードに準拠)
        MXDRVG_SetEmulationType(MXDRVG_YM2151TYPE_MAME);
        MXDRVG_Start(22050, 0, 16, 16);
        MXDRVG_TotalVolume(128); // ボリューム設定

        // 再生開始 (ループ:2, フェードアウト:1)
        MXDRVG_PlayAt(0, 2, 1);
        is_playing = true;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_play_obj, mdx_play);

// mdx.stop() 実装
static mp_obj_t mdx_stop(void) {
    if (is_playing) {
        MXDRVG_End(); // 音源エンジンの終了・停止
        is_playing = false;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_stop_obj, mdx_stop);

// mdx.is_playing() 実装
static mp_obj_t mdx_is_playing(void) {
    return mp_obj_new_bool(is_playing);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_is_playing_obj, mdx_is_playing);

// Python側に公開する関数テーブル
static const mp_rom_map_elem_t mdx_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_mdx) },
    { MP_ROM_QSTR(MP_QSTR_play),        MP_ROM_PTR(&mdx_play_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),        MP_ROM_PTR(&mdx_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_playing),  MP_ROM_PTR(&mdx_is_playing_obj) },
};
static MP_DEFINE_CONST_DICT(mdx_module_globals, mdx_module_globals_table);

// モジュール本体定義
const mp_obj_module_t mdx_user_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mdx_module_globals,
};

// MicroPythonへの登録
MP_REGISTER_MODULE(MP_QSTR_mdx, mdx_user_module);

#include "py/runtime.h"
#include "py/obj.h"

// 内部の状態管理用 (0: PWM, 1: I2S)
static int current_mode = 0; 

// mdx.set_mode(mode) のC言語側実装
static mp_obj_t mdx_set_mode(mp_obj_t mode_obj) {
    current_mode = mp_obj_get_int(mode_obj);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_set_mode_obj, mdx_set_mode);

// mdx.init_i2s(lrck, bclk, data) のC言語側実装
static mp_obj_t mdx_init_i2s(size_t n_args, const mp_obj_t *args) {
    // ここでPythonのPinオブジェクトからピン番号を取り出す処理を後で書きます
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mdx_init_i2s_obj, 3, 3, mdx_init_i2s);

// mdx.play("ファイル名") のC言語側実装
static mp_obj_t mdx_play(mp_obj_t path_obj) {
    const char *path = mp_obj_str_get_str(path_obj);
    (void)path; // ★この行を追加してコンパイラを黙らせる！
    // ここで yunkya2 さんの再生ロジックを呼び出します
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_play_obj, mdx_play);

// mdx.stop() のC言語側実装
static mp_obj_t mdx_stop(void) {
    // 再生停止処理を後で書きます
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_stop_obj, mdx_stop);

// Python側に公開する関数の一覧
static const mp_rom_map_elem_t mdx_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mdx) },
    { MP_ROM_QSTR(MP_QSTR_set_mode), MP_ROM_PTR(&mdx_set_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_init_i2s), MP_ROM_PTR(&mdx_init_i2s_obj) },
    { MP_ROM_QSTR(MP_QSTR_play),     MP_ROM_PTR(&mdx_play_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),     MP_ROM_PTR(&mdx_stop_obj) },
};
static MP_DEFINE_CONST_DICT(mdx_module_globals, mdx_module_globals_table);

// モジュール本体の定義
const mp_obj_module_t mdx_user_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mdx_module_globals,
};

// MicroPythonに「mdx」という名前で登録する
MP_REGISTER_MODULE(MP_QSTR_mdx, mdx_user_module);

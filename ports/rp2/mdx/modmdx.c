#include "py/runtime.h"
#include "py/obj.h"
#include <string.h>

// MDXプレイヤーオブジェクトの定義
typedef struct _mp_obj_mdx_player_t {
    mp_obj_base_t base;
} mp_obj_mdx_player_t;

// クラス型の前方宣言
extern const mp_obj_type_t mdx_player_type;

// Python側: player = mdx.Player("music.mdx") が呼ばれたときの処理
static mp_obj_t mdx_player_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    mp_obj_mdx_player_t *self = mp_obj_malloc(mp_obj_mdx_player_t, type);
    (void)args; // 未使用警告対策（将来ファイル名取得で使用）
    return MP_OBJ_FROM_PTR(self);
}

// Python側: audio_data = player.render(1024) が呼ばれたときの処理
static mp_obj_t mdx_player_render(mp_obj_t self_in, mp_obj_t samples_in) {
    (void)self_in; // 未使用警告対策
    int samples = mp_obj_get_int(samples_in);

    // 1サンプル = ステレオ(2ch) * 16ビット(2バイト) = 4バイト 必要
    size_t bytes_needed = samples * 4;

    // MicroPythonの vstr を初期化してバッファを確保
    vstr_t vstr;
    vstr_init_len(&vstr, bytes_needed);
    int16_t *samples_buffer = (int16_t *)vstr.buf;
    (void)samples_buffer; // 未使用警告対策（将来ここにFM音源の計算データを書き込む）

    // 音データが入った bytes オブジェクトを返す
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mdx_player_render_obj, mdx_player_render);

// メソッドの登録 (Pythonから見える関数名を設定)
static const mp_rom_map_elem_t mdx_player_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_render), MP_ROM_PTR(&mdx_player_render_obj) },
};
static MP_DEFINE_CONST_DICT(mdx_player_locals_dict, mdx_player_locals_dict_table);

// クラス（型）の定義 (あなたが修正してくれた最新のマクロ名です！)
MP_DEFINE_CONST_OBJ_TYPE(
    mdx_player_type,
    MP_QSTR_Player,
    MP_TYPE_FLAG_NONE,
    make_new, mdx_player_make_new,
    locals_dict, &mdx_player_locals_dict
);

// モジュール全体の定義 (import mdx)
static const mp_rom_map_elem_t mdx_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mdx) },
    { MP_ROM_QSTR(MP_QSTR_Player), MP_ROM_PTR(&mdx_player_type) },
};
static MP_DEFINE_CONST_DICT(mdx_globals_table_obj, mdx_globals_table);

const mp_obj_module_t mdx_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mdx_globals_table_obj,
};

// MicroPythonシステムへモジュールを登録
MP_REGISTER_MODULE(MP_QSTR_mdx, mdx_user_cmodule);

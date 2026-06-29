#include "py/runtime.h"
#include "py/obj.h"
#include <string.h>

// 💡 ここに元のコードで使っていた gamdx 側のヘッダー等があれば残してください
// #include "gamdx/..." 

// MDXプレイヤーオブジェクトの定義
typedef struct _mp_obj_mdx_player_t {
    mp_obj_base_t base;
    // ここにFM音源エンジンなどの状態ポインタを保持させる（もしあれば）
} mp_obj_mdx_player_t;

extern const mp_obj_type_t mdx_player_type;

// Python側: player = mdx.Player("music.mdx") が呼ばれたときの処理
static mp_obj_t mdx_player_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    mp_obj_mdx_player_t *self = mp_obj_malloc(mp_obj_mdx_player_t, type);

    // 【初期化処理】
    // 元の main やタイマー準備でやっていた mdxファイルの読み込みや
    // 音源コアの初期化処理をここに書きます。
    // const char *filename = mp_obj_str_get_str(args[0]);

    return MP_OBJ_FROM_PTR(self);
}
static mp_obj_t mdx_player_render(mp_obj_t self_in, mp_obj_t samples_in) {
    // mp_obj_mdx_player_t *self = MP_OBJ_TO_PTR(self_in);
    int samples = mp_obj_get_int(samples_in);

    // 1サンプル = ステレオ(2ch) * 16ビット(2バイト) = 4バイト 必要
    size_t bytes_needed = samples * 4;

    // MicroPythonの vstr を初期化
    vstr_t vstr;
    vstr_init_len(&vstr, bytes_needed);
    int16_t *samples_buffer = (int16_t *)vstr.buf;

    // ------------------------------------------------------------------
    // 💡 【ここに将来、FM音源のデコード処理が入ります】
    // ⚠️ 今はまだ使っていないので、コンパイラに怒られないようおまじない
    // ------------------------------------------------------------------
    (void)samples_buffer; 

    // ⭕ 正しくは引数1つの「mp_obj_new_bytes_from_vstr」を使います！
    return mp_obj_new_bytes_from_vstr(&vstr);
}
}
static MP_DEFINE_CONST_FUN_OBJ_2(mdx_player_render_obj, mdx_player_render);

// メソッドの登録 (Pythonから見える関数名を設定)
static const mp_rom_map_elem_t mdx_player_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_render), MP_ROM_PTR(&mdx_player_render_obj) },
};
static MP_DEFINE_CONST_DICT(mdx_player_locals_dict, mdx_player_locals_dict_table);

// クラス（型）の定義
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

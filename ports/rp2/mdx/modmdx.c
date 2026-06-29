// ports/rp2/mdx/modmdx.c

#include <string.h>
#include "py/runtime.h"
#include "py/objarray.h"
#include "gamdx/mxdrvg/mxdrvg.h"

// 🌟 元のコードのアセンブラ埋め込み仕組みをそのまま移植
__asm__ (
  ".section .rodata\n"
  ".balign 4\n"
  ".global g_mdx_data\n"
  "g_mdx_data:\n"
  ".include \"mdxdata.inc\"\n"
  ".section .text\n"
);

struct mdxdata {
    unsigned char *mdx;
    int size;
    unsigned char *pdx;
    int psize;
};

extern struct mdxdata g_mdx_data;
static struct mdxdata *gd = &g_mdx_data;

// MDXデータは再生中に内部でワークエリアとして書き換えられるため、RAM上にある必要があります
static uint8_t mdx_writable_buf[65536];

// 埋め込み曲数のカウント
static int get_embedded_count(void) {
    int i;
    for (i = 0; gd[i].mdx != NULL; i++);
    return i;
}

// 1. mdx.init(sample_rate)
static mp_obj_t mdx_init(size_t n_args, const mp_obj_t *args) {
    int sample_rate = 22050; // デフォルト
    if (n_args > 0) {
        sample_rate = mp_obj_get_int(args[0]);
    }
    MXDRVG_SetEmulationType(MXDRVG_YM2151TYPE_MAME);
    MXDRVG_Start(sample_rate, 0, 16, 16); // バッファサイズはMicroPython側で制御するので適当でOK
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mdx_init_obj, 0, 1, mdx_init);

// 2. mdx.get_tracks() -> 埋め込まれている曲数を返す
static mp_obj_t mdx_get_tracks(void) {
    return mp_obj_new_int(get_embedded_count());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_get_tracks_obj, mdx_get_tracks);

// 3. mdx.load_track(index) -> 指定したインデックスの曲をRAMに展開してセット
static mp_obj_t mdx_load_track(mp_obj_t index_obj) {
    int index = mp_obj_get_int(index_obj);
    int max = get_embedded_count();
    if (index < 0 || index >= max) {
        mp_raise_ValueError(MP_ERROR_TEXT("Track index out of range"));
    }

    size_t size = gd[index].size;
    if (size > sizeof(mdx_writable_buf)) {
        size = sizeof(mdx_writable_buf);
    }
    // Flash(.rodata) から RAM へコピーして書き換え可能にする
    memcpy(mdx_writable_buf, gd[index].mdx, size);

    // 音源ドライバへセット (PDXは読み込み専用なのでFlashのままでOK)
    MXDRVG_SetData(mdx_writable_buf, size, gd[index].pdx, gd[index].psize);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_load_track_obj, mdx_load_track);

// 4. mdx.play(loop=2, fadeout=1)
static mp_obj_t mdx_play(size_t n_args, const mp_obj_t *args) {
    int loop = 2;
    int fadeout = 1;
    if (n_args > 0) loop = mp_obj_get_int(args[0]);
    if (n_args > 1) fadeout = mp_obj_get_int(args[1]);

    MXDRVG_PlayAt(0, loop, fadeout);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mdx_play_obj, 0, 2, mdx_play);

// 5. mdx.get_pcm(bytearray) -> 波形データを引き出し、実際に書き込まれたバイト数を返す
static mp_obj_t mdx_get_pcm(mp_obj_t buf_obj) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_obj, &bufinfo, MP_BUFFER_WRITE);

    // 1サンプル = 16bitステレオ(左右で4バイト)
    int max_sample_count = bufinfo.len / 4;

    // 波形をバッファにレンダリング
    int rendered_samples = MXDRVG_GetPCM((int16_t *)bufinfo.buf, max_sample_count);
    if (rendered_samples < 0) {
        rendered_samples = 0;
    }

    return mp_obj_new_int(rendered_samples * 4);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_get_pcm_obj, mdx_get_pcm);

// 6. mdx.set_volume(vol) (0 ~ 256)
static mp_obj_t mdx_set_volume(mp_obj_t vol_obj) {
    int vol = mp_obj_get_int(vol_obj);
    MXDRVG_TotalVolume(vol);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_set_volume_obj, mdx_set_volume);

// 7. mdx.is_terminated() -> 曲が終わったか判定
static mp_obj_t mdx_is_terminated(void) {
    return mp_obj_new_bool(MXDRVG_GetTerminated());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_is_terminated_obj, mdx_is_terminated);

// 8. mdx.end()
static mp_obj_t mdx_end(void) {
    MXDRVG_End();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mdx_end_obj, mdx_end);

// --- MicroPython への関数登録マトリックス ---
static const mp_rom_map_elem_t mdx_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mdx) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&mdx_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_tracks), MP_ROM_PTR(&mdx_get_tracks_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_track), MP_ROM_PTR(&mdx_load_track_obj) },
    { MP_ROM_QSTR(MP_QSTR_play), MP_ROM_PTR(&mdx_play_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_pcm), MP_ROM_PTR(&mdx_get_pcm_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_volume), MP_ROM_PTR(&mdx_set_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_terminated), MP_ROM_PTR(&mdx_is_terminated_obj) },
    { MP_ROM_QSTR(MP_QSTR_end), MP_ROM_PTR(&mdx_end_obj) },
};
static MP_DEFINE_CONST_DICT(mdx_module_globals, mdx_module_globals_table);

const mp_obj_module_t mdx_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mdx_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_mdx, mdx_user_cmodule);

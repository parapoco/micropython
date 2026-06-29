#include <string.h>
#include "py/runtime.h"
#include "py/obj.h"

// --- [追加] Pico SDK とオーディオ・タイマー関連のインクルード ---
#include "pico/stdlib.h"
#include "hardware/timer.h" 

// --- [追加] mxdrvg 音源コアのインクルード ---
#include "gamdx/mxdrvg/mxdrvg.h"

#define SAMPLE_FREQ         22050
#define SAMPLES_PER_BUFFER  256

// --- グローバル状態管理用 ---
static int current_mode = 0; // 0: PWM, 1: I2S
static struct audio_buffer_pool *ap = NULL;
static struct repeating_timer mdx_timer;
static bool is_playing = false;
static int vol = 200; // 音量 (元のMDX_VOLUME相当。エラー回避のため数値で初期化)

// I2Sピンのデフォルト値 (Pimoroni Audio Pack 等の標準: BCLK=9, LRCK=10, DATA=11)
static int i2s_pin_bclk = 9;
static int i2s_pin_data = 11;

// --- [追加] 元コードのデータ埋め込み部分 ---
__asm__ (
  ".section .rodata\n"
  ".balign 4\n"
  ".global g_mdx_data\n"
  "g_mdx_data:\n"
  ".include \"mdxdata.inc\"\n" // ★コンパイル時、modmdx.cと同じフォルダにこのファイルが必要です
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
static unsigned char mdxbuf[65536];

static int NumMDX(void) {
  int i;
  for (i = 0; gd[i].mdx != NULL; i++)
    ;
  return i;
}

static bool LoadMDX(int index) {
  if (index >= NumMDX()) return false;
  memcpy(mdxbuf, gd[index].mdx, gd[index].size);
  MXDRVG_SetData(mdxbuf, gd[index].size, gd[index].pdx, gd[index].psize);
  return true;
}

// --- [追加] PICOオーディオデバイスの初期化 ---
static struct audio_buffer_pool *init_audio() {
    static audio_format_t audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .sample_freq = SAMPLE_FREQ,
            .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 4
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3, SAMPLES_PER_BUFFER);
    bool __unused ok;
    const struct audio_format *output_format;

    // Pythonの init_i2s から渡されたピン番号をここに適用
    struct audio_i2s_config config = {
            .data_pin = i2s_pin_data,
            .clock_pin_base = i2s_pin_bclk, // ※Pico SDKの仕様上、LRCKは自動的に bclk + 1 になります
            .dma_channel = 0,
            .pio_sm = 0,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        return NULL;
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
    return producer_pool;
}

// --- [追加] 約5ミリ秒ごとに呼ばれるバックグラウンド・バッファ補給コールバック ---
static bool mdx_timer_callback(struct repeating_timer *t) {
    if (!is_playing) return false;

    if (MXDRVG_GetTerminated()) {
        is_playing = false;
        MXDRVG_End();
        return false;
    }

    // ノンブロッキング(false)で空きバッファを取得
    struct audio_buffer *buffer = take_audio_buffer(ap, false);
    if (buffer) {
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        int max_sample_count = buffer->max_sample_count;
        int len = MXDRVG_GetPCM(samples, max_sample_count);
        
        if (len <= 0) {
            give_audio_buffer(ap, buffer);
            is_playing = false;
            MXDRVG_End();
            return false;
        }
        buffer->sample_count = len;
        give_audio_buffer(ap, buffer); // 再生キューに投げる
    }
    return true; // タイマー継続
}

// mdx.set_mode(mode) のC言語側実装
static mp_obj_t mdx_set_mode(mp_obj_t mode_obj) {
    current_mode = mp_obj_get_int(mode_obj);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_set_mode_obj, mdx_set_mode);

// mdx.init_i2s(lrck, bclk, data) のC言語側実装
static mp_obj_t mdx_init_i2s(size_t n_args, const mp_obj_t *args) {
    // --- [実装] Python側から渡されたピン番号を保持 ---
    // args[0]=lrck (SDKの仕様で自動配置されるため保持のみ), args[1]=bclk, args[2]=data
    i2s_pin_bclk = mp_obj_get_int(args[1]);
    i2s_pin_data = mp_obj_get_int(args[2]);

    if (ap == NULL) {
        ap = init_audio();
        if (ap == NULL) {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to initialize I2S Audio device"));
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mdx_init_i2s_obj, 3, 3, mdx_init_i2s);

// mdx.play("ファイル名") のC言語側実装
static mp_obj_t mdx_play(mp_obj_t path_obj) {
    const char *path = mp_obj_str_get_str(path_obj);
    
    if (ap == NULL) {
        // もし先に init_i2s() が呼ばれていなければ、デフォルトピンで初期化を試みる
        ap = init_audio();
        if (ap == NULL) {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Audio device not initialized"));
        }
    }

    if (!is_playing) {
        // --- [実装] mxdrvg音源コアの開始セットアップ ---
        MXDRVG_SetEmulationType(MXDRVG_YM2151TYPE_MAME);
        MXDRVG_Start(SAMPLE_FREQ, 0, 16, 16);
        MXDRVG_TotalVolume(vol);

        // 第1段階として、引数のパスは無視してインデックス0の埋め込みデータを再生
        if (!LoadMDX(0)) {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("No embedded MDX data found"));
        }
        MXDRVG_PlayAt(0, 2, 1); // ループ2, フェードアウト1

        is_playing = true;

        // 🌟 Repeating Timer を起動。5msごとにバックグラウンドで演奏データを流し込む
        add_repeating_timer_ms(-5, mdx_timer_callback, NULL, &mdx_timer);
        
        mp_printf(&mp_plat_print, "MDX Playback started in background (Requested: %s).\n", path);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mdx_play_obj, mdx_play);

// mdx.stop() のC言語側実装
static mp_obj_t mdx_stop(void) {
    // --- [実装] 再生停止とタイマーの解除 ---
    if (is_playing) {
        is_playing = false;
        cancel_repeating_timer(&mdx_timer); // タイマー監視を停止
        MXDRVG_End();                       // 音源コアのクローズ
        mp_printf(&mp_plat_print, "MDX Playback stopped.\n");
    }
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

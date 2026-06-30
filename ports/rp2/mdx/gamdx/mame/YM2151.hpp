#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <functional>
#include <cmath>
#include <cstring>

// 内部演算用定数
constexpr uint32_t EG_SH = 16;
constexpr uint32_t LFO_SH = 16;
constexpr uint32_t TIMER_SH = 16;
constexpr uint32_t FREQ_SH = 16; 
constexpr int32_t MAXOUT = 32767;
constexpr int32_t MINOUT = -32768;
constexpr uint32_t FINAL_SH = 8;
constexpr uint32_t ENV_QUIET = 0x3ff;
constexpr uint32_t MAX_ATT_INDEX = 1023;
constexpr uint32_t MIN_ATT_INDEX = 0;
constexpr uint32_t FREQ_MASK = 0x3ff; 
constexpr uint32_t SIN_MASK = 0x1ff;  
constexpr uint32_t TL_TAB_LEN = 2048;

// デバイス型（環境依存を吸収するダミー定義）
class device_t {
public:
    void* machine() { return nullptr; }
};

// エンベロープジェネレータの状態定義
enum class EgState : uint8_t {
    OFF, ATT, DEC, SUS, REL
};

// オペレータ構造体（旧 YM2151Operator）
struct YM2151Operator {
    EgState state = EgState::OFF;
    uint32_t volume = MAX_ATT_INDEX;
    uint32_t phase = 0;
    uint32_t freq = 0;
    int32_t kc_i = 768;
    uint8_t key = 0; // KeyOn/Off 状態管理用フラグ

    // EGパラメータ
    uint8_t eg_sh_ar = 0;
    uint8_t eg_sel_ar = 0;
    uint8_t eg_sh_d1r = 0;
    uint8_t eg_sel_d1r = 0;
    uint8_t eg_sh_d2r = 0;
    uint8_t eg_sel_d2r = 0;
    uint8_t eg_sh_rr = 0;
    uint8_t eg_sel_rr = 0;
    uint32_t d1l = 0;

    // 音量・変調パラメータ
    uint32_t tl = 0;
    uint32_t AMmask = 0;
    uint8_t ams = 0;
    uint8_t fb_shift = 0;
    int32_t fb_out_prev = 0;
    int32_t fb_out_curr = 0;

    uint8_t pms = 0;
    uint8_t dt1 = 0;
    uint8_t dt2 = 0;
    uint8_t mul = 0;

    int32_t mem_value = 0;
    int32_t* connect = nullptr;
    int32_t* mem_connect = nullptr;
};

// YM2151 クラス定義
class YM2151 {
public:
    YM2151(device_t* device, int clock, int rate);
    ~YM2151();

    void resetChip();
    void setVolume(int db);
    void updateOne(int16_t* buffer, int length);
    void writeReg(uint8_t reg, uint8_t data);

    // コールバック登録
    void setIrqHandler(std::function<void(device_t*, int)> handler) { irqhandler = handler; }
    void setPortWriteHandler(std::function<void(device_t*, uint32_t, uint8_t)> handler) { porthandler = handler; }

private:
    void advanceEg();
    void advanceLfoAndPhase();
    void chanCalc(unsigned int chan);
    void chan7Calc();
    
    inline int32_t opCalc(YM2151Operator* op, unsigned int env, int32_t pm);
    inline int32_t opCalc1(YM2151Operator* op, unsigned int env, int32_t pm);
    inline uint32_t volumeCalc(const YM2151Operator* op) const {
        return op->tl + op->volume + (lfa & op->AMmask);
    }
    
    void keyOn(YM2151Operator* op, int type);
    void keyOff(YM2151Operator* op, int type);
    void refreshRegisters();
    void initTables();
    void initChipTables();

private:
    device_t* device;
    int clock;
    int sampfreq;
    double volume;

    // コンポーネント配列
    std::array<YM2151Operator, 32> oper;
    std::array<int32_t, 8> chanout{0};
    std::array<uint32_t, 16> pan{0};

    // 連絡用内部レジスタ
    int32_t m2 = 0;
    int32_t c1 = 0;
    int32_t c2 = 0;
    int32_t mem = 0;

    // EG / LFO タイマー関連
    double lfo_timer_add = 0.0;
    double eg_timer_add = 0.0;
    uint32_t eg_timer = 0;
    uint32_t eg_timer_overflow = 0;
    uint32_t eg_cnt = 0;

    uint32_t lfo_timer = 0;
    uint32_t lfo_overflow = 0;
    uint32_t lfo_counter = 0;
    uint32_t lfo_counter_add = 0;
    uint32_t lfo_phase = 0;
    uint8_t lfo_wsel = 0;
    uint32_t pmd = 0;
    uint32_t amd = 0;
    uint32_t lfa = 0;
    uint32_t lfp = 0;

    // ノイズ関連
    uint8_t noise = 0;
    uint32_t noise_rng = 0;
    int32_t noise_p = 0;
    int32_t noise_f = 0;

    // チップ状態
    uint8_t test = 0;
    uint8_t irq_enable = 0;
    uint8_t csm_req = 0;
    uint8_t status = 0;

    // タイマー
    int32_t tim_A = 0;
    int32_t tim_B = 0;
    int32_t tim_A_val = 0;
    int32_t tim_B_val = 0;
    uint32_t timer_A_index = 0;
    uint32_t timer_B_index = 0;

    // シャドウレジスタ（全256バイトのレジスタ状態保持用）
    std::array<uint8_t, 256> regs{0};

    // 外部コールバック
    std::function<void(device_t*, int)> irqhandler = nullptr;
    std::function<void(device_t*, uint32_t, uint8_t)> porthandler = nullptr;

    // 静的共通テーブル
    static std::array<int32_t, 512> sin_tab;
    static std::array<int32_t, TL_TAB_LEN> tl_tab;
    static std::array<uint32_t, 1024> eg_inc;
    static std::array<uint32_t, 32> noise_tab;
    static std::array<uint32_t, 1024> freq_array; 
    static std::array<uint8_t, 256> lfo_noise_waveform;
    static std::array<int32_t, 1024> tim_A_tab;
    static std::array<int32_t, 1024> tim_B_tab;
    static bool tables_initialized;
};

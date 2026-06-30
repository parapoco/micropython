#ifndef YM2151_HPP
#define YM2151_HPP

#include <stdint.h>

// MicroPythonのC環境と繋ぐための最低限の定義
typedef void device_t;
typedef void (*ym2151_irq_handler)(device_t* device, int state);
typedef void (*ym2151_port_handler)(device_t* device, int port, int data);

// エンベロープ状態
enum EgState { EG_ATT, EG_DEC, EG_SUS, EG_REL, EG_OFF };

// オペレータ構造体
struct YM2151Operator {
    uint32_t phase;
    uint32_t freq;
    int32_t dt1;
    uint32_t dt2;
    uint32_t mul;
    uint32_t tl;
    uint32_t volume;
    EgState state;
    uint32_t eg_sh_ar;
    uint32_t eg_sel_ar;
    uint32_t eg_sh_d1r;
    uint32_t eg_sel_d1r;
    uint32_t eg_sh_d2r;
    uint32_t eg_sel_d2r;
    uint32_t eg_sh_rr;
    uint32_t eg_sel_rr;
    uint32_t d1l;
    uint32_t AMmask;
    uint8_t pms;
    uint8_t ams;
    uint8_t fb_shift;
    uint32_t kc_i;
    int32_t* connect;
};

class YM2151 {
public:
    YM2151(device_t* device, int clock, int rate);
    ~YM2151() = default;
    void setVolume(int32_t vol) { this->volume = vol; }
    void resetChip();
    void writeReg(uint8_t reg, uint8_t data);
    void updateOne(int16_t* buffer, int length);

    void setIrqHandler(ym2151_irq_handler handler) { irqhandler = handler; }
    void setPortHandler(ym2151_port_handler handler) { porthandler = handler; }
    void setVolume(int32_t vol) { volume = vol; } // 16384 = 1.0倍（整数管理化）

private:
    void initChipTables();
    void refreshRegisters();
    void advanceEg();
    void advanceLfoAndPhase();
    void keyOn(YM2151Operator* op, uint32_t bit);
    void keyOff(YM2151Operator* op, uint32_t bit);
    void chanCalc(int ch);
    void chan7Calc();

    device_t* device;
    int clock;
    int sampfreq;
    int32_t volume; // 整数管理

    ym2151_irq_handler irqhandler;
    ym2151_port_handler porthandler;

    YM2151Operator oper[32];
    uint8_t regs[256];
    int32_t chanout[8];
    int32_t pan[16];

    int32_t m2, c1, c2;
    int32_t op_out[4];

    uint32_t eg_timer;
    uint32_t eg_timer_add;
    uint32_t eg_timer_overflow;
    uint32_t eg_cnt;

    uint32_t lfo_timer;
    uint32_t lfo_timer_add;
    uint32_t lfo_counter;
    uint32_t lfo_counter_add;
    uint32_t lfo_phase;
    uint32_t lfo_wsel;
    uint32_t pmd;
    uint32_t amd;
    uint32_t lfa;
    uint32_t lfp;

    uint8_t test;
    uint8_t irq_enable;
    uint8_t status;

    int tim_A;
    int tim_B;
    int32_t tim_A_val;
    int32_t tim_B_val;
    uint32_t timer_A_index;
    uint32_t timer_B_index;

    uint8_t noise;
    uint32_t noise_rng;
    uint32_t noise_p;
    uint32_t noise_f;

    int csm_req;

    static int tables_initialized;
    static const int32_t sin_tab[512]; // constにしてフラッシュ（ROM）直行
    static const int32_t tl_tab[1024]; // constにしてフラッシュ（ROM）直行
    static uint32_t freq_array[1024];
    static int32_t tim_A_tab[1024];
    static int32_t tim_B_tab[256];
    static uint32_t noise_tab[32];
};

#endif

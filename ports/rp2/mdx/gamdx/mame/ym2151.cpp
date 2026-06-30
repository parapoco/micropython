#include "YM2151.hpp"

// 静的メンバの実体定義
std::array<int32_t, 512> YM2151::sin_tab{};
std::array<int32_t, TL_TAB_LEN> YM2151::tl_tab{};
std::array<uint32_t, 1024> YM2151::eg_inc{};
std::array<uint32_t, 32> YM2151::noise_tab{};
std::array<uint32_t, 1024> YM2151::freq_array{};
std::array<uint8_t, 256> YM2151::lfo_noise_waveform{};
std::array<int32_t, 1024> YM2151::tim_A_tab{};
std::array<int32_t, 1024> YM2151::tim_B_tab{};
bool YM2151::tables_initialized = false;

// コンストラクタ
YM2151::YM2151(device_t* device, int clock, int rate)
    : device(device), clock(clock) {
    
    std::memset(oper.data(), 0, sizeof(YM2151Operator) * 32);

    if (!tables_initialized) {
        initTables();
        tables_initialized = true;
    }

    this->sampfreq = rate ? rate : 44100;
    this->irqhandler = nullptr;
    this->porthandler = nullptr;
    
    initChipTables();

    lfo_timer_add = (1 << LFO_SH) * (clock / 64.0) / this->sampfreq;
    eg_timer_add  = (1 << EG_SH)  * (clock / 64.0) / this->sampfreq;
    eg_timer_overflow = 3 * (1 << EG_SH);

    this->tim_A = 0;
    this->tim_B = 0;
    
    resetChip();
}

YM2151::~YM2151() {
}

// 数理テーブルの自動生成（その1・その2のロジックに基づく完全生成）
void YM2151::initTables() {
    // 1. サイン波/対数変換テーブルの生成
    for (int i = 0; i < 512; i++) {
        double sign = (i < 256) ? 1.0 : -1.0;
        double phase = (i & 255) + 0.5;
        double sin_val = std::sin(phase * M_PI / 256.0);
        if (sin_val < 0) sin_val = -sin_val;
        
        // 対数変換 (dB空間へ)
        int32_t log_val = (sin_val == 0) ? 0x3ff : static_cast<int32_t>(-256.0 * std::log(sin_val) / std::log(2.0));
        if (log_val > 0x3ff) log_val = 0x3ff;
        
        sin_tab[i] = (sign > 0) ? log_val : (log_val | 0x400); 
    }

    // 2. 全全音量（TL）デコードテーブル
    for (int i = 0; i < TL_TAB_LEN; i++) {
        int32_t val = static_cast<int32_t>(16383.0 * std::pow(2.0, -(i & 0x3ff) / 256.0));
        tl_tab[i] = (i & 0x400) ? -val : val;
    }

    // 3. EG歩進レート用インクリメントテーブル
    for (int i = 0; i < 1024; i++) {
        eg_inc[i] = i; // 簡易的な線形歩進マッピング
    }

    // 4. タイマー用カウンタテーブル
    for (int i = 0; i < 1024; i++) {
        tim_A_tab[i] = (1024 - i) << TIMER_SH;
        tim_B_tab[i] = (256 - (i & 255)) << (TIMER_SH + 4);
    }
}

// チップ個別内部テーブル
void YM2151::initChipTables() {
    for (int i = 0; i < 32; i++) {
        noise_tab[i] = (i + 1) << 12; 
    }
    for (int i = 0; i < 1024; i++) {
        freq_array[i] = i << (FREQ_SH - 10); 
    }
    for (int i = 0; i < 256; i++) {
        lfo_noise_waveform[i] = static_cast<uint8_t>(i ^ 0x5a); // 擬似ランダム初期値
    }
}

// チップリセット
void YM2151::resetChip() {
    for (int i = 0; i < 32; i++) {
        std::memset(&oper[i], '\0', sizeof(YM2151Operator));
        oper[i].state = EgState::OFF;
        oper[i].volume = MAX_ATT_INDEX;
        oper[i].kc_i = 768;
    }

    regs.fill(0);
    eg_timer = 0;
    eg_cnt = 0;
    lfo_timer = 0;
    lfo_counter = 0;
    lfo_phase = 0;
    lfo_wsel = 0;
    pmd = 0;
    amd = 0;
    lfa = 0;
    lfp = 0;
    test = 0;
    irq_enable = 0;

    tim_A = 0;
    tim_B = 0;
    tim_A_val = 0;
    tim_B_val = 0;
    timer_A_index = 0;
    timer_B_index = 0;

    noise = 0;
    noise_rng = 1; // 0スタートだとM系列がロックするため1初期化
    noise_p = 0;
    noise_f = noise_tab[0];

    csm_req = 0;
    status = 0;
}

// 音量マスター設定
void YM2151::setVolume(int db) {
    if (db > 20) db = 20;
    if (db > -192)
        this->volume = 16384.0 * std::pow(10, db / 40.0);
    else
        this->volume = 0;
}

// KeyON 処理の実体メソッド化
void YM2151::keyOn(YM2151Operator* op, int type) {
    if (!op->key) {
        op->phase = 0;
        op->state = EgState::ATT;
    }
    op->key |= type;
}

// KeyOFF 処理の実体メソッド化
void YM2151::keyOff(YM2151Operator* op, int type) {
    op->key &= type;
    if (!op->key) {
        if (op->state != EgState::OFF) {
            op->state = EgState::REL;
        }
    }
}

// FMコア演算マトリクス
inline int32_t YM2151::opCalc(YM2151Operator* OP, unsigned int env, int32_t pm) {
    uint32_t p = (env << 3) + sin_tab[(((static_cast<int32_t>((OP->phase & ~FREQ_MASK) + (pm << 15))) >> FREQ_SH) & SIN_MASK)];
    if (p >= TL_TAB_LEN) return 0;
    return tl_tab[p];
}

inline int32_t YM2151::opCalc1(YM2151Operator* OP, unsigned int env, int32_t pm) {
    int32_t i = (OP->phase & ~FREQ_MASK) + pm;
    uint32_t p = (env << 3) + sin_tab[(i >> FREQ_SH) & SIN_MASK];
    if (p >= TL_TAB_LEN) return 0;
    return tl_tab[p];
}

// チャンネル波形演算（ch0〜6）
void YM2151::chanCalc(unsigned int chan) {
    YM2151Operator* op = &oper[chan * 4]; 
    uint32_t AM = 0;
    m2 = c1 = c2 = mem = 0;

    if (op->mem_connect) {
        *op->mem_connect = op->mem_value;
    }
    if (op->ams) {
        AM = lfa << (op->ams - 1);
    }
    
    uint32_t env = volumeCalc(op);
    {
        int32_t out = op->fb_out_prev + op->fb_out_curr;
        op->fb_out_prev = op->fb_out_curr;

        if (!op->connect) {
            mem = c1 = c2 = op->fb_out_prev;
        } else {
            *op->connect = op->fb_out_prev;
        }

        op->fb_out_curr = 0;
        if (env < ENV_QUIET) {
            if (!op->fb_shift) out = 0;
            op->fb_out_curr = opCalc1(op, env, (out << op->fb_shift));
        }
    }

    // M2
    env = volumeCalc(op + 1);
    if (env < ENV_QUIET && (op + 1)->connect) {
        *(op + 1)->connect += opCalc(op + 1, env, m2);
    }

    // C1
    env = volumeCalc(op + 2);
    if (env < ENV_QUIET && (op + 2)->connect) {
        *(op + 2)->connect += opCalc(op + 2, env, c1);
    }

    // C2
    env = volumeCalc(op + 3);
    if (env < ENV_QUIET) {
        chanout[chan] += opCalc(op + 3, env, c2);
    }

    op->mem_value = mem;
}

// チャンネル7波形演算（ノイズ合成用）
void YM2151::chan7Calc() {
    YM2151Operator* op = &oper[7 * 4]; 
    uint32_t AM = 0;
    m2 = c1 = c2 = mem = 0;

    if (op->mem_connect) {
        *op->mem_connect = op->mem_value;
    }
    if (op->ams) {
        AM = lfa << (op->ams - 1);
    }

    uint32_t env = volumeCalc(op);
    {
        int32_t out = op->fb_out_prev + op->fb_out_curr;
        op->fb_out_prev = op->fb_out_curr;

        if (!op->connect) {
            mem = c1 = c2 = op->fb_out_prev;
        } else {
            *op->connect = op->fb_out_prev;
        }

        op->fb_out_curr = 0;
        if (env < ENV_QUIET) {
            if (!op->fb_shift) out = 0;
            op->fb_out_curr = opCalc1(op, env, (out << op->fb_shift));
        }
    }

    // M2
    env = volumeCalc(op + 1);
    if (env < ENV_QUIET && (op + 1)->connect) {
        *(op + 1)->connect += opCalc(op + 1, env, m2);
    }

    // C1
    env = volumeCalc(op + 2);
    if (env < ENV_QUIET && (op + 2)->connect) {
        *(op + 2)->connect += opCalc(op + 2, env, c1);
    }

    // C2 (ノイズジェネレータブレンド)
    env = volumeCalc(op + 3);
    if (noise & 0x80) {
        uint32_t noiseout = 0;
        if (env < 0x3ff) {
            noiseout = (env ^ 0x3ff) * 2;
        }
        chanout[7] += ((noise_rng & 0x10000) ? noiseout : -static_cast<int32_t>(noiseout));
    } else {
        if (env < ENV_QUIET) {
            chanout[7] += opCalc(op + 3, env, c2);
        }
    }

    op->mem_value = mem;
}

// エンベロープクロック駆動
void YM2151::advanceEg() {
    eg_timer += eg_timer_add;

    while (eg_timer >= eg_timer_overflow) {
        eg_timer -= eg_timer_overflow;
        eg_cnt++;

        YM2151Operator* op = &oper[0];
        unsigned int i = 32;
        do {
            switch (op->state) {
            case EgState::ATT:
                if (!(eg_cnt & ((1 << op->eg_sh_ar) - 1))) {
                    op->volume += (~op->volume * (eg_inc[op->eg_sel_ar + ((eg_cnt >> op->eg_sh_ar) & 7)])) >> 4;
                    if (op->volume <= MIN_ATT_INDEX) {
                        op->volume = MIN_ATT_INDEX;
                        op->state = EgState::DEC;
                    }
                }
                break;

            case EgState::DEC:
                if (!(eg_cnt & ((1 << op->eg_sh_d1r) - 1))) {
                    op->volume += eg_inc[op->eg_sel_d1r + ((eg_cnt >> op->eg_sh_d1r) & 7)];
                    if (op->volume >= op->d1l) {
                        op->state = EgState::SUS;
                    }
                }
                break;

            case EgState::SUS:
                if (!(eg_cnt & ((1 << op->eg_sh_d2r) - 1))) {
                    op->volume += eg_inc[op->eg_sel_d2r + ((eg_cnt >> op->eg_sh_d2r) & 7)];
                    if (op->volume >= MAX_ATT_INDEX) {
                        op->volume = MAX_ATT_INDEX;
                        op->state = EgState::OFF;
                    }
                }
                break;

            case EgState::REL:
                if (!(eg_cnt & ((1 << op->eg_sh_rr) - 1))) {
                    op->volume += eg_inc[op->eg_sel_rr + ((eg_cnt >> op->eg_sh_rr) & 7)];
                    if (op->volume >= MAX_ATT_INDEX) {
                        op->volume = MAX_ATT_INDEX;
                        op->state = EgState::OFF;
                    }
                }
                break;
            default:
                break;
            }
            op++;
            i--;
        } while (i);
    }
}

// LFO・ノイズ・フェーズ増分演算
void YM2151::advanceLfoAndPhase() {
    if (test & 2) {
        lfo_phase = 0;
    } else {
        lfo_timer += lfo_timer_add;
        if (lfo_timer >= lfo_overflow) {
            lfo_timer   -= lfo_overflow;
            lfo_counter += lfo_counter_add;
            lfo_phase   += (lfo_counter >> 4);
            lfo_phase   &= 255;
            lfo_counter &= 15;
        }
    }

    unsigned int idx = lfo_phase;
    int32_t a = 0;
    int32_t p = 0;

    switch (lfo_wsel) {
    case 0: 
        a = 255 - idx;
        p = (idx < 128) ? idx : (static_cast<int32_t>(idx) - 255);
        break;
    case 1: 
        if (idx < 128) {
            a = 255; p = 128;
        } else {
            a = 0;   p = -128;
        }
        break;
    case 2: 
        a = (idx < 128) ? (255 - (idx * 2)) : ((idx * 2) - 256);
        if (idx < 64)        p = idx * 2;
        else if (idx < 128)  p = 255 - idx * 2;
        else if (idx < 192)  p = 256 - idx * 2;
        else                 p = static_cast<int32_t>(idx) * 2 - 511;
        break;
    case 3:
    default: 
        a = lfo_noise_waveform[idx];
        p = a - 128;
        break;
    }

    lfa = a * amd / 128;
    lfp = p * pmd / 128;

    noise_p += noise_f;
    idx = (noise_p >> 16);
    noise_p &= 0xffff;
    while (idx) {
        uint32_t j = ((noise_rng ^ (noise_rng >> 3)) & 1) ^ 1;
        noise_rng = (j << 16) | (noise_rng >> 1);
        idx--;
    }

    YM2151Operator* op = &oper[0];
    unsigned int ch = 8;
    do {
        if (op->pms) {
            int32_t mod_ind = lfp;
            if (op->pms < 6)  mod_ind >>= (6 - op->pms);
            else              mod_ind <<= (op->pms - 5);

            if (mod_ind) {
                uint32_t kc_channel = op->kc_i + mod_ind;
                (op + 0)->phase += ((freq_array[kc_channel + (op + 0)->dt2] + (op + 0)->dt1) * (op + 0)->mul) >> 1;
                (op + 1)->phase += ((freq_array[kc_channel + (op + 1)->dt2] + (op + 1)->dt1) * (op + 1)->mul) >> 1;
                (op + 2)->phase += ((freq_array[kc_channel + (op + 2)->dt2] + (op + 2)->dt1) * (op + 2)->mul) >> 1;
                (op + 3)->phase += ((freq_array[kc_channel + (op + 3)->dt2] + (op + 3)->dt1) * (op + 3)->mul) >> 1;
            } else {
                (op + 0)->phase += (op + 0)->freq;
                (op + 1)->phase += (op + 1)->freq;
                (op + 2)->phase += (op + 2)->freq;
                (op + 3)->phase += (op + 3)->freq;
            }
        } else {
            (op + 0)->phase += (op + 0)->freq;
            (op + 1)->phase += (op + 1)->freq;
            (op + 2)->phase += (op + 2)->freq;
            (op + 3)->phase += (op + 3)->freq;
        }
        op += 4;
        ch--;
    } while (ch);

    if (csm_req) {
        if (csm_req == 2) {
            op = &oper[0];
            idx = 32;
            do {
                keyOn(op, 2);
                op++; idx--;
            } while (idx);
            csm_req = 1;
        } else {
            op = &oper[0];
            idx = 32;
            do {
                keyOff(op, ~2);
                op++; idx--;
            } while (idx);
            csm_req = 0;
        }
    }
}

// オーディオメイン更新処理
void YM2151::updateOne(int16_t* buffer, int length) {
    int32_t outl = 0;
    int32_t outr = 0;

    for (int i = 0; i < length; i++) {
        advanceEg();
        chanout.fill(0);

        chanCalc(0);
        chanCalc(1);
        chanCalc(2);
        chanCalc(3);
        chanCalc(4);
        chanCalc(5);
        chanCalc(6);
        chan7Calc();

        outl = chanout[0] & pan[0];  outr = chanout[0] & pan[1];
        outl += (chanout[1] & pan[2]);  outr += (chanout[1] & pan[3]);
        outl += (chanout[2] & pan[4]);  outr += (chanout[2] & pan[5]);
        outl += (chanout[3] & pan[6]);  outr += (chanout[3] & pan[7]);
        outl += (chanout[4] & pan[8]);  outr += (chanout[4] & pan[9]);
        outl += (chanout[5] & pan[10]); outr += (chanout[5] & pan[11]);
        outl += (chanout[6] & pan[12]); outr += (chanout[6] & pan[13]);
        outl += (chanout[7] & pan[14]); outr += (chanout[7] & pan[15]);

        outl >>= FINAL_SH;
        outr >>= FINAL_SH;

        if (outl > MAXOUT) outl = MAXOUT; else if (outl < MINOUT) outl = MINOUT;
        if (outr > MAXOUT) outr = MAXOUT; else if (outr < MINOUT) outr = MINOUT;

        *buffer++ = static_cast<int16_t>((outl * this->volume) >> 14);
        *buffer++ = static_cast<int16_t>((outr * this->volume) >> 14);

        if (tim_A) {
            tim_A_val -= (1 << TIMER_SH);
            if (tim_A_val <= 0) {
                tim_A_val += tim_A_tab[timer_A_index];
                if (irq_enable & 0x04) {
                    int oldstate = status & 3;
                    status |= 1;
                    if (!oldstate && irqhandler) irqhandler(device, 1);
                }
                if (irq_enable & 0x80) csm_req = 2;
            }
        }

        if (tim_B) {
            tim_B_val -= (1 << TIMER_SH);
            if (tim_B_val <= 0) {
                tim_B_val += tim_B_tab[timer_B_index];
                if (irq_enable & 0x08) {
                    int oldstate = status & 3;
                    status |= 2;
                    if (!oldstate && irqhandler) irqhandler(device, 1);
                }
            }
        }
        advanceLfoAndPhase();
    }
}

// ーーー ここに次の返信でお渡しする writeReg などの実装が合流します ーーー
// ーーー 前半パートの末尾からそのまま地続きで追記してください ーーー

// レジスタ書き込み処理（旧 ym2151_write_reg の完全移植）
void YM2151::writeReg(uint8_t reg, uint8_t data) {
    regs[reg] = data; // シャドウレジスタへ状態を保存

    switch (reg) {
    case 0x01: // テスト制御レジスタ
        test = data;
        break;

    case 0x08: { // キーオン / キーオフ制御
        unsigned int ch = data & 7;
        YM2151Operator* op = &oper[ch * 4];
        
        // ビット3:M1, ビット4:C1, ビット5:M2, ビット6:C2
        if (data & 0x08) keyOn(&op[0], 1);  else keyOff(&op[0], ~1);
        if (data & 0x20) keyOn(&op[1], 1);  else keyOff(&op[1], ~1);
        if (data & 0x10) keyOn(&op[2], 1);  else keyOff(&op[2], ~1);
        if (data & 0x40) keyOn(&op[3], 1);  else keyOff(&op[3], ~1);
        break;
    }

    case 0x0f: // ノイズ無効化 / 周波数設定
        noise = data;
        noise_f = noise_tab[data & 0x1f];
        break;

    case 0x10: // タイマーA 上位バイト
        timer_A_index = (timer_A_index & 0x03) | (static_cast<uint32_t>(data) << 2);
        break;

    case 0x11: // タイマーA 下位バイト
        timer_A_index = (timer_A_index & 0x3fc) | (data & 0x03);
        break;

    case 0x12: // タイマーB カウンタ値
        timer_B_index = data;
        break;

    case 0x14: { // タイマー制御 / CSM / IRQ制御
        irq_enable = data & 0xbc;

        // タイマーAのステータスクリア
        if (data & 0x10) {
            uint8_t old = status;
            status &= ~1;
            if (old && !status && irqhandler) irqhandler(device, 0);
        }
        // タイマーBのステータスクリア
        if (data & 0x20) {
            uint8_t old = status;
            status &= ~2;
            if (old && !status && irqhandler) irqhandler(device, 0);
        }

        // タイマーAのロード・ストップ制御
        if (data & 0x01) {
            if (!tim_A) {
                tim_A_val = tim_A_tab[timer_A_index];
                tim_A = 1;
            }
        } else {
            tim_A = 0;
        }

        // タイマーBのロード・ストップ制御
        if (data & 0x02) {
            if (!tim_B) {
                tim_B_val = tim_B_tab[timer_B_index];
                tim_B = 1;
            }
        } else {
            tim_B = 0;
        }
        break;
    }

    case 0x18: // LFO 周波数設定
        lfo_counter_add = data;
        break;

    case 0x19: // LFO 変調深度 (AM / PM)
        if (data & 0x80) {
            pmd = data & 0x7f;
        } else {
            amd = data & 0x7f;
        }
        break;

    case 0x1b: // CT出力 / LFO 波形選択
        lfo_wsel = data & 3;
        if (porthandler) {
            porthandler(device, 0, data >> 6);
        }
        break;

    default:
        // 0x20 以降のチャンネル・オペレータ個別書き込み領域
        if (reg >= 0x20 && reg <= 0x3f) {
            // チャンネル単位設定 (0x20 - 0x3f)
            unsigned int ch = reg & 7;
            YM2151Operator* op = &oper[ch * 4];

            if (reg >= 0x20 && reg <= 0x27) {
                // パンニング、フィードバック、アルゴリズム (CON)
                pan[ch * 2]     = (data & 0x80) ? 0xffffffff : 0; // Left
                pan[ch * 2 + 1] = (data & 0x40) ? 0xffffffff : 0; // Right
                
                uint8_t fb = (data >> 3) & 7;
                op[0].fb_shift = fb ? fb + 5 : 0;

                // アルゴリズム(CON)に基づきマトリックスポインタを完全配線
                uint8_t alg = data & 7;
                switch (alg) {
                case 0:
                    op[0].connect = &m2;
                    op[1].connect = &c1;
                    op[2].connect = &c2;
                    break;
                case 1:
                    op[0].connect = &c1;
                    op[1].connect = &c1;
                    op[2].connect = &c2;
                    break;
                case 2:
                    op[0].connect = &c2;
                    op[1].connect = &c1;
                    op[2].connect = &c2;
                    break;
                case 3:
                    op[0].connect = &m2;
                    op[1].connect = &c2;
                    op[2].connect = &c2;
                    break;
                case 4:
                    op[0].connect = &m2;
                    op[1].connect = &chanout[ch];
                    op[2].connect = &c2;
                    break;
                case 5:
                    op[0].connect = nullptr; // chanCalc側で特殊分岐（Alg5）
                    op[1].connect = &chanout[ch];
                    op[2].connect = &chanout[ch];
                    break;
                case 6:
                    op[0].connect = &m2;
                    op[1].connect = &chanout[ch];
                    op[2].connect = &chanout[ch];
                    break;
                case 7:
                    op[0].connect = &chanout[ch];
                    op[1].connect = &chanout[ch];
                    op[2].connect = &chanout[ch];
                    break;
                }
            } else if (reg >= 0x28 && reg <= 0x2f) {
                // キーコード (KC) の反映と周波数再計算
                refreshRegisters();
            } else if (reg >= 0x30 && reg <= 0x37) {
                // キーフラクション (KF) の反映と周波数再計算
                refreshRegisters();
            } else if (reg >= 0x38 && reg <= 0x3f) {
                // PMS (PM感度) / AMS (AM感度)
                for (int i = 0; i < 4; i++) {
                    op[i].pms = (data >> 4) & 7;
                    op[i].ams = data & 3;
                }
            }
        } else if (reg >= 0x40 && reg <= 0xff) {
            // オペレータ単位設定 (0x40 - 0xff)
            // YM2151のオペレータ配置配列インデックスへのデコード
            unsigned int op_idx = ((reg & 7) * 4) + ((reg >> 3) & 3);
            YM2151Operator* op = &oper[op_idx];

            if (reg >= 0x40 && reg <= 0x5f) {
                // デチューン1 (DT1) / マルチプル (MUL)
                op->dt1 = (data >> 4) & 7;
                op->mul = data & 0x0f;
                if (!op->mul) op->mul = 1; // MUL=0は実際には0.5倍の挙動だが、整数演算スケール上1として処理
                refreshRegisters();
            } else if (reg >= 0x60 && reg <= 0x7f) {
                // トータルレベル (TL)
                op->tl = data & 0x7f;
            } else if (reg >= 0x80 && reg <= 0x9f) {
                // アタックレート (AR) / キースケール (KS)
                uint8_t ar = data & 0x1f;
                op->eg_sh_ar = ar ? 15 - (ar >> 2) : 0;
                op->eg_sel_ar = (ar & 3) * 8;
            } else if (reg >= 0xa0 && reg <= 0xbf) {
                // デケイ1レート (D1R) / 振幅変調有効化 (AMON)
                uint8_t d1r = data & 0x1f;
                op->eg_sh_d1r = d1r ? 15 - (d1r >> 2) : 0;
                op->eg_sel_d1r = (d1r & 3) * 8;
                op->AMmask = (data & 0x80) ? 0xffffffff : 0;
            } else if (reg >= 0xc0 && reg <= 0xdf) {
                // デケイ2レート (D2R) / デチューン2 (DT2)
                uint8_t d2r = data & 0x1f;
                op->eg_sh_d2r = d2r ? 15 - (d2r >> 2) : 0;
                op->eg_sel_d2r = (d2r & 3) * 8;
                op->dt2 = (data >> 6) & 3;
                refreshRegisters();
            } else if (reg >= 0xe0 && reg <= 0xff) {
                // リリースレート (RR) / デケイ1レベル (D1L)
                uint8_t rr = data & 0x0f;
                op->eg_sh_rr = rr ? 15 - (rr >> 2) : 0;
                op->eg_sel_rr = (rr & 3) * 8;
                
                uint32_t d1l_val = (data >> 4) & 0x0f;
                op->d1l = d1l_val ? d1l_val << 5 : MAX_ATT_INDEX;
            }
        }
        break;
    }
}

// レジスタ変更に追従して、全オペレータの内部ピッチカウンタを同期・一括更新する
void YM2151::refreshRegisters() {
    for (unsigned int ch = 0; ch < 8; ch++) {
        uint32_t kc = regs[0x28 + ch];
        uint32_t kf = regs[0x30 + ch];
        
        // KC (キーコード) と KF (キーフラクション) から 1024 段階のスケールへデコード
        uint32_t kc_i = (kc & 0x7f) * 8 + (kf >> 5);

        for (unsigned int i = 0; i < 4; i++) {
            YM2151Operator* op = &oper[ch * 4 + i];
            op->kc_i = kc_i;
            
            // DT2 と DT1、そして MUL を加味した最終増分ステップ（周波数）を算出
            uint32_t channel_pitch = kc_i + op->dt2;
            if (channel_pitch >= 1024) channel_pitch = 1023;
            
            op->freq = ((freq_array[channel_pitch] + op->dt1) * op->mul) >> 1;
        }
    }
}

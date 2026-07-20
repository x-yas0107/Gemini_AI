/*********************************************************************
 * ファイル名      : main.c
 * バージョン      : 0.31
 * 日付            : 2026-07-20
 * 説明            : CH32V003 6chシンセ (UARTオーバーラン対策版)
 * 変更履歴        :
 * - V0.29: 波形合成アルゴリズム(Wavetable風)の導入。
 * - V0.30: ストリングス/パッド系(env:3)のアタック速度が遅すぎたため、
 *          短い音符でも確実に立ち上がるように演算係数(t*2)を最適化。
 * - V0.31: UARTオーバーラン対策。波形計算・エンベロープ計算中の
 *          CPU負荷によるシリアル受信の取りこぼしを解消するため、
 *          割り算をシフト演算へ最適化し、受信バッファの全読み出し(while)を実装。
 **********************************************************************/
#include "debug.h"
#include "ch32v00x.h"

typedef struct {
    uint32_t phase;
    uint32_t delta;
    uint8_t  volume;    
    uint8_t  base_vol;  
    uint8_t  inst;      // Env Type (0:Organ, 1:Piano, 2:Guitar, 3:Strings)
    uint8_t  wave;      // Wave Type (0:Square, 1:Sawtooth, 2:Triangle, 3:Pulse)
    uint16_t env_tick;  
} PSG_Channel_t;

volatile PSG_Channel_t channels[6];
volatile uint32_t sample_ticks = 0;

uint8_t audio_buf[2][256];
volatile uint8_t play_buf = 0;
uint8_t load_buf = 0;
volatile uint16_t play_ptr = 0;
uint16_t load_ptr = 256;

uint8_t auto_play = 0; 
uint8_t current_song = 255; 
uint8_t current_step = 0;
uint32_t step_timer = 0;
uint32_t tempo_wait = 150;

uint8_t rx_state = 0;
uint8_t rx_cmd = 0;
uint8_t rx_ch = 0;
uint8_t rx_note = 0;
uint8_t rx_vol = 0;
uint8_t rx_val = 0;
uint8_t rx_env = 0;
uint8_t rx_wave = 0;

typedef struct { uint8_t note[6]; } Step_t;
const Step_t song_test[16] = {{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}},{{72,0,0,0,0,0}},{{0,0,0,0,0,0}}};
const Step_t song_techno[16] = {{{60,67,36,0,0,0}},{{0,72,0,0,0,0}},{{63,67,36,0,0,0}},{{0,72,0,0,0,0}},{{65,70,41,0,0,0}},{{0,74,0,0,0,0}},{{67,72,43,0,0,0}},{{0,76,0,0,0,0}},{{60,67,36,0,0,0}},{{0,72,0,0,0,0}},{{63,67,36,0,0,0}},{{0,72,0,0,0,0}},{{68,75,44,0,0,0}},{{67,74,43,0,0,0}},{{65,72,41,0,0,0}},{{63,70,39,0,0,0}}};
const Step_t song_bass[16] = {{{48,0,36,0,0,0}},{{0,0,36,0,0,0}},{{51,0,39,0,0,0}},{{0,0,36,0,0,0}},{{53,0,41,0,0,0}},{{0,0,41,0,0,0}},{{55,0,43,0,0,0}},{{56,0,44,0,0,0}},{{48,0,36,0,0,0}},{{0,0,36,0,0,0}},{{51,0,39,0,0,0}},{{0,0,36,0,0,0}},{{46,0,34,0,0,0}},{{46,0,34,0,0,0}},{{47,0,35,0,0,0}},{{47,0,35,0,0,0}}};
const Step_t song_fantasy[16] = {{{72,76,60,0,0,0}},{{0,0,0,0,0,0}},{{74,79,62,0,0,0}},{{0,0,0,0,0,0}},{{76,81,64,0,0,0}},{{0,0,0,0,0,0}},{{79,84,67,0,0,0}},{{0,0,0,0,0,0}},{{81,86,69,0,0,0}},{{0,0,0,0,0,0}},{{79,84,67,0,0,0}},{{0,0,0,0,0,0}},{{76,81,64,0,0,0}},{{0,0,0,0,0,0}},{{74,79,62,0,0,0}},{{0,0,0,0,0,0}}};
const Step_t song_stereo[16] = {{{60,64,48,0,0,0}},{{62,66,50,0,0,0}},{{64,67,52,0,0,0}},{{67,71,55,0,0,0}},{{69,72,57,0,0,0}},{{67,71,55,0,0,0}},{{64,67,52,0,0,0}},{{62,66,50,0,0,0}},{{60,64,48,0,0,0}},{{62,66,50,0,0,0}},{{64,67,52,0,0,0}},{{67,71,55,0,0,0}},{{72,76,60,0,0,0}},{{71,74,59,0,0,0}},{{69,72,57,0,0,0}},{{67,71,55,0,0,0}}};

uint32_t get_delta(uint8_t note) {
    if (note == 0) return 0;
    const uint32_t lut12[] = {50960322,53990669,57201124,60602511,64206124,68024220,72069273,76354816,80894982,85705353,90801648,96200984};
    int8_t octave = (note / 12) - 5;
    uint32_t d = lut12[note % 12];
    if (octave > 0) d <<= octave;
    else if (octave < 0) d >>= (-octave);
    return d;
}

void TIM1_Audio_Init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_TIM1;
    GPIOC->CFGLR &= ~(0xF << (4 * 4)); GPIOC->CFGLR |= (0xB << (4 * 4));
    GPIOD->CFGLR &= ~(0xF << (2 * 4)); GPIOD->CFGLR |= (0xB << (2 * 4));
    TIM1->PSC = 0; TIM1->ATRLR = 255; 
    TIM1->CHCTLR1 |= TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC1PE; 
    TIM1->CHCTLR2 |= TIM_OC4M_2 | TIM_OC4M_1 | TIM_OC4PE; 
    TIM1->CCER |= TIM_CC1E | TIM_CC4E; TIM1->BDTR |= TIM_AOE | TIM_MOE; TIM1->CTLR1 |= TIM_CEN;
}

void TIM2_Stopwatch_Init(void) {
    RCC->APB1PCENR |= RCC_APB1Periph_TIM2;
    TIM2->PSC = 47; 
    TIM2->ATRLR = 65535; 
    TIM2->CTLR1 |= TIM_CEN;
}

void LED_Init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;
    GPIOC->CFGLR &= ~((0xF<<0) | (0xF<<12) | (0xF<<20) | (0xF<<24) | (0xF<<28));
    GPIOC->CFGLR |=  ((0x3<<0) | (0x3<<12) | (0x3<<20) | (0x3<<24) | (0x3<<28));
    GPIOD->CFGLR &= ~(0xF<<0); GPIOD->CFGLR |= (0x3<<0);
    GPIOC->BCR = (1<<0) | (1<<3) | (1<<5) | (1<<6) | (1<<7); GPIOD->BCR = (1<<0);
}

void silence_all_channels() {
    for (int i = 0; i < 6; i++) { 
        channels[i].volume = 0; 
        channels[i].delta = 0; 
        channels[i].base_vol = 0;
    }
}

void update_audio_parameters(uint8_t song, uint8_t step) {
    const Step_t* songs[5] = {song_test, song_techno, song_bass, song_fantasy, song_stereo};
    const Step_t* p_song = songs[song];
    for (int i = 0; i < 6; i++) {
        uint8_t note = p_song[step].note[i];
        if (note > 0) {
            channels[i].delta = get_delta(note);
            if (song == 0) { channels[i].base_vol = 200; channels[i].inst = 0; channels[i].wave = 0; }
            else if (song == 1) { channels[i].base_vol = (i == 2) ? 160 : 100; channels[i].inst = 1; channels[i].wave = (i == 2) ? 1 : 0; }
            else if (song == 2) { if (i == 2) { channels[i].base_vol = 255; channels[i].inst = 2; channels[i].wave = 1; } else { channels[i].base_vol = 50; channels[i].inst = 0; channels[i].wave = 0; } }
            else if (song == 3) { channels[i].base_vol = 140; channels[i].inst = 3; channels[i].wave = 2; }
            else if (song == 4) { channels[i].base_vol = 120; channels[i].inst = 1; channels[i].wave = 3; }
            channels[i].env_tick = 0;
        } else { if (song != 3) { channels[i].base_vol = 0; } }
    }
    if (song == 1 && (step == 4 || step == 12)) { channels[3].delta = 8888888; channels[3].base_vol = 160; channels[3].inst = 2; channels[3].wave = 3; channels[3].env_tick = 0; }
}

inline void poll_audio(void) {
    if(TIM2->CNT >= 45) {
        TIM2->CNT -= 45;
        uint8_t val = audio_buf[play_buf][play_ptr];
        TIM1->CH1CVR = val;
        TIM1->CH4CVR = val;
        play_ptr++;
        sample_ticks++;
        if(play_ptr >= 256) {
            play_ptr = 0;
            play_buf ^= 1;
        }
    }
}

// 修正点1: while文に変更し、到着している全データを残さず処理する
inline void poll_uart(void) {
    while (USART1->STATR & (1 << 5)) {
        char c = USART1->DATAR;
        if (c == 'N' || c == 'n' || c == 'S' || c == 's' || c == 'T' || c == 't' || c == 'I' || c == 'i') { 
            rx_cmd = c; rx_state = 1; rx_val = 0; 
        }
        else if (c == ',') {
            if (rx_cmd == 'N' || rx_cmd == 'n') {
                if (rx_state == 1)      { rx_val = 0; rx_state = 2; }
                else if (rx_state == 2) { rx_ch = rx_val; rx_val = 0; rx_state = 3; }
                else if (rx_state == 3) { rx_note = rx_val; rx_val = 0; rx_state = 4; }
                else if (rx_state == 4) { rx_vol = rx_val; rx_val = 0; rx_state = 5; }
            } else if (rx_cmd == 'I' || rx_cmd == 'i') {
                if (rx_state == 1)      { rx_val = 0; rx_state = 2; }
                else if (rx_state == 2) { rx_ch = rx_val; rx_val = 0; rx_state = 3; }
                else if (rx_state == 3) { rx_env = rx_val; rx_val = 0; rx_state = 4; }
            } else if (rx_cmd == 'T' || rx_cmd == 't') { 
                if (rx_state == 1) { rx_val = 0; rx_state = 2; } 
            }
        } else if (c >= '0' && c <= '9') { 
            rx_val = (rx_val * 10) + (c - '0'); 
        }
        else if (c == '\n' || c == '\r') {
            if ((rx_cmd == 'N' || rx_cmd == 'n') && rx_state == 5) {
                auto_play = 0; current_song = 255;
                if (rx_ch >= 1 && rx_ch <= 6) {
                    uint8_t idx = rx_ch - 1;
                    if (rx_vol == 0) { 
                        channels[idx].base_vol = 0; 
                    } else {
                        channels[idx].delta = get_delta(rx_note);
                        channels[idx].base_vol = rx_vol << 4;
                        channels[idx].env_tick = 0; 
                        channels[idx].volume = (channels[idx].inst == 3) ? 0 : channels[idx].base_vol;
                    }
                }
            } else if (rx_cmd == 'I' || rx_cmd == 'i') {
                if (rx_state == 3) {
                    if (rx_ch >= 1 && rx_ch <= 6) channels[rx_ch - 1].inst = rx_val;
                } else if (rx_state == 4) {
                    rx_wave = rx_val;
                    if (rx_ch >= 1 && rx_ch <= 6) {
                        channels[rx_ch - 1].inst = rx_env;
                        channels[rx_ch - 1].wave = rx_wave;
                    }
                }
            } else if (rx_cmd == 'S' || rx_cmd == 's') { 
                auto_play = 0; current_song = 255; silence_all_channels(); 
            } else if ((rx_cmd == 'T' || rx_cmd == 't') && rx_state == 2) {
                if (rx_val < 5) { silence_all_channels(); current_song = rx_val; current_step = 0; step_timer = 0; auto_play = 1; }
            }
            rx_state = 0; rx_cmd = 0;
        }
    }
}

void setup() {
    Serial.begin(115200);
    RCC->APB2PCENR |= RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOD;
    GPIOD->CFGLR &= ~((0xF << (5 * 4)) | (0xF << (6 * 4)));
    GPIOD->CFGLR |= ((0xB << (5 * 4)) | (0x4 << (6 * 4)));
    USART1->BRR = 0x1A1; USART1->CTLR1 = 0x200C; 
    SystemCoreClockUpdate(); 
    TIM1_Audio_Init(); 
    TIM2_Stopwatch_Init(); 
    LED_Init();

    for(int s = 0; s < 256; s++) { audio_buf[0][s] = 128; audio_buf[1][s] = 128; }
    play_buf = 0; load_buf = 1; play_ptr = 0; load_ptr = 0; TIM2->CNT = 0;
}

void loop() {
    poll_audio();
    poll_uart();

    if (sample_ticks >= 221) {
        sample_ticks -= 221;
        
        uint32_t pc_s = 0, pc_r = 0, pd_s = 0, pd_r = 0;
        if (channels[0].volume > 0) pc_s |= (1<<0); else pc_r |= (1<<0);
        if (channels[1].volume > 0) pc_s |= (1<<3); else pc_r |= (1<<3);
        if (channels[2].volume > 0) pc_s |= (1<<5); else pc_r |= (1<<5);
        if (channels[3].volume > 0) pc_s |= (1<<6); else pc_r |= (1<<6);
        if (channels[4].volume > 0) pc_s |= (1<<7); else pc_r |= (1<<7);
        if (channels[5].volume > 0) pd_s |= (1<<0); else pd_r |= (1<<0);
        GPIOC->BSHR = pc_s; GPIOC->BCR = pc_r; GPIOD->BSHR = pd_s; GPIOD->BCR = pd_r;

        for (int i = 0; i < 6; i++) {
            // 修正点2: 計算ループ中にも通信を拾うことで取りこぼしを防ぐ
            poll_uart(); 
            
            if (channels[i].base_vol == 0) {
                if (channels[i].volume > 15) channels[i].volume -= 15;
                else { channels[i].volume = 0; channels[i].delta = 0; }
            } else {
                channels[i].env_tick++;
                uint16_t t = channels[i].env_tick;
                uint8_t bv = channels[i].base_vol;
                
                // 修正点3: 割り算をビットシフト(>>)に置き換え、CPU負荷スパイクを消滅させる
                if (channels[i].inst == 0) {
                    channels[i].volume = bv;
                } else if (channels[i].inst == 1) {
                    uint16_t dec = t >> 3; 
                    if (bv > dec) channels[i].volume = bv - dec;
                    else { channels[i].volume = 0; channels[i].base_vol = 0; }
                } else if (channels[i].inst == 2) {
                    uint16_t dec = t >> 2; 
                    if (bv > dec) channels[i].volume = bv - dec;
                    else { channels[i].volume = 0; channels[i].base_vol = 0; }
                } else if (channels[i].inst == 3) {
                    uint16_t att = t << 1;
                    if (att < bv) channels[i].volume = att;
                    else channels[i].volume = bv;
                }
            }
        }

        if (auto_play && current_song < 5) {
            if (current_song == 0) tempo_wait = 250; else if (current_song == 1) tempo_wait = 100; else if (current_song == 2) tempo_wait = 160; else if (current_song == 3) tempo_wait = 260; else if (current_song == 4) tempo_wait = 120;
            if (step_timer == 0) { update_audio_parameters(current_song, current_step); current_step = (current_step + 1) % 16; }
            step_timer += 10; if (step_timer >= tempo_wait) step_timer = 0;
            if (current_song == 3) { for (int i = 0; i < 6; i++) { if (channels[i].base_vol > 4) channels[i].base_vol -= 3; else { channels[i].base_vol = 0; } } }
        }
    }

    poll_audio();
    poll_uart();

    if (play_buf == load_buf) {
        load_buf = play_buf ^ 1;
        load_ptr = 0;
    }

    if (load_ptr < 256) {
        for(int s = 0; s < 8; s++) {
            poll_audio();
            poll_uart();
            int16_t mix = 0;
            for (int i = 0; i < 6; i++) {
                channels[i].phase += channels[i].delta;
                if (channels[i].volume > 0) {
                    int8_t sample = 0;
                    uint32_t p = channels[i].phase;
                    
                    if (channels[i].wave == 0) {
                        sample = (p & 0x80000000) ? 127 : -128; 
                    } else if (channels[i].wave == 1) {
                        sample = (int8_t)((p >> 24) ^ 0x80);    
                    } else if (channels[i].wave == 2) {
                        uint8_t tp = p >> 24;                   
                        if (tp & 0x80) tp = ~tp;
                        sample = (tp << 1) - 128;
                    } else {
                        sample = (p >> 30) == 0 ? 127 : -128;   
                    }
                    
                    mix += (sample * channels[i].volume) >> 10;
                }
            }
            mix += 128;
            if (mix > 255) mix = 255;
            if (mix < 0) mix = 0;
            audio_buf[load_buf][load_ptr++] = (uint8_t)mix;
        }
    }
}
/*
 * ======================================================================================
 * Project: CH32V003J4M6_MIDI
 * Version: 0.16
 * Date: 2026/07/21
 *
 * [Description]
 * CH32V003J4M6 (8-pin) dedicated MIDI synthesizer.
 * 6-polyphony, software envelope & waveform generation.
 * 
 * [I/O Pin Map]
 * Pin 1 : PD6 (UART1 RX - MIDI Input 115200bps)
 * Pin 4 : VDD (3.3V)
 * Pin 5 : PC1 (Monitor LED - RX Indicator toggle on command)
 * Pin 7 : PC4 (PWM Audio Output - TIM1 CH4)
 * Pin 8 : PD1 (SWIO - Programming)
 *
 * [Change History]
 * v0.11 : Implemented 128-byte UART ring buffer to prevent command drop.
 * v0.12 : Rolled back architecture to stable v0.11 baseline.
 * v0.13 : FULL AUDIO ENGINE REWRITE (UIAPduino V0.31 port).
 *         - Changed sample rate to 22.222kHz (TIM2 Period = 2159 at 48MHz).
 *         - Replaced note_to_inc array with UIAPduino get_delta() logic.
 *         - Implemented 100Hz sub-tick for UIAPduino-accurate envelope decays.
 *         - Replaced quantization-heavy wave math with UIAPduino bitwise math.
 * v0.14 : Fixed volume multiplier bug causing premature decay cutoff.
 *         Restored UIAPduino original 'vol << 4' calculation for base volume.
 * v0.15 : Added parser hard-sync for 'N', 'I', 'S' to prevent ghost notes.
 *         (Introduced bug: wrong parser start index, skipped comma caused silence).
 * v0.16 : Fixed zero-audio bug by reverting parse index to &cmd_buf[2].
 *         Fixed critical memory corruption (ghost notes) by mapping 1-6 channel to 0-5 array index.
 *         Restored accurate volume scaling (vol << 4) for UIAPduino sound quality.
 * ======================================================================================
 */

#include "ch32v00x.h"
#include <stdlib.h>
#include <Arduino.h>

#define NUM_CH 6
#define RX_BUF_SIZE 128

volatile uint32_t phase[NUM_CH] = {0};
volatile uint32_t phase_inc[NUM_CH] = {0};
volatile uint8_t  wave_type[NUM_CH] = {0, 0, 0, 0, 0, 0};
volatile uint8_t  env_type[NUM_CH]  = {0, 0, 0, 0, 0, 0}; 
volatile uint8_t  base_vol[NUM_CH] = {0};
volatile uint8_t  env_level[NUM_CH] = {0};
volatile uint16_t env_tick[NUM_CH] = {0};
volatile uint8_t  env_state[NUM_CH] = {0};
volatile uint8_t  current_note[NUM_CH] = {0};

volatile char rx_buf[RX_BUF_SIZE];
volatile uint8_t rx_head = 0;
volatile uint8_t rx_tail = 0;

char cmd_buf[32];
uint8_t cmd_idx = 0;

// UIAPduino exact delta calculation
uint32_t get_delta(uint8_t note) {
    if (note == 0) return 0;
    const uint32_t lut12[] = {50960322,53990669,57201124,60602511,64206124,68024220,72069273,76354816,80894982,85705353,90801648,96200984};
    int8_t octave = (note / 12) - 5;
    uint32_t d = lut12[note % 12];
    if (octave > 0) d <<= octave;
    else if (octave < 0) d >>= (-octave);
    return d;
}

void init_clock_48mhz() {
    FLASH->ACTLR = (FLASH->ACTLR & ~((uint32_t)0x1F)) | ((uint32_t)0x01);
    RCC->CTLR |= (uint32_t)0x01000000;
    while((RCC->CTLR & (uint32_t)0x02000000) == 0);
    RCC->CFGR0 = (RCC->CFGR0 & ~((uint32_t)0x03)) | ((uint32_t)0x02);
    while ((RCC->CFGR0 & (uint32_t)0x0C) != (uint32_t)0x08);
    SystemCoreClock = 48000000;
}

void init_pwm() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_TimeBaseStructure.TIM_Period = 255;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 128;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM1, &TIM_OCInitStructure);
    
    TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

void init_monitor_led() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void init_uart() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    USART_InitTypeDef USART_InitStructure = {0};
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART1_IRQn);
    
    USART_Cmd(USART1, ENABLE);
}

void init_timer2() {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    // UIAPduino sample rate: 48MHz / 2160 = 22,222.22 Hz
    TIM_TimeBaseStructure.TIM_Period = 2159; 
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_Cmd(TIM2, ENABLE);
}

#ifdef __cplusplus
extern "C" {
#endif

void USART1_IRQHandler(void) __attribute__((interrupt));
void USART1_IRQHandler(void) {
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        char c = USART_ReceiveData(USART1);
        rx_buf[rx_head] = c;
        rx_head = (rx_head + 1) % RX_BUF_SIZE;
    }
}

void TIM2_IRQHandler(void) __attribute__((interrupt));
void TIM2_IRQHandler(void) {
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        
        // 100Hz divider for envelope updates (22222Hz / 222 = ~100Hz)
        static uint16_t env_divider = 0;
        env_divider++;
        uint8_t tick_100hz = 0;
        if(env_divider >= 222) {
            env_divider = 0;
            tick_100hz = 1;
        }
        
        int16_t mix = 0;
        for(uint8_t i = 0; i < NUM_CH; i++) {
            if(env_state[i] > 0) {
                
                // UIAPduino exact Envelope Logic (runs at 100Hz)
                if (tick_100hz) {
                    if (env_state[i] == 3) { // Release
                        if (env_level[i] > 15) env_level[i] -= 15;
                        else { env_level[i] = 0; env_state[i] = 0; base_vol[i] = 0; }
                    } else { // Playing
                        env_tick[i]++;
                        uint16_t t = env_tick[i];
                        uint8_t bv = base_vol[i];
                        
                        if (env_type[i] == 0) { // Organ
                            env_level[i] = bv;
                        } else if (env_type[i] == 1) { // Piano
                            uint16_t dec = t >> 3; 
                            if (bv > dec) env_level[i] = bv - dec;
                            else { env_level[i] = 0; base_vol[i] = 0; env_state[i] = 0; }
                        } else if (env_type[i] == 2) { // Guitar
                            uint16_t dec = t >> 2; 
                            if (bv > dec) env_level[i] = bv - dec;
                            else { env_level[i] = 0; base_vol[i] = 0; env_state[i] = 0; }
                        } else if (env_type[i] == 3) { // Strings
                            uint16_t att = t << 1;
                            if (att < bv) env_level[i] = att;
                            else env_level[i] = bv;
                        }
                    }
                }
                
                // UIAPduino exact Waveform Logic
                if (env_level[i] > 0) {
                    phase[i] += phase_inc[i];
                    int8_t sample = 0;
                    uint32_t p = phase[i];
                    
                    if (wave_type[i] == 0) { // Square
                        sample = (p & 0x80000000) ? 127 : -128; 
                    } else if (wave_type[i] == 1) { // Saw
                        sample = (int8_t)((p >> 24) ^ 0x80);    
                    } else if (wave_type[i] == 2) { // Triangle
                        uint8_t tp = p >> 24;                   
                        if (tp & 0x80) tp = ~tp;
                        sample = (tp << 1) - 128;
                    } else { // Pulse/Noise
                        sample = (p >> 30) == 0 ? 127 : -128;   
                    }
                    
                    mix += (sample * env_level[i]) >> 10;
                }
            }
        }
        
        mix += 128;
        if(mix > 255) mix = 255;
        if(mix < 0) mix = 0;
        
        TIM1->CH4CVR = (uint8_t)mix;
    }
}
#ifdef __cplusplus
}
#endif

void parse_command() {
    if(cmd_buf[0] == 'N') {
        uint8_t ch = 255, note = 255, vol = 255;
        char *p = &cmd_buf[2]; // Fixed: Reverted to &cmd_buf[2] to skip "N,"
        
        ch = 0; while(*p >= '0' && *p <= '9') { ch = ch * 10 + (*p - '0'); p++; }
        if(*p == ',') p++;
        note = 0; while(*p >= '0' && *p <= '9') { note = note * 10 + (*p - '0'); p++; }
        if(*p == ',') p++;
        vol = 0; while(*p >= '0' && *p <= '9') { vol = vol * 10 + (*p - '0'); p++; }
        
        if(ch >= 1 && ch <= NUM_CH && note >= 24 && note <= 107) {
            uint8_t idx = ch - 1; // Fixed: Map 1-6 to 0-5
            if(vol > 0) {
                phase_inc[idx] = get_delta(note);
                base_vol[idx] = vol << 4; // Restored: Map 0-15 MIDI vol to 0-240
                current_note[idx] = note;
                env_tick[idx] = 0;
                env_level[idx] = (env_type[idx] == 3) ? 0 : base_vol[idx];
                env_state[idx] = 1; 
            } else {
                if(current_note[idx] == note) { 
                    env_state[idx] = 3; // Trigger Release
                }
            }
        }
    } else if(cmd_buf[0] == 'I') {
        uint8_t ch = 255, env = 255, wave = 255;
        char *p = &cmd_buf[2]; // Fixed: Reverted to &cmd_buf[2] to skip "I,"
        
        ch = 0; while(*p >= '0' && *p <= '9') { ch = ch * 10 + (*p - '0'); p++; }
        if(*p == ',') p++;
        env = 0; while(*p >= '0' && *p <= '9') { env = env * 10 + (*p - '0'); p++; }
        if(*p == ',') p++;
        wave = 0; while(*p >= '0' && *p <= '9') { wave = wave * 10 + (*p - '0'); p++; }
        
        if(ch >= 1 && ch <= NUM_CH) {
            uint8_t idx = ch - 1; // Fixed: Map 1-6 to 0-5
            env_type[idx] = env;
            wave_type[idx] = wave;
        }
    } else if(cmd_buf[0] == 'S') {
        for(uint8_t i = 0; i < NUM_CH; i++) {
            env_state[i] = 3;
        }
    }
    
    GPIOC->OUTDR ^= GPIO_Pin_1;
}

void setup() {
    init_clock_48mhz();
    init_pwm();
    init_monitor_led();
    init_uart();
    init_timer2();
}

void loop() {
    while(rx_head != rx_tail) {
        char c = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        
        if(c == 'N' || c == 'I' || c == 'S') { // Hard-sync parser to command start
            cmd_idx = 0;
            cmd_buf[cmd_idx++] = c;
        } else if(c == '\n') {
            cmd_buf[cmd_idx] = '\0';
            parse_command();
            cmd_idx = 0;
        } else if (c != '\r') {
            if(cmd_idx < 31) {
                cmd_buf[cmd_idx++] = c;
            }
        }
    }
}
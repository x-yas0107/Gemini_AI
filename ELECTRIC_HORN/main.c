/*********************************************************************************
 * [PROJECT NAME] : ELECTRIC HORN (SKY-SYNC006)
 * [VERSION]      : 1.08 "Real Yankee Edit" (Fluctuation Mode)
 * [DEVELOPERS]   : yas & Gemini (Joint Development)
 * [DATE]         : 2026-01-30
 * [HARDWARE]     : CH32V006F8P6 (TSSOP20)
 * * ===============================================================================
 * 【設計思想：Real Yankee Edit について】
 * デジタル音声は正確すぎて「3音の和音」が「1つの音」に聞こえがちです。
 * 本バージョンでは、各周波数をあえて数Hz未満の単位でずらし（デチューン）、
 * さらに割り込みごとに微細なゆらぎを加えることで、エアーホーン特有の
 * 「空気が震えて音がぶつかり合う実在感」を再現しています。
 * ===============================================================================
 * * [Pin Mapping / ピンアサイン (TSSOP20 画像準拠)]
 * Pin 01 (PD4): Mix Out (TIM2_CH1) -> 合成和音出力
 * Pin 02 (PD5): SW1 (Waveform)     -> 波形切替 (Sine/Square/Saw)
 * Pin 03 (PD6): SW2 (Horn ON)      -> ホーンON & ADC読み取りトリガー
 * Pin 05 (PA1): Solo S2 (TIM1_CH2) -> 独立中音出力
 * Pin 07 (VSS): GND
 * Pin 09 (VDD): VCC (3.3V - 5.0V)
 * Pin 10 (PC0): Status LED         -> 動作確認用
 * Pin 11 (PC1): SW3 (Style)        -> スタイル切替 (Classic/Pop/Euro)
 * Pin 13 (PC3): Solo S3 (TIM1_CH3) -> 独立高音出力
 * Pin 19 (PD2): Solo S1 (TIM1_CH1) -> 独立低音出力
 * Pin 20 (PD3): ADC In (CH4)       -> 余韻(Fade-out)調整ボリューム
 *********************************************************************************/

#include "debug.h"
#include <string.h>

#define SAMPLING_RATE 22050 

/* --- Audio Waveforms / 音色テーブル (256 samples) --- */
const uint8_t sine_table[256] = {
    128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,
    176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,216,
    218,220,222,224,226,228,230,232,234,236,237,239,240,242,243,245,
    246,247,248,249,250,251,252,252,253,254,254,255,255,255,255,255,
    255,255,255,255,255,255,254,254,253,252,252,251,250,249,248,247,
    246,245,243,242,240,239,237,236,234,232,230,228,226,224,222,220,
    218,216,213,211,209,206,204,201,199,196,193,191,188,185,182,179,
    176,174,171,168,165,162,159,156,152,149,146,143,140,137,134,131,
    128,124,121,118,115,112,109,106,103,99,96,93,90,87,84,81,
    79,76,73,70,67,64,62,59,56,54,51,49,46,44,42,39,
    37,35,33,31,29,27,25,23,21,19,18,16,15,13,12,10,
    9,8,7,6,5,4,3,3,2,1,1,0,0,0,0,0,
    0,0,0,0,0,0,1,1,2,3,3,4,5,6,7,8,
    9,10,12,13,15,16,18,19,21,23,25,27,29,31,33,35,
    37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,
    79,81,84,87,90,93,96,99,103,106,109,112,115,118,121,124
};

/* --- Pitch Style (with Micro-Detune) / 和音スタイル (分離感を出すため僅かにずらし済み) --- */
typedef struct { float f1, f2, f3; } FreqStyle;
const FreqStyle styles[3] = {
    {330.55f, 411.82f, 496.18f}, // Classic: 重厚で深いうねり
    {440.32f, 553.75f, 661.12f}, // Pop: 華やかな3連ヤンキー
    {500.15f, 624.91f, 751.04f}  // Euro: 鋭い高音
};

/* System Variables */
volatile uint8_t style_mode = 0, wave_mode = 0, horn_on = 0;
uint32_t phase1, phase2, phase3, inc1, inc2, inc3;
uint32_t jitter_cnt = 0; // ゆらぎ生成用
#define MAX_VOL 1048576 
volatile uint32_t master_vol = 0, fade_step = 100;

/* Logic: Calculate Phase Increment / 周波数の増分計算 */
void Update_Frequencies(void) {
    inc1 = (uint32_t)((styles[style_mode].f1 * 4294967296.0f) / SAMPLING_RATE);
    inc2 = (uint32_t)((styles[style_mode].f2 * 4294967296.0f) / SAMPLING_RATE);
    inc3 = (uint32_t)((styles[style_mode].f3 * 4294967296.0f) / SAMPLING_RATE);
}

/* Core Engine: Audio Synthesis with Jitter / ゆらぎ付き音声合成エンジン (TIM2) */
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        jitter_cnt++;
        
        /* 独立した「ゆらぎ（ジッタ）」の生成：音が生きているように微細に震わせる */
        uint32_t j1 = (jitter_cnt & 0x07); 
        uint32_t j2 = ((jitter_cnt >> 1) & 0x07);
        uint32_t j3 = ((jitter_cnt >> 2) & 0x07);

        phase1 += (inc1 + j1); 
        phase2 += (inc2 + j2); 
        phase3 += (inc3 + j3);

        uint32_t o1 = 0, o2 = 0, o3 = 0;
        uint16_t mix_val = 0;

        /* Envelope Logic / 音量フェード制御 */
        if (horn_on) { 
            master_vol = MAX_VOL; 
        } else if (master_vol > fade_step) { 
            master_vol -= fade_step; 
        } else { 
            master_vol = 0; 
        }

        /* Waveform Generation / 波形生成 */
        if (master_vol > 0) {
            uint8_t p1 = (uint8_t)(phase1 >> 24), p2 = (uint8_t)(phase2 >> 24), p3 = (uint8_t)(phase3 >> 24);
            
            if (wave_mode == 0) { // Sine
                o1 = sine_table[p1]; o2 = sine_table[p2]; o3 = sine_table[p3]; 
            } else if (wave_mode == 1) { // Square
                o1 = (p1 < 128) ? 255 : 0; o2 = (p2 < 128) ? 255 : 0; o3 = (p3 < 128) ? 255 : 0; 
            } else { // Sawtooth
                o1 = p1; o2 = p2; o3 = p3; 
            }
            
            /* Apply Volume and Mixing / 音量適用と合成 */
            o1 = (o1 * master_vol) >> 20;
            o2 = (o2 * master_vol) >> 20;
            o3 = (o3 * master_vol) >> 20;
            mix_val = (uint16_t)((((uint32_t)o1 + o2 + o3) * 2177) / 765);
        }

        /* Register Outputs / ハードウェア出力 */
        TIM1->CH1CVR = (uint16_t)o1; // S1 (Pin 19)
        TIM1->CH2CVR = (uint16_t)o2; // S2 (Pin 05)
        TIM1->CH3CVR = (uint16_t)o3; // S3 (Pin 13)
        TIM2->CH1CVR = mix_val;      // Mix (Pin 01)

        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

/* Hardware Setup / 周辺機能初期化 */
void Setup_Hardware(void) {
    /* Enable Clocks */
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA|RCC_PB2Periph_GPIOC|RCC_PB2Periph_GPIOD|RCC_PB2Periph_TIM1|RCC_PB2Periph_ADC1|RCC_PB2Periph_AFIO, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_InitTypeDef g;
    /* Output: PD4(1), PD2(19) */
    g.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_2; g.GPIO_Mode = GPIO_Mode_AF_PP; g.GPIO_Speed = GPIO_Speed_30MHz; GPIO_Init(GPIOD, &g);
    /* Output: PA1(5) */
    g.GPIO_Pin = GPIO_Pin_1; GPIO_Init(GPIOA, &g);
    /* Output: PC3(13) */
    g.GPIO_Pin = GPIO_Pin_3; GPIO_Init(GPIOC, &g);
    
    /* Input Switch: PD5(2), PD6(3) */
    g.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6; g.GPIO_Mode = GPIO_Mode_IPU; GPIO_Init(GPIOD, &g);
    /* Input Switch: PC1(11) */
    g.GPIO_Pin = GPIO_Pin_1; g.GPIO_Mode = GPIO_Mode_IPU; GPIO_Init(GPIOC, &g);
    
    /* ADC: PD3(20) / LED: PC0(10) */
    g.GPIO_Pin = GPIO_Pin_3; g.GPIO_Mode = GPIO_Mode_AIN; GPIO_Init(GPIOD, &g);
    g.GPIO_Pin = GPIO_Pin_0; g.GPIO_Mode = GPIO_Mode_Out_PP; GPIO_Init(GPIOC, &g);

    /* ADC 12-bit Initialization */
    ADC_InitTypeDef a;
    a.ADC_Mode = ADC_Mode_Independent; a.ADC_ScanConvMode = DISABLE; a.ADC_ContinuousConvMode = DISABLE;
    a.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; a.ADC_DataAlign = ADC_DataAlign_Right; a.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &a); ADC_Cmd(ADC1, ENABLE);

    /* TIM1 (Solo Channels) Setup */
    TIM_TimeBaseInitTypeDef t;
    memset(&t, 0, sizeof(t));
    t.TIM_Period = 255; t.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &t);

    TIM_OCInitTypeDef o;
    memset(&o, 0, sizeof(o));
    o.TIM_OCMode = TIM_OCMode_PWM1; o.TIM_OutputState = TIM_OutputState_Enable;
    o.TIM_Pulse = 0; o.TIM_OCPolarity = TIM_OCPolarity_Low; 
    TIM_OC1Init(TIM1, &o); TIM_OC2Init(TIM1, &o); TIM_OC3Init(TIM1, &o);
    TIM_CtrlPWMOutputs(TIM1, ENABLE); TIM_Cmd(TIM1, ENABLE);

    /* TIM2 (Mix/Update) Setup */
    t.TIM_Period = 2177 - 1; TIM_TimeBaseInit(TIM2, &t);
    TIM_OC1Init(TIM2, &o); NVIC_EnableIRQ(TIM2_IRQn);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); TIM_Cmd(TIM2, ENABLE);
}

/* Main Loop */
int main(void) {
    SystemInit();
    Update_Frequencies();
    Setup_Hardware();

    uint8_t l1=1, l2=1, l3=1;
    uint32_t led_cnt = 0;

    while(1) {
        uint8_t sw1 = GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_5); // Waveform
        uint8_t sw2 = GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_6); // Horn ON
        uint8_t sw3 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1); // Style

        /* SW1 Handler: Change Waveform */
        if (l1==1 && sw1==0) { wave_mode = (wave_mode+1)%3; } l1=sw1;

        /* SW3 Handler: Change Style */
        if (l3==1 && sw3==0) { style_mode = (style_mode+1)%3; Update_Frequencies(); } l3=sw3;

        /* SW2 Handler: Horn & On-demand ADC */
        if (sw2==0) {
            if (l2==1) { // Sample ADC only once per push
                ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_CyclesMode7);
                ADC_SoftwareStartConvCmd(ADC1, ENABLE);
                while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
                fade_step = 190 - ((uint32_t)ADC_GetConversionValue(ADC1) * 171 / 4095);
            }
            horn_on = 1;
        } else { horn_on = 0; }
        l2=sw2;

        /* Visual Feedback (LED) */
        if (horn_on || master_vol > 0) { GPIO_WriteBit(GPIOC, GPIO_Pin_0, Bit_SET); }
        else {
            led_cnt++;
            if (led_cnt < 50000) { GPIO_WriteBit(GPIOC, GPIO_Pin_0, Bit_SET); }
            else { 
                GPIO_WriteBit(GPIOC, GPIO_Pin_0, Bit_RESET); 
                if (led_cnt > 500000) led_cnt = 0; 
            }
        }
    }
}
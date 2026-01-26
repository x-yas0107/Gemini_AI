/*********************************************************************************
 * Project Name : SKY-SYNC006 (Vintage Aircraft Compass Driver)
 * Version      : 1.07 (Final Stable / Adaptive Filter Edition)
 * Date         : 2026-01-25
 * Target MCU   : WCH CH32V006F8P6 (TSSOP20)
 * Developers   : yas & Gemini
 *
 * [Description]
 * 航空機用アナログコンパス (Synchro Motor) を駆動するための
 * 400Hz 3相交流信号 (26V/11.8V line equivalent signal source) を生成する。
 *
 * [Key Features]
 * 1. Precision 400Hz 3-Phase Sine Wave Generation (0, 120, 240 deg).
 * 2. Adaptive Filtering:
 * - Fast response when rotating the knob.
 * - High stability (strong smoothing) when holding position.
 * 3. Sync Signal Output for Oscilloscope Triggering.
 * 4. Real-time OLED Dashboard (Angle & Bar Graph).
 *
 * [Pin Assignment (TSSOP20)]
 * Pin  1 (PD4) : SYNC OUT (400Hz Square Wave for Trigger)
 * Pin  5 (PA1) : PWM S1   (TIM1_CH2) - Phase 0 deg
 * Pin  6 (PA2) : ADC IN   (ADC_CH0)  - Angle Control Volume
 * Pin 10 (PC0) : LED      (GPIO)     - Status Indicator
 * Pin 11 (PC1) : OLED SDA (GPIO)     - I2C Data
 * Pin 12 (PC2) : OLED SCL (GPIO)     - I2C Clock
 * Pin 13 (PC3) : PWM S2   (TIM1_CH3) - Phase 120 deg
 * Pin 14 (PC4) : PWM S3   (TIM1_CH4) - Phase 240 deg
 * Pin 19 (PD2) : PWM REF  (TIM1_CH1) - Reference Phase
 *********************************************************************************/

#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h> // for abs()

/* --- Type Definitions --- */
#ifndef u8
typedef uint8_t u8;
#endif

/* --- Constants --- */
#define PWM_PERIOD    500           // PWM Carrier Resolution
#define PI            3.14159265f   // PI for trigonometric calc

/* --- Pin Definitions --- */
// Display Interface (Bit-Bang I2C)
#define OLED_GPIO_PORT GPIOC
#define OLED_SCL_PIN   GPIO_Pin_2   // Pin 12
#define OLED_SDA_PIN   GPIO_Pin_1   // Pin 11

// User Interface
#define LED_PIN        GPIO_Pin_0   // Pin 10
#define SYNC_PIN       GPIO_Pin_4   // Pin  1 (Oscilloscope Trigger)

// PWM Output Pins
#define PWM_REF_PIN    GPIO_Pin_2   // Pin 19 (PD2)
#define PWM_S1_PIN     GPIO_Pin_1   // Pin  5 (PA1)
#define PWM_S2_PIN     GPIO_Pin_3   // Pin 13 (PC3)
#define PWM_S3_PIN     GPIO_Pin_4   // Pin 14 (PC4)

/* --- Global Variables --- */
u8 OLED_Buffer[512];                // Screen Buffer
int last_deg = -999;                // Force initial update
volatile uint8_t wave_idx = 0;      // Current position in Sine Table (0-63)

// Amplitude for each phase
volatile int16_t amp_s1 = 0;
volatile int16_t amp_s2 = 0;
volatile int16_t amp_s3 = 0;

// Sine Look-up Table (64 steps)
const uint16_t SineTable[64] = {
    250,274,299,323,345,367,387,406,423,437,450,460,468,474,477,478,
    476,472,466,457,446,433,418,401,382,362,340,317,293,268,243,218,
    194,169,146,123,102,82,64,48,35,24,15,9,5,4,5,8,
    14,22,33,46,61,78,97,118,140,163,187,212,237,262,287,312
};

/* --- OLED / I2C Driver Functions --- */
const u8 FontTable[] = { 
    0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31, 
    0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03, 
    0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 
    0x3E,0x41,0x41,0x41,0x22, 0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 
    0x3E,0x41,0x49,0x49,0x7A, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 
    0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 
    0x3E,0x41,0x41,0x41,0x3E, 0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 
    0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 
    0x00,0x00,0x00,0x00,0x00, 0x00,0x36,0x36,0x00,0x00, 0x08,0x08,0x08,0x08,0x08 
};

void OLED_SetPixel(u8 x, u8 y, u8 state) { if(x >= 128 || y >= 32) return; if(state) OLED_Buffer[x + (y / 8) * 128] |= (1 << (y % 8)); else OLED_Buffer[x + (y / 8) * 128] &= ~(1 << (y % 8)); }
void OLED_DrawString2x(u8 x, u8 y, char* s) { while(*s) { char c = *s++; u8 idx = 36; if(c >= '0' && c <= '9') idx = c - '0'; else if(c >= 'A' && c <= 'Z') idx = c - 'A' + 10; else if(c == ':') idx = 37; for(u8 i=0; i<5; i++) { u8 b = FontTable[idx*5 + i]; for(u8 j=0; j<7; j++) if(b & (1<<j)) { OLED_SetPixel(x+i*2, y+j*2, 1); OLED_SetPixel(x+i*2+1, y+j*2, 1); OLED_SetPixel(x+i*2, y+j*2+1, 1); OLED_SetPixel(x+i*2+1, y+j*2+1, 1); } } x += 12; } }

#define I2C_SCL_H GPIO_WriteBit(OLED_GPIO_PORT, OLED_SCL_PIN, Bit_SET)
#define I2C_SCL_L GPIO_WriteBit(OLED_GPIO_PORT, OLED_SCL_PIN, Bit_RESET)
#define I2C_SDA_H GPIO_WriteBit(OLED_GPIO_PORT, OLED_SDA_PIN, Bit_SET)
#define I2C_SDA_L GPIO_WriteBit(OLED_GPIO_PORT, OLED_SDA_PIN, Bit_RESET)

void I2C_Delay(void) { volatile int i = 10; while(i--); } 
void I2C_Start(void) { I2C_SDA_H; I2C_SCL_H; I2C_Delay(); I2C_SDA_L; I2C_Delay(); I2C_SCL_L; }
void I2C_Stop(void) { I2C_SDA_L; I2C_SCL_H; I2C_Delay(); I2C_SDA_H; I2C_Delay(); }
void I2C_SendByte(u8 byte) { for(u8 i=0; i<8; i++) { if(byte & 0x80) I2C_SDA_H; else I2C_SDA_L; I2C_Delay(); I2C_SCL_H; I2C_Delay(); I2C_SCL_L; byte <<= 1; } I2C_SDA_H; I2C_Delay(); I2C_SCL_H; I2C_Delay(); I2C_SCL_L; }
void OLED_WriteCmd(u8 cmd) { I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x00); I2C_SendByte(cmd); I2C_Stop(); }

void OLED_Init(void) { 
    Delay_Ms(100); 
    OLED_WriteCmd(0xAE); OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80); 
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x1F); OLED_WriteCmd(0x40); 
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14); OLED_WriteCmd(0x20); 
    OLED_WriteCmd(0x00); OLED_WriteCmd(0xA1); OLED_WriteCmd(0xC8); 
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x02); OLED_WriteCmd(0xAF); 
}

void OLED_Refresh(void) { 
    OLED_WriteCmd(0x21); OLED_WriteCmd(0); OLED_WriteCmd(127); 
    OLED_WriteCmd(0x22); OLED_WriteCmd(0); OLED_WriteCmd(3); 
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x40); 
    for(int i=0; i<512; i++) I2C_SendByte(OLED_Buffer[i]); 
    I2C_Stop(); 
}

/* ------------------------------------------------------------------
 * Interrupt Handler: TIM2
 * Function: Updates PWM duty cycle to generate 400Hz 3-Phase Sine Wave
 * ------------------------------------------------------------------ */
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        
        // 1. Sync Signal (Pin 1) for Oscilloscope Trigger
        // High at start of wave, Low at middle.
        if (wave_idx == 0) GPIO_WriteBit(GPIOD, SYNC_PIN, Bit_SET);
        else if (wave_idx == 32) GPIO_WriteBit(GPIOD, SYNC_PIN, Bit_RESET);

        // 2. 3-Phase Calculation (120 degree shift)
        // Ref Phase : 0 deg
        // S1 Phase  : 0 deg (Same as Ref)
        // S2 Phase  : +120 deg (+21 steps)
        // S3 Phase  : +240 deg (+43 steps)
        
        int16_t c1 = (int16_t)SineTable[wave_idx] - 250;
        int16_t c2 = (int16_t)SineTable[(wave_idx + 21) & 63] - 250;
        int16_t c3 = (int16_t)SineTable[(wave_idx + 43) & 63] - 250;
        
        uint16_t intval_ref;
        uint16_t intval_s1;
        uint16_t intval_s2;
        uint16_t intval_s3;

        intval_ref = 250 + c1;                       // Reference
        intval_s1  = 250 + ((c1 * amp_s1) >> 8);     // S1 (Modulated)
        intval_s2  = 250 + ((c2 * amp_s2) >> 8);     // S2 (Modulated)
        intval_s3  = 250 + ((c3 * amp_s3) >> 8);     // S3 (Modulated)

        // Update PWM Hardware
        TIM1->CH1CVR = intval_ref; 
        TIM1->CH2CVR = intval_s1;  
        TIM1->CH3CVR = intval_s2;  
        TIM1->CH4CVR = intval_s3;  
        
        // Loop 0-63
        wave_idx = (wave_idx + 1) & 63;
        
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

/* ------------------------------------------------------------------
 * Hardware Setup
 * Function: Initialize GPIO, ADC, TIM1(PWM), TIM2(Interrupt)
 * ------------------------------------------------------------------ */
void Setup_Hardware(void) {
    SystemInit(); // 24MHz System Clock
    
    // Enable Clocks
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_GPIOC | RCC_PB2Periph_GPIOD | RCC_PB2Periph_ADC1 | RCC_PB2Periph_TIM1 | RCC_PB2Periph_AFIO, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);

    GPIO_InitTypeDef g = {0};

    // LED & OLED
    g.GPIO_Pin = LED_PIN; g.GPIO_Mode = GPIO_Mode_Out_PP; g.GPIO_Speed = GPIO_Speed_30MHz; GPIO_Init(GPIOC, &g);
    g.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN; g.GPIO_Mode = GPIO_Mode_Out_OD; GPIO_Init(GPIOC, &g);

    // ADC In
    g.GPIO_Pin = GPIO_Pin_2; g.GPIO_Mode = GPIO_Mode_AIN; GPIO_Init(GPIOA, &g);

    // Sync Out
    g.GPIO_Pin = SYNC_PIN; g.GPIO_Mode = GPIO_Mode_Out_PP; g.GPIO_Speed = GPIO_Speed_30MHz; GPIO_Init(GPIOD, &g);

    // PWM Outs
    g.GPIO_Pin = GPIO_Pin_2; g.GPIO_Mode = GPIO_Mode_AF_PP; g.GPIO_Speed = GPIO_Speed_30MHz; GPIO_Init(GPIOD, &g); // PD2
    g.GPIO_Pin = GPIO_Pin_1; GPIO_Init(GPIOA, &g); // PA1
    g.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4; GPIO_Init(GPIOC, &g); // PC3, PC4

    // ADC Setup
    ADC_InitTypeDef a = {ADC_Mode_Independent, DISABLE, ENABLE, ADC_ExternalTrigConv_None, ADC_DataAlign_Right, 1};
    ADC_Init(ADC1, &a); 
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_CyclesMode7); 
    ADC_Cmd(ADC1, ENABLE); 
    
    // TIM1 Setup (Carrier PWM)
    TIM_TimeBaseInitTypeDef t = {0};
    t.TIM_Period = PWM_PERIOD - 1;
    t.TIM_Prescaler = 0; 
    t.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &t);

    TIM_OCInitTypeDef o = {0};
    o.TIM_OCMode = TIM_OCMode_PWM1; 
    o.TIM_OutputState = TIM_OutputState_Enable;
    o.TIM_Pulse = 250; 
    o.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM1, &o); 
    TIM_OC2Init(TIM1, &o); 
    TIM_OC3Init(TIM1, &o); 
    TIM_OC4Init(TIM1, &o);
    TIM_CtrlPWMOutputs(TIM1, ENABLE); 
    TIM_Cmd(TIM1, ENABLE);

    // TIM2 Setup (400Hz Wave Update)
    // 24MHz / 937 = ~25.6kHz interrupt -> 400Hz wave (64 steps)
    t.TIM_Period = 937; 
    t.TIM_Prescaler = 0;
    TIM_TimeBaseInit(TIM2, &t);
    NVIC_EnableIRQ(TIM2_IRQn); 
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); 
    TIM_Cmd(TIM2, ENABLE);
}

/* ------------------------------------------------------------------
 * Main Function
 * ------------------------------------------------------------------ */
int main(void) {
    Setup_Hardware(); 
    Delay_Init(); 
    OLED_Init();
    
    char buf[16]; 
    u8 led = 0;
    
    // Adaptive Filter State
    float adc_smooth = 0.0f;
    int first_run = 1;
    
    while(1) {
        GPIO_WriteBit(GPIOC, LED_PIN, led ? Bit_SET : Bit_RESET); led = !led;
        
        // --- 1. Oversampling (16x Average) ---
        uint32_t adc_sum = 0;
        for(int i=0; i<16; i++) {
            ADC_SoftwareStartConvCmd(ADC1, ENABLE);
            while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
            adc_sum += ADC_GetConversionValue(ADC1);
            for(volatile int k=0; k<50; k++); // short wait
        }
        uint16_t v_raw_avg = (uint16_t)(adc_sum / 16);

        // --- 2. Adaptive Logic ---
        // Detect movement magnitude
        int diff = 0;
        if (!first_run) {
            diff = abs((int)v_raw_avg - (int)adc_smooth);
        }

        // Adjust Filter Strength (Alpha)
        float alpha;
        if (first_run) {
            alpha = 1.0f; 
            first_run = 0;
        } else if (diff > 50) { 
            alpha = 0.6f; // Moving: Fast response
        } else {
            alpha = 0.05f; // Stopped: Strong smoothing
        }

        // Apply EMA Filter
        adc_smooth = (adc_smooth * (1.0f - alpha)) + ((float)v_raw_avg * alpha);
        
        uint16_t v = (uint16_t)adc_smooth;
        
        // Map to 360 degrees
        int deg = (int)(((float)v / 4095.0f) * 360.0f) % 360;
        
        // --- 3. Hysteresis Update ---
        // Only update logic/display if angle changed > 1 degree
        if (last_deg == -999 || abs(deg - last_deg) > 1) {
            
            float rad = (float)deg * PI / 180.0f;
            
            // Synchro Signal Calculation
            amp_s1 = (int16_t)(sinf(rad) * 255.0f);
            amp_s2 = (int16_t)(sinf(rad + 2.0944f) * 255.0f); // +120
            amp_s3 = (int16_t)(sinf(rad + 4.1888f) * 255.0f); // +240
            
            // OLED Update
            memset(OLED_Buffer, 0, 512);
            sprintf(buf, "%03d DEG", deg); 
            OLED_DrawString2x(20, 8, buf); 
            
            // Bar Graph
            int bar_width = (int)((uint32_t)v * 120 / 4095);
            for(int i=0; i<bar_width; i++) {
                OLED_SetPixel(4 + i, 27, 1);
                OLED_SetPixel(4 + i, 28, 1);
                OLED_SetPixel(4 + i, 29, 1);
                OLED_SetPixel(4 + i, 30, 1);
            }

            OLED_Refresh(); 
            last_deg = deg;
        }
        
        Delay_Ms(100); 
    }
}
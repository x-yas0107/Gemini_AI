/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : yas & Gemini
 * Version            : V9.1.0 (Fireworks + Calibration)
 * Description        : CH32V006 Voltmeter
 *********************************************************************************/

#include "debug.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* =========================================================================
 * ★★★ 設定エリア (ここをいじって調整！) ★★★
 * ========================================================================= */
#define VDD_VOLTAGE 3.30f      
#define R1_VAL      1000000.0f // 1MΩ
#define R2_VAL      15000.0f   // 15kΩ

// ★補正係数 (トリム)
// OLEDの表示がズレている場合、ここで調整します。
// 例: 表示が低い場合は 1.02f とかに増やす。高い場合は 0.98f とかに減らす。
// 計算式: (テスターの正しい値) ÷ (OLEDの表示値) = ここに入れる数字
#define CORRECTION_FACTOR  1.000f 

// I2C Pin Settings (TSSOP20: PC2=SCL, PC1=SDA)
#define OLED_SCL_PIN   GPIO_Pin_2
#define OLED_SDA_PIN   GPIO_Pin_1
#define OLED_GPIO_PORT GPIOC
#define OLED_RCC_PORT  RCC_PB2Periph_GPIOC

/* =========================================================================
 * OLED ドライバ
 * ========================================================================= */
u8 OLED_Buffer[128 * 32 / 8];

#define I2C_SCL_H  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SCL_PIN, Bit_SET)
#define I2C_SCL_L  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SCL_PIN, Bit_RESET)
#define I2C_SDA_H  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SDA_PIN, Bit_SET)
#define I2C_SDA_L  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SDA_PIN, Bit_RESET)

void I2C_Delay(void) { volatile int i = 5; while(i--); }

void I2C_Start(void) {
    I2C_SDA_H; I2C_SCL_H; I2C_Delay();
    I2C_SDA_L; I2C_Delay(); I2C_SCL_L;
}

void I2C_Stop(void) {
    I2C_SDA_L; I2C_SCL_H; I2C_Delay();
    I2C_SDA_H; I2C_Delay();
}

void I2C_SendByte(u8 byte) {
    for(u8 i=0; i<8; i++) {
        if(byte & 0x80) I2C_SDA_H; else I2C_SDA_L;
        I2C_Delay(); I2C_SCL_H; I2C_Delay(); I2C_SCL_L;
        byte <<= 1;
    }
    I2C_SDA_H; I2C_Delay(); I2C_SCL_H; I2C_Delay(); I2C_SCL_L; 
}

void OLED_WriteCmd(u8 cmd) {
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x00); I2C_SendByte(cmd); I2C_Stop();
}

void OLED_WriteData(u8 data) {
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x40); I2C_SendByte(data); I2C_Stop();
}

void OLED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_PB2PeriphClockCmd(OLED_RCC_PORT, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz; 
    GPIO_Init(OLED_GPIO_PORT, &GPIO_InitStructure);
    
    I2C_SDA_H; I2C_SCL_H; Delay_Ms(100);

    OLED_WriteCmd(0xAE); OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x1F); OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40); OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x00); OLED_WriteCmd(0xA1);
    OLED_WriteCmd(0xC8); OLED_WriteCmd(0xDA); OLED_WriteCmd(0x02);
    OLED_WriteCmd(0x81); OLED_WriteCmd(0x8F); OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x40); OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xA6); OLED_WriteCmd(0xAF);
}

void OLED_Clear(void) {
    for(int i=0; i<sizeof(OLED_Buffer); i++) OLED_Buffer[i] = 0x00;
}

void OLED_Refresh(void) {
    OLED_WriteCmd(0x21); OLED_WriteCmd(0); OLED_WriteCmd(127);
    OLED_WriteCmd(0x22); OLED_WriteCmd(0); OLED_WriteCmd(3);
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x40);
    for(int i=0; i<sizeof(OLED_Buffer); i++) I2C_SendByte(OLED_Buffer[i]);
    I2C_Stop();
}

void OLED_DrawPixel(u8 x, u8 y) {
    if(x >= 128 || y >= 32) return;
    OLED_Buffer[x + (y / 8) * 128] |= (1 << (y % 8));
}

void OLED_DrawLine_Raw(u8 x1, u8 y1, u8 x2, u8 y2) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    while(1) {
        OLED_DrawPixel(x1, y1);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

/* =========================================================================
 * 描画ロジック
 * ========================================================================= */

void OLED_DrawPixel_Skewed(int x, int y) {
    int offset = (32 - y) / 3; 
    if (x + offset >= 128) return;
    OLED_DrawPixel(x + offset, y);
}

void Draw_Skewed_Rect(u8 x, u8 y, u8 w, u8 h) {
    for(u8 i=0; i<w; i++) {
        for(u8 j=0; j<h; j++) {
            OLED_DrawPixel_Skewed(x+i, y+j);
        }
    }
}

// ミニサイズ V
void Draw_Unit_V_Mini(u8 x, u8 y) {
    OLED_DrawLine_Raw(x, 16, x+4, 31);
    OLED_DrawLine_Raw(x+1, 16, x+5, 31);
    OLED_DrawLine_Raw(x+8, 16, x+4, 31);
    OLED_DrawLine_Raw(x+9, 16, x+5, 31);
}

/* =========================================================================
 * 7セグ描画
 * ========================================================================= */

void Draw_Digital_Digit(u8 x, u8 y, char c) {
    if (c < '0' || c > '9') return; 
    u8 num = c - '0';
    const u8 seg_map[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    u8 pat = seg_map[num];

    u8 w = 14; 
    u8 h = 32;  
    u8 t = 3;   
    u8 g = 3;   

    if(pat & 0x01) Draw_Skewed_Rect(x+g, y, w-2*g, t);           
    if(pat & 0x02) Draw_Skewed_Rect(x+w-t, y+g, t, (h/2)-2*g);   
    if(pat & 0x04) Draw_Skewed_Rect(x+w-t, y+(h/2)+g, t, (h/2)-2*g); 
    if(pat & 0x08) Draw_Skewed_Rect(x+g, y+h-t, w-2*g, t);       
    if(pat & 0x10) Draw_Skewed_Rect(x, y+(h/2)+g, t, (h/2)-2*g); 
    if(pat & 0x20) Draw_Skewed_Rect(x, y+g, t, (h/2)-2*g);       
    if(pat & 0x40) Draw_Skewed_Rect(x+g, y+(h/2)-(t/2), w-2*g, t); 
}

void Display_Voltage(float volt) {
    char buf[16];
    OLED_Clear();
    
    int v_int = (int)volt;
    int v_dec = 0;
    
    if (volt < 10.0f) {
        v_dec = (int)((volt - v_int) * 10000);
        sprintf(buf, "%d.%04d", v_int, v_dec);
    } else if (volt < 100.0f) {
        v_dec = (int)((volt - v_int) * 1000);
        sprintf(buf, "%d.%03d", v_int, v_dec);
    } else {
        v_dec = (int)((volt - v_int) * 100);
        sprintf(buf, "%d.%02d", v_int, v_dec);
    }

    int cursor_x = 0; 
    int cursor_y = 0; 

    for(int i=0; buf[i] != '\0'; i++) {
        if(buf[i] >= '0' && buf[i] <= '9') {
            Draw_Digital_Digit(cursor_x, cursor_y, buf[i]);
            cursor_x += 19; 
        } else if (buf[i] == '.') {
            Draw_Skewed_Rect(cursor_x + 2, 28, 4, 4);
            cursor_x += 9;
        }
    }
    
    if (cursor_x < 118) {
        Draw_Unit_V_Mini(cursor_x + 4, 0);
    }
    
    OLED_Refresh();
}

void ADC_Function_Init(void) {
    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_Cmd(ADC1, ENABLE);
    Delay_Us(100);
}

u16 Get_ADC_Val(u8 ch) {
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_CyclesMode7);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

float Measure_Voltage(void) {
    u32 sum = 0;
    
    // 4096回平均 (安定化)
    for(int i=0; i<4096; i++) { 
        sum += Get_ADC_Val(ADC_Channel_0); 
    }
    
    float adc_val = (float)sum / 4096.0f;
    
    float pin_voltage = adc_val * (VDD_VOLTAGE / 4095.0f);
    float actual_voltage = pin_voltage * ((R1_VAL + R2_VAL) / R2_VAL);
    
    // ★ここで補正係数を掛ける！
    actual_voltage *= CORRECTION_FACTOR;

    if(actual_voltage < 0.1f) actual_voltage = 0.0f;
    return actual_voltage;
}

int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    
    ADC_Function_Init();
    OLED_Init(); 
    OLED_Clear();

    while(1)
    {
        float v = Measure_Voltage();
        Display_Voltage(v);
        Delay_Ms(10); 
    }
}
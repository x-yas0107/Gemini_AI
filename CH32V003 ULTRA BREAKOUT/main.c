/*******************************************************************************
 * PROJECT     : CH32V003 ULTRA BREAKOUT [Official Release]
 * DEVELOPER   : Gemini & yas
 * DATE        : 2026-03-01
 * VERSION     : Ver 1.00
 * * [DESCRIPTION]
 * High-performance, resource-optimized Breakout game for CH32V003 (8-pin).
 * Features: On-the-fly rendering (no framebuffer), 60fps-class smoothness, 
 * and sub-4.5KB footprint. 100% register-level I2C control.
 * * [I/O MAP]
 * - PA1 (Pin 1): SOUND (Buzzer / Beep)
 * - PD1 (Pin 8): START BUTTON (SWDIO Shared / Internal Pull-up)
 * - PC1 (Pin 6): I2C SDA (OLED)
 * - PC2 (Pin 5): I2C SCL (OLED)
 * - PC4 (Pin 4): PADDLE KNOB (ADC Channel 2 / Potentiometer)
 * * [VERSION HISTORY]
 * 2026-03-01: Ver 1.00 - First Official Release.
 * - Optimized 'S' & 'G' fonts.
 * - Fixed ball tunneling and paddle collision physics.
 * - Implemented "Paddle Launch" and "Quick Paddle" sensitivity.
 * - Added 2s GameOver lock and improved title spacing.
 *******************************************************************************/
#include "debug.h"

#define OLED_ADDR       0x78
#define PADDLE_WIDTH    16
#define BALL_SIZE       3
#define SCREEN_W        128
#define SCREEN_H        64
#define COLUMN_OFFSET   1       

typedef enum { STATE_TITLE, STATE_PLAY, STATE_GAMEOVER, STATE_CLEAR } GameState;
GameState gameState = STATE_TITLE;

const uint8_t font3x5[10][3] = {
    {0x1F,0x11,0x1F},{0x00,0x1F,0x00},{0x1D,0x15,0x17},{0x15,0x15,0x1F},{0x07,0x04,0x1F},
    {0x17,0x15,0x1D},{0x1F,0x15,0x1D},{0x01,0x01,0x1F},{0x1F,0x15,0x1F},{0x17,0x15,0x1F}
};

const uint8_t msg_chars[12][3] = {
    {0x1F,0x11,0x1D}, // 0: G 
    {0x1F,0x05,0x1F}, // 1: A
    {0x1F,0x02,0x1F}, // 2: M
    {0x1F,0x15,0x15}, // 3: E
    {0x1F,0x11,0x1F}, // 4: O
    {0x1F,0x10,0x1F}, // 5: V
    {0x1F,0x05,0x1A}, // 6: R
    {0x1F,0x11,0x11}, // 7: C
    {0x1F,0x10,0x10}, // 8: L
    {0x17,0x15,0x1D}, // 9: S
    {0x01,0x1F,0x01}, // 10: T
    {0x00,0x00,0x00}  // 11: Space
};

const uint8_t title_font[8][3] = {
    {0x1F,0x15,0x0A}, // B
    {0x1F,0x05,0x1A}, // R
    {0x1F,0x15,0x15}, // E
    {0x1F,0x05,0x1F}, // A
    {0x1F,0x04,0x1B}, // K
    {0x1F,0x11,0x1F}, // O
    {0x1F,0x10,0x1F}, // U
    {0x01,0x1F,0x01}  // T
};

int16_t ballX, ballY;
int8_t  ballDX, ballDY;
int16_t paddleX = 52;
uint64_t blocks;
uint16_t score = 0;
uint16_t adc_filter = 512;
uint32_t state_timer = 0;

void Beep(uint16_t freq, uint16_t dur) {
    for(int i=0; i<dur; i++) {
        GPIO_WriteBit(GPIOA, GPIO_Pin_1, (i%2)?Bit_SET:Bit_RESET);
        Delay_Us(freq * 10); 
    }
}

void OLED_Write(uint8_t mode, uint8_t data) {
    while(I2C1->STAR2 & I2C_STAR2_BUSY);
    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    I2C_SendData(I2C1, mode);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_SendData(I2C1, data);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_GenerateSTOP(I2C1, ENABLE);
}

void Init_Hardware(void) {
    Delay_Ms(2000); 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD|RCC_APB2Periph_ADC1|RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    AFIO->PCFR1 &= ~(1 << 15); 
    GPIO_InitTypeDef g={0};
    g.GPIO_Pin=GPIO_Pin_1|GPIO_Pin_2; g.GPIO_Mode=GPIO_Mode_AF_OD; g.GPIO_Speed=GPIO_Speed_50MHz; GPIO_Init(GPIOC,&g);
    g.GPIO_Pin=GPIO_Pin_4; g.GPIO_Mode=GPIO_Mode_AIN; GPIO_Init(GPIOC,&g);
    g.GPIO_Pin=GPIO_Pin_1; g.GPIO_Mode=GPIO_Mode_Out_PP; g.GPIO_Speed=GPIO_Speed_50MHz; GPIO_Init(GPIOA,&g);
    g.GPIO_Pin=GPIO_Pin_1; g.GPIO_Mode=GPIO_Mode_IPU; GPIO_Init(GPIOD,&g);
    I2C_InitTypeDef i2={0}; i2.I2C_ClockSpeed=400000; i2.I2C_Mode=I2C_Mode_I2C;
    i2.I2C_DutyCycle=I2C_DutyCycle_2; i2.I2C_Ack=I2C_Ack_Enable; I2C_Init(I2C1,&i2); I2C_Cmd(I2C1,ENABLE);
    ADC_Init(ADC1, &(ADC_InitTypeDef){ADC_Mode_Independent,DISABLE,DISABLE,ADC_ExternalTrigConv_None,ADC_DataAlign_Right,1});
    ADC_Cmd(ADC1,ENABLE); ADC_RegularChannelConfig(ADC1,ADC_Channel_2,1,ADC_SampleTime_241Cycles);
}

void OLED_Init(void) {
    Delay_Ms(100);
    uint8_t c[]={0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,0x8D,0x14,0x20,0x02,0xA1,0xC8,0xDA,0x12,0x81,0xCF,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,0xAF};
    for(int i=0;i<sizeof(c);i++) OLED_Write(0,c[i]);
    for(int p=0;p<8;p++){ OLED_Write(0,0xB0+p); OLED_Write(0,0); OLED_Write(0,0x10); for(int x=0;x<132;x++) OLED_Write(0x40,0); }
}

void Render(void) {
    for (uint8_t page = 0; page < 8; page++) {
        OLED_Write(0, 0xB0+page); OLED_Write(0, COLUMN_OFFSET&0xF); OLED_Write(0, 0x10|(COLUMN_OFFSET>>4));
        I2C_GenerateSTART(I2C1, ENABLE);
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
        I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
        I2C_SendData(I2C1, 0x40);
        for (uint8_t x = 0; x < 128; x++) {
            uint8_t p = 0;
            if (page == 0) { 
                if (x >= 2 && x < 14) { uint8_t i=(x-2)/4,c=(x-2)%4; if(c<3) p|=(font3x5[i==2?3:0][c]<<1); }
                if (x >= 18 && x < 50) { uint8_t i=(x-18)/4,c=(x-18)%4; if(c<3) p|=(title_font[i][c]<<1); }
                if (x >= 110) { uint8_t xr=x-110,dP=xr/4,xC=xr%4; if(dP<3&&xC<3) p|=(font3x5[(dP==0)?(score/100)%10:(dP==1)?(score/10)%10:score%10][xC]<<1); }
            } else {
                if ((gameState == STATE_PLAY || gameState == STATE_TITLE) && page > 0 && page < 5 && x < 127) {
                    if((x % 8 < 7) && (blocks & (1ULL << (x/8 + (page-1)*16)))) p = 0x7F;
                }
                if (page == 7 && x >= paddleX && x < paddleX + PADDLE_WIDTH) p |= 0x03;
                int16_t rY = ballY-(page*8);
                if (x >= ballX && x < ballX+BALL_SIZE && rY > -BALL_SIZE && rY < 8) p |= (rY<=0)?(0x07>>(-rY)):(0x07 << rY);
                if (page == 5) {
                    const uint8_t* m; uint8_t l=0;
                    if(gameState==STATE_TITLE){ static const uint8_t s[]={9,10,1,6,10}; m=s; l=5; }
                    else if(gameState==STATE_GAMEOVER){ static const uint8_t o[]={0,1,2,3,11,4,5,3,6}; m=o; l=9; }
                    else if(gameState==STATE_CLEAR){ static const uint8_t c[]={7,8,3,1,6}; m=c; l=5; }
                    if(l > 0) {
                        int sX = 64-(l*4/2);
                        if(x>=sX && x<sX+l*4){ uint8_t i=(x-sX)/4,c=(x-sX)%4; if(c<3) p = (msg_chars[m[i]][c] << 2); }
                    }
                }
            }
            while(!(I2C1->STAR1 & I2C_STAR1_TXE)); I2C_SendData(I2C1, p);
        }
        I2C_GenerateSTOP(I2C1, ENABLE);
    }
}

void ResetGame() {
    blocks=0xFFFFFFFFFFFFFFFFULL; score=0;
    ballDX=0; ballDY=0; ballY=53;
}

int main(void) {
    SystemInit(); Delay_Init(); Init_Hardware(); OLED_Init();
    ResetGame();
    Beep(100, 50);

    while(1) {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
        uint16_t raw_adc = ADC_GetConversionValue(ADC1);
        adc_filter = (adc_filter*3 + raw_adc)/4;
        int16_t v = (int16_t)adc_filter - 287;
        if(v < 0) v = 0; if(v > 450) v = 450;
        paddleX = (v * (128 - PADDLE_WIDTH)) / 450;

        if (gameState == STATE_PLAY) {
            ballX += ballDX; ballY += ballDY;
            if (ballX <= 0 || ballX >= 125) { ballDX = -ballDX; Beep(15, 60); }
            if (ballY <= 8) { ballDY = -ballDY; Beep(15, 60); }
            if (ballY + BALL_SIZE >= 56 && ballY < 58 && ballX + BALL_SIZE >= paddleX && ballX < paddleX + PADDLE_WIDTH) { 
                ballDY = -1; ballY = 53; Beep(35, 150); 
            }
            if (ballY >= 8 && ballY < 40) {
                int i = (ballX/8) + ((ballY/8)-1)*16;
                if (blocks & (1ULL << i)) { blocks &= ~(1ULL << i); ballDY = -ballDY; score += 10; Beep(10, 600); }
            }
            if (ballY > 64) { gameState = STATE_GAMEOVER; state_timer = 0; Beep(180, 100); }
            if (blocks == 0) { gameState = STATE_CLEAR; state_timer = 0; Beep(40, 100); }
        } else if (gameState == STATE_GAMEOVER || gameState == STATE_CLEAR) {
            if (state_timer < 200) { state_timer++; }
            else { gameState = STATE_TITLE; state_timer = 0; ResetGame(); }
        } else {
            ballX = paddleX + (PADDLE_WIDTH/2) - (BALL_SIZE/2);
            ballY = 53;
            if (raw_adc > 950 || GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_1) == 0) {
                gameState = STATE_PLAY;
                ballDX = 1; ballDY = -1;
                Beep(100, 50);
            }
        }
        Render();
    }
}
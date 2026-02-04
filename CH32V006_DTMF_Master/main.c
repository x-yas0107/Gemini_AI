/*
 * ==============================================================================
 * Project      : CH32V006 DTMF Master
 * Version      : 1.00 (Stable Release)
 * Developer    : yas & Gemini (AI Collaborator)
 * Release Date : 2026/02/04
 * * ------------------------------------------------------------------------------
 * [PHYSICAL PIN CONNECTION / 物理ピン接続番号 (CH32V006F8P6 - SOP20)]
 * ------------------------------------------------------------------------------
 * - 1番ピン  : PD4 (TIM2_CH1 / Audio PWM出力) -> スピーカー/アンプへ
 * - 2番ピン  : PD5 (USART1_TX / シリアル送信) -> PC(USB-Serial) RXへ
 * - 3番ピン  : PD6 (USART1_RX / シリアル受信) -> PC(USB-Serial) TXへ
 * - 7番ピン  : VSS (GND / 接地)
 * - 9番ピン  : VDD (3.3V / 電源)
 * - 11番ピン : PC1 (I2C1_SDA / データライン)  -> OLED SDAへ
 * - 12番ピン : PC2 (I2C1_SCL / クロックライン) -> OLED SCLへ
 * ------------------------------------------------------------------------------
 * * [概要]
 * シリアルターミナル(115200bps)から入力されたキーに応じたDTMF音を生成し、
 * 同時に128x32 OLEDディスプレイへ波形と文字をリアルタイム表示します。
 * * [操作マニュアル]
 * - [0-9, *, #] : 入力バッファ蓄積 ＆ 単音再生 ＆ OLED表示
 * - [ENTER]     : バッファ内の数値を一気に連続送出（ダイヤル発信）
 * - [p]         : メロディ(Secret Song)を演奏
 * - [+] / [-]   : 演奏速度(Tempo)の調整
 * - [*]         : テンポを標準(80ms)にリセット
 * - [SPACE]     : バッファのクリア
 * * [設計上の重要事項]
 * - 文字列回り込み回避：看板文字を X=2 と X=61 に分けて描画し、全幅124pxに抑制。
 * - 視覚的バランス調整：単音表示を左エリア中央(X=11)に配置。
 * ==============================================================================
 */

#include "debug.h"
#include <string.h>

/* --- プロトタイプ宣言 --- */
void OLED_Cmd(uint8_t c);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_Clear(void);
void OLED_ClearArea(uint8_t s_pg, uint8_t e_pg);
void OLED_Init(void);
void OLED_PutC(uint8_t x, uint8_t y, char c, uint8_t ds);
void OLED_Print(uint8_t x, uint8_t y, char* s, uint8_t ds);
void DrawWaveSplit(uint8_t start_x);
void PlayTone(char k, int ms);
void PlaySecretSong(void);
void Periph_Init(void);
void TIM1_Init(void);
void ShowMenu(void);
void ResetDisplay(void);

/* --- フォントデータ (Ver. 1.10 標準版) --- */
const uint8_t Font6x8[][6] = {
    {0x00,0x00,0x00,0x00,0x00,0x00}, // 0: space
    {0x3E,0x51,0x49,0x45,0x3E,0x00}, // 1: '0'
    {0x00,0x42,0x7F,0x40,0x00,0x00}, // 2: '1'
    {0x42,0x61,0x51,0x49,0x46,0x00}, // 3: '2'
    {0x21,0x41,0x45,0x4B,0x31,0x00}, // 4: '3'
    {0x18,0x14,0x12,0x7F,0x10,0x00}, // 5: '4'
    {0x27,0x45,0x45,0x45,0x39,0x00}, // 6: '5'
    {0x3C,0x4A,0x49,0x49,0x30,0x00}, // 7: '6'
    {0x01,0x71,0x09,0x05,0x03,0x00}, // 8: '7'
    {0x36,0x49,0x49,0x49,0x36,0x00}, // 9: '8'
    {0x06,0x49,0x49,0x29,0x1E,0x00}, // 10: '9'
    {0x22,0x14,0x08,0x14,0x22,0x00}, // 11: '*'
    {0x08,0x08,0x3E,0x08,0x08,0x00}, // 12: '#'
    {0x7F,0x41,0x41,0x22,0x1C,0x00}, // 13: 'D'
    {0x01,0x01,0x7F,0x01,0x01,0x00}, // 14: 'T'
    {0x7F,0x02,0x04,0x02,0x7F,0x00}, // 15: 'M'
    {0x7F,0x09,0x09,0x01,0x01,0x00}, // 16: 'F'
    {0x7F,0x49,0x49,0x49,0x41,0x00}, // 17: 'E'
    {0x7C,0x12,0x11,0x12,0x7C,0x00}, // 18: 'A'
    {0x7F,0x09,0x19,0x29,0x46,0x00}, // 19: 'R'
    {0x03,0x04,0x78,0x04,0x03,0x00}  // 20: 'Y'
};

/* --- DTMF シンセサイザ --- */
#define F_INC(f) ((uint32_t)((float)f * 214748.3648f))
const uint32_t R_Incs[] = {F_INC(697), F_INC(770), F_INC(852), F_INC(941)};
const uint32_t C_Incs[] = {F_INC(1209), F_INC(1336), F_INC(1477)};
const uint8_t SineTable[] = { 128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,215, 218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,245,246,247,248,249,250,251,251,252,253,253,254,254,254,254,254, 255,254,254,254,254,254,254,253,253,252,251,251,250,249,248,247,246,245,244,243,241,240,238,237,235,234,232,230,228,226,224,222, 220,218,215,213,211,209,206,204,201,199,196,193,191,188,185,182,179,176,174,171,168,165,162,159,156,152,149,146,143,140,137,134, 131,128,124,121,118,115,112,109,106,103,99,96,93,90,87,84,81,79,76,73,70,67,64,62,59,56,54,51,49,46,44,42,40,37,35,33,31,29,27,25, 23,21,20,18,17,15,14,12,11,10,9,8,7,6,5,4,4,3,2,2,1,1,1,1,1,0,1,1,1,1,1,1,2,2,3,4,4,5,6,7,8,9,10,11,12,14,15,17,18,20,21,23,25,27, 29,31,33,35,37,40,42,44,46,49,51,54,56,59,62,64,67,70,73,76,79,81,84,87,90,93,96,99,103,106,109,112,115,118,121,124 };

volatile uint32_t p_acc_r=0, p_acc_c=0, p_inc_r=0, p_inc_c=0;
volatile uint8_t current_val = 128;
char dial_buf[21] = {0};
uint8_t dial_pos = 0;
uint8_t wave_data[93]; 
int tempo_ms = 80;

/* --- OLED コア制御 (Ver. 1.10) --- */
void OLED_Cmd(uint8_t c) {
    I2C1->CTLR1 |= I2C_CTLR1_START; while(!(I2C1->STAR1 & I2C_STAR1_SB));
    I2C1->DATAR = 0x78; while(!(I2C1->STAR1 & I2C_STAR1_ADDR)); (void)I2C1->STAR2;
    I2C1->DATAR = 0x00; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
    I2C1->DATAR = c; while(!(I2C1->STAR1 & I2C_STAR1_TXE)); I2C1->CTLR1 |= I2C_CTLR1_STOP;
}
void OLED_SetPos(uint8_t x, uint8_t y) { OLED_Cmd(0xB0 + y); OLED_Cmd(((x & 0xF0) >> 4) | 0x10); OLED_Cmd(x & 0x0F); }
void OLED_ClearArea(uint8_t s_pg, uint8_t e_pg) {
    for(uint8_t i=s_pg; i<=e_pg; i++) {
        OLED_SetPos(0, i); I2C1->CTLR1 |= I2C_CTLR1_START; while(!(I2C1->STAR1 & I2C_STAR1_SB));
        I2C1->DATAR = 0x78; while(!(I2C1->STAR1 & I2C_STAR1_ADDR)); (void)I2C1->STAR2;
        I2C1->DATAR = 0x40; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
        for(uint8_t j=0; j<128; j++) { I2C1->DATAR = 0x00; while(!(I2C1->STAR1 & I2C_STAR1_TXE)); }
        I2C1->CTLR1 |= I2C_CTLR1_STOP;
    }
}
void OLED_Clear() { OLED_ClearArea(0, 3); }
void OLED_Init() {
    uint8_t cmds[] = {0xAE,0xD5,0x80,0xA8,0x1F,0xD3,0x00,0x40,0x8D,0x14,0xA1,0xC8,0xDA,0x02,0x81,0x7F,0xA4,0xA6,0xAF};
    for(int i=0; i<sizeof(cmds); i++) OLED_Cmd(cmds[i]);
}

void OLED_PutC(uint8_t x, uint8_t y, char c, uint8_t ds) {
    uint8_t idx = 0;
    if(c >= '0' && c <= '9') idx = (c - '0') + 1;
    else switch(c){
        case '*': idx=11; break; case '#': idx=12; break; case 'D': idx=13; break;
        case 'T': idx=14; break; case 'M': idx=15; break; case 'F': idx=16; break;
        case 'E': idx=17; break; case 'A': idx=18; break; case 'R': idx=19; break;
        case 'Y': idx=20; break;
    }
    if(ds) {
        for(int p=0; p<2; p++) {
            OLED_SetPos(x, y + p); I2C1->CTLR1 |= I2C_CTLR1_START; while(!(I2C1->STAR1 & I2C_STAR1_SB));
            I2C1->DATAR = 0x78; while(!(I2C1->STAR1 & I2C_STAR1_ADDR)); (void)I2C1->STAR2;
            I2C1->DATAR = 0x40; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
            for(int i=0; i<6; i++){
                uint8_t b = Font6x8[idx][i]; uint8_t exp = 0;
                for(int j=0; j<4; j++) { if((b >> (j + p*4)) & 1) exp |= (3 << (j*2)); }
                I2C1->DATAR = exp; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
                I2C1->DATAR = exp; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
            }
            I2C1->CTLR1 |= I2C_CTLR1_STOP;
        }
    } else {
        OLED_SetPos(x, y); I2C1->CTLR1 |= I2C_CTLR1_START; while(!(I2C1->STAR1 & I2C_STAR1_SB));
        I2C1->DATAR = 0x78; while(!(I2C1->STAR1 & I2C_STAR1_ADDR)); (void)I2C1->STAR2;
        I2C1->DATAR = 0x40; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
        for(int i=0; i<6; i++) { I2C1->DATAR = Font6x8[idx][i]; while(!(I2C1->STAR1 & I2C_STAR1_TXE)); }
        I2C1->CTLR1 |= I2C_CTLR1_STOP;
    }
}
void OLED_Print(uint8_t x, uint8_t y, char* s, uint8_t ds) { while(*s) { OLED_PutC(x, y, *s++, ds); x += (ds ? 13 : 7); } }

/* --- 表示リフレッシュ (Ver. 1.12 物理補正版) --- */
void ResetDisplay() {
    OLED_Clear(); 
    OLED_Print(2, 1, "DTMF", 1);     // X=2
    OLED_Print(61, 1, "READY", 1);   // X=61 (gap=7px)
}

/* --- 波形描画 (Ver. 1.10 連続線ロジック) --- */
void DrawWaveSplit(uint8_t start_x) {
    uint8_t last_y = (wave_data[0] >> 3);
    if(last_y < 1) last_y = 1; if(last_y > 30) last_y = 30;
    for(int i=0; i<93; i++) {
        uint8_t curr_y = (wave_data[i] >> 3);
        if(curr_y < 1) curr_y = 1; if(curr_y > 30) curr_y = 30;
        uint8_t min_y = (curr_y < last_y) ? curr_y : last_y;
        uint8_t max_y = (curr_y > last_y) ? curr_y : last_y;
        for(uint8_t y = min_y; y <= max_y; y++) {
            OLED_SetPos(start_x + i, y / 8);
            I2C1->CTLR1 |= I2C_CTLR1_START; while(!(I2C1->STAR1 & I2C_STAR1_SB));
            I2C1->DATAR = 0x78; while(!(I2C1->STAR1 & I2C_STAR1_ADDR)); (void)I2C1->STAR2;
            I2C1->DATAR = 0x40; while(!(I2C1->STAR1 & I2C_STAR1_TXE));
            I2C1->DATAR = (1 << (y % 8)); while(!(I2C1->STAR1 & I2C_STAR1_TXE));
            I2C1->CTLR1 |= I2C_CTLR1_STOP;
        }
        last_y = curr_y;
    }
}

void PlayTone(char k, int ms) {
    int r=-1, c=-1;
    if(k>='1'&&k<='3'){r=0;c=k-'1';} else if(k>='4'&&k<='6'){r=1;c=k-'4';}
    else if(k>='7'&&k<='9'){r=2;c=k-'7';} else if(k=='*'){r=3;c=0;}
    else if(k=='0'){r=3;c=1;} else if(k=='#'){r=3;c=2;}
    if(r!=-1){
        p_inc_r=R_Incs[r]; p_inc_c=C_Incs[c];
        for(int j=0; j<93; j++) { wave_data[j] = current_val; Delay_Us(40); }
        OLED_Clear(); 
        OLED_PutC(11, 1, k, 1); // X=11
        DrawWaveSplit(35); 
        Delay_Ms(ms); p_inc_r=0; p_inc_c=0; 
        Delay_Ms(20); 
    } else { Delay_Ms(ms); }
}

void PlaySecretSong() {
    char notes[] = "3212333 222 366 3212333322321";
    for(int i=0; i<strlen(notes); i++) PlayTone(notes[i], tempo_ms);
}

/* --- ダッシュボード (Ver. 1.10 完全維持) --- */
void ShowMenu(void) {
    printf("\033[2J\033[H");
    printf("========================================\r\n");
    printf("   CH32V006 DTMF MASTER COMMANDER\r\n");
    printf("========================================\r\n");
    printf(" [0-9,*,#] : Input digits to buffer\r\n");
    printf(" [ENTER]    : Dial all from buffer\r\n");
    printf(" [p]        : Play Secret Song\r\n");
    printf(" [+]        : Increase Tempo (Faster)\r\n");
    printf(" [-]        : Decrease Tempo (Slower)\r\n");
    printf(" [*]        : Reset Tempo to 80ms\r\n");
    printf(" [SPACE]    : Clear Buffer\r\n");
    printf("----------------------------------------\r\n");
    printf(" STATUS:\r\n");
    printf("  Tempo  : %d ms\r\n", tempo_ms);
    printf("  Buffer : [%s]\r\n", dial_buf);
    printf("----------------------------------------\r\n");
    printf(" COMMAND > ");
}

/* --- 周辺機器初期化 --- */
void Periph_Init(void) {
    GPIO_InitTypeDef G={0}; USART_InitTypeDef U={0}; I2C_InitTypeDef I={0}; TIM_TimeBaseInitTypeDef T={0}; TIM_OCInitTypeDef O={0};
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOD|RCC_PB2Periph_GPIOC|RCC_PB2Periph_USART1|RCC_PB2Periph_AFIO, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2|RCC_PB1Periph_I2C1, ENABLE);
    G.GPIO_Pin=GPIO_Pin_4|GPIO_Pin_5; G.GPIO_Mode=GPIO_Mode_AF_PP; G.GPIO_Speed=GPIO_Speed_30MHz; GPIO_Init(GPIOD, &G);
    G.GPIO_Pin=GPIO_Pin_6; G.GPIO_Mode=GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOD, &G);
    U.USART_BaudRate=115200; U.USART_Mode=USART_Mode_Tx|USART_Mode_Rx; USART_Init(USART1, &U); USART_Cmd(USART1, ENABLE);
    G.GPIO_Pin=GPIO_Pin_1|GPIO_Pin_2; G.GPIO_Mode=GPIO_Mode_AF_OD; GPIO_Init(GPIOC, &G);
    I.I2C_Mode=I2C_Mode_I2C; I.I2C_ClockSpeed=400000; I2C_Init(I2C1, &I); I2C_Cmd(I2C1, ENABLE);
    T.TIM_Period=255; T.TIM_Prescaler=0; TIM_TimeBaseInit(TIM2, &T);
    O.TIM_OCMode=TIM_OCMode_PWM1; O.TIM_OutputState=TIM_OutputState_Enable; O.TIM_Pulse=128; TIM_OC1Init(TIM2, &O); TIM_Cmd(TIM2, ENABLE);
}

void TIM1_Init(void) {
    TIM_TimeBaseInitTypeDef T={0}; RCC_PB2PeriphClockCmd(RCC_PB2Periph_TIM1, ENABLE);
    T.TIM_Period=2400-1; TIM_TimeBaseInit(TIM1, &T); TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM1_UP_IRQn); TIM_Cmd(TIM1, ENABLE);
}

void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void) {
    if(TIM_GetITStatus(TIM1, TIM_IT_Update)!=RESET){
        if(p_inc_r>0){
            p_acc_r+=p_inc_r; p_acc_c+=p_inc_c;
            current_val = (SineTable[(p_acc_r>>24)&0xFF]+SineTable[(p_acc_c>>24)&0xFF])>>1;
            TIM2->CH1CVR=current_val;
        }else{ TIM2->CH1CVR=128; }
        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
    }
}

int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1); Delay_Init(); Periph_Init(); TIM1_Init();
    OLED_Init(); ResetDisplay(); ShowMenu();
    while(1) {
        if(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
            char c = USART_ReceiveData(USART1);
            if(c == 'p') { PlaySecretSong(); Delay_Ms(1000); ResetDisplay(); ShowMenu(); }
            else if(c == '+') { tempo_ms -= 10; if(tempo_ms < 20) tempo_ms = 20; ShowMenu(); }
            else if(c == '-') { tempo_ms += 10; if(tempo_ms > 500) tempo_ms = 500; ShowMenu(); }
            else if(c == '*') { tempo_ms = 80; ShowMenu(); }
            else if(c == '\r' || c == '\n') {
                for(int i=0; i<dial_pos; i++) PlayTone(dial_buf[i], tempo_ms);
                Delay_Ms(1000); dial_pos = 0; dial_buf[0] = 0; ResetDisplay(); ShowMenu();
            } else if(c == ' ') { dial_pos = 0; dial_buf[0] = 0; ResetDisplay(); ShowMenu(); }
            else if(dial_pos < 18 && ((c>='0'&&c<='9')||c=='*'||c=='#')) {
                dial_buf[dial_pos++] = c; dial_buf[dial_pos] = 0;
                OLED_Clear(); 
                OLED_PutC(11, 1, c, 1); // X=11
                OLED_Print(0, 3, dial_buf, 0); 
                ShowMenu();
            }
        }
    }
}
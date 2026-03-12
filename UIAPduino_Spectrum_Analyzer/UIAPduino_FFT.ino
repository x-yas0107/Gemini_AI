/*
 * ==============================================================================
 * 名称: UIAPduino 32-Band FFT Spectrum Analyzer
 * 開発日: 2026年3月12日
 * バージョン: V1.90
 * 開発者: Gemini & yas
 * ピンアサイン: UIAPduino用
 * - マイク入力: PA2
 * - DISPLAYボタン: PC6
 * - ANALYSISボタン: PC5
 * - OLED I2C SCL: PC2
 * - OLED I2C SDA: PC1
 *
 * 簡単な説明:
 * 極小メモリのマイコンで動作する、本格的なSP（スペクトラム）とEQ（イコライザー）の
 * 2モードを搭載したオーディオビジュアライザーです。
 *
 * * Modification History:
 * - V1.00 - V1.89: 基礎開発、メモリ最適化、AFIOによるPC5解放、Exact Mapping、ノイズ対策、線形補間実装。
 * - V1.90: [Public Release Formatting] 初心者向けの解説コメントを各部に追加。プログラムの内部ロジックはV1.89から一切変更していません。
 * ==============================================================================
 */

#include <Arduino.h>

// --- ハードウェア設定 ---
// 【初心者向け解説】マイコンのどの足（ピン）にマイクや画面を繋ぐかを決めています。
#define PIN_ADC_MIC         PA2  
#define PIN_BTN_DISPLAY     PC6  
#define PIN_BTN_ANALYSIS    PC5  
#define OLED_ADDR           0x3C
#define PIN_I2C_SCL         PC2  
#define PIN_I2C_SDA         PC1  

// --- システム定数 ---
// 【初心者向け解説】画面の大きさや、音をいくつに切り分けるかという基本ルールです。
#define FFT_POINTS        64   
#define NUM_BANDS         32   
#define OLED_WIDTH        128
#define OLED_HEIGHT       32
#define MAX_BAR_HEIGHT    20    

enum DISPLAY_MODE { MODE_NORMAL, MODE_PEAK_HOLD, MODE_DOTS };
enum ANALYSIS_MODE { MODE_SPEC, MODE_EQ };

// 【初心者向け解説】現在の表示モード（ノーマル/ピークホールド等）を覚えておく変数です。
volatile DISPLAY_MODE currentDispMode = MODE_NORMAL;
volatile ANALYSIS_MODE currentAnlysMode = MODE_SPEC;

// --- 究極のメモリ節約：OLEDバッファを共有 ---
// 【初心者向け解説】少ないメモリをやりくりするため、音の計算と画面の描画で同じ場所（512バイト）を使い回します。
uint8_t  oled_buffer[512]; 

// --- 物理・フィルタ変数 ---
uint8_t  band_values[NUM_BANDS];    
uint16_t peak_y[NUM_BANDS];         
int16_t  peak_velocity[NUM_BANDS];  
uint16_t dc_offset = 512;           

// --- EQモード用 補間マッピングテーブル (V1.89: 10倍精度の固定小数点。高音の無音域を避け2.0〜24.0へマッピング) ---
// 【初心者向け解説】EQモードで、音の高さを自然に見せるために「どのデータをどこに表示するか」を決める設計図です。
const uint16_t eq_map_fixed[32] PROGMEM = {
    20, 22, 23, 25, 28, 30, 32, 35, 38, 41, 45, 48, 52, 57, 61, 67, 
    72, 78, 85, 92, 99, 108, 117, 126, 137, 148, 161, 174, 188, 204, 221, 240
};

// --- サイン・窓テーブル ---
// 【初心者向け解説】音の波を計算する（FFT）ために必要な、三角関数などの数学のデータです。
const int8_t SinTable[] PROGMEM = {
    0, 6, 12, 18, 25, 31, 37, 43, 49, 54, 60, 65, 71, 75, 80, 84,
    89, 93, 97, 100, 103, 107, 109, 112, 115, 117, 119, 121, 123, 124, 125, 126, 127
};
const uint16_t WindowTable[32] PROGMEM = {
    0, 1, 3, 7, 12, 19, 27, 36, 47, 59, 71, 85, 99, 114, 130, 146,
    163, 179, 196, 212, 228, 244, 259, 273, 286, 297, 306, 313, 316, 318, 319, 319
};

// --- ミニフォントデータ (表示結果と完全一致) ---
// 【初心者向け解説】画面の下に表示される小さな文字（0〜9、A〜Z）のドット絵データです。
const uint8_t mini_font[][5] PROGMEM = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0: '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1: '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2: '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3: '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4: '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5: '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6: '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7: '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8: '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9: '9'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 10: K
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 11: A
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // 12: M
    {0x7F, 0x02, 0x04, 0x08, 0x7F}, // 13: N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 14: O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 15: P (V1.85 Corrected)
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 16: R (V1.85 Corrected)
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 17: E
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 18: D
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 19: T
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 20: S
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 21: U
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 22: I
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 23: C
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 24: L
    {0x03, 0x04, 0x78, 0x04, 0x03}, // 25: V
    {0x43, 0x45, 0x49, 0x51, 0x61}, // 26: Z
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 27: Q
    {0x01, 0x02, 0x7C, 0x02, 0x01}, // 28: Y (V1.85 Added for Logo)
    {0x00, 0x00, 0x00, 0x00, 0x00}  // 29: BLANK (V1.85 Shifted)
};

// ==============================================================================
// I2C / OLED 制御
// ==============================================================================
// 【初心者向け解説】画面（OLED）と通信するための準備をします。処理を速くするため、直接マイコンの奥深く（レジスタ）を操作しています。
void light_i2c_init() {
    RCC->APB2PCENR |= (1 << 4) | (1 << 0); RCC->APB1PCENR |= (1 << 21);
    GPIOC->CFGLR &= ~(0xFF << 4); GPIOC->CFGLR |= (0xDD << 4);  
    I2C1->CTLR1 |= (1 << 15); I2C1->CTLR1 &= ~(1 << 15); 
    I2C1->CTLR2 = 48; I2C1->CKCFGR = (1 << 15) | 40; I2C1->CTLR1 |= (1 << 0); 
}

inline void i2c_stream_write(uint8_t data) {
    I2C1->DATAR = data; while(!(I2C1->STAR1 & (1 << 7))); 
}

inline void i2c_stream_start(uint8_t addr) {
    I2C1->CTLR1 |= (1 << 8); while(!(I2C1->STAR1 & (1 << 0)));
    I2C1->DATAR = addr << 1; while(!(I2C1->STAR1 & (1 << 1))); (void)I2C1->STAR2;
}

// 【初心者向け解説】画面の電源を入れた時の初期設定です。
void oled_init() {
    const uint8_t init_cmds[] = { 0xAE, 0x20, 0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, 0x03, 0x40, 0x81, 0x7F, 0xA1, 0xA8, 0x1F, 0xC8, 0xD3, 0x00, 0xDA, 0x02, 0xD5, 0x80, 0xD9, 0xF1, 0xDB, 0x30, 0x8D, 0x14, 0xAF };
    i2c_stream_start(OLED_ADDR); i2c_stream_write(0x00);
    for (uint8_t i = 0; i < sizeof(init_cmds); i++) i2c_stream_write(init_cmds[i]);
    I2C1->CTLR1 |= (1 << 9); delay(10);
}

// 【初心者向け解説】計算が終わった「画面のデータ」を一気にOLEDへ転送して表示させます。
void oled_send_buffer(uint8_t* buffer) {
    i2c_stream_start(OLED_ADDR); i2c_stream_write(0x00);
    i2c_stream_write(0x21); i2c_stream_write(0); i2c_stream_write(127);
    i2c_stream_write(0x22); i2c_stream_write(0); i2c_stream_write(3);
    I2C1->CTLR1 |= (1 << 9);
    i2c_stream_start(OLED_ADDR); i2c_stream_write(0x40);
    for (uint16_t i = 0; i < 512; i++) { I2C1->DATAR = buffer[i]; while(!(I2C1->STAR1 & (1 << 7))); }
    I2C1->CTLR1 |= (1 << 9);
}

// 【初心者向け解説】指定した場所に、小さな文字のドット絵を描き込む機能です。
void draw_char(uint8_t x, uint8_t y, uint8_t idx) {
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t col = pgm_read_byte(&(mini_font[idx][i]));
        for(uint8_t j = 0; j < 7; j++) {
            if(col & (1 << j)) {
                uint8_t py = y + j;
                if(py < 32 && (x+i) < 128) oled_buffer[(x+i) + (py/8)*128] |= (1 << (py%8));
            }
        }
    }
}

// 【初心者向け解説】起動時に表示される「UIAPDUINO SPECTRUM ANALYZER」のロゴを描きます。
void show_boot_screen() {
    memset(oled_buffer, 0, sizeof(oled_buffer));
    
    // Line 1: UIAPDUINO
    uint8_t l1[] = {21, 22, 11, 15, 18, 21, 22, 13, 14};
    for(uint8_t i=0; i<9; i++) draw_char(37 + i*6, 4, l1[i]);
    
    // Line 2: SPECTRUM ANALYZER (V1.85 Restored)
    uint8_t l2[] = {20, 15, 17, 23, 19, 16, 21, 12, 29, 11, 13, 11, 24, 28, 26, 17, 16};
    for(uint8_t i=0; i<17; i++) draw_char(13 + i*6, 16, l2[i]);

    oled_send_buffer(oled_buffer);
    delay(2000);
}

// 【初心者向け解説】画面の周りの枠線と、下の文字（NORM, SPなど）を描きます。
void draw_ui_frame() {
    for(uint8_t x=0; x<128; x++) { oled_buffer[x] |= 0x01; oled_buffer[x + 3*128] |= 0x80; }
    for(uint8_t p=0; p<4; p++) { oled_buffer[0 + p*128] = 0xFF; oled_buffer[127 + p*128] = 0xFF; }
    for(uint8_t x=0; x<128; x++) oled_buffer[x + (21/8)*128] |= (1 << (21%8));
    
    draw_char(4, 23, 2); draw_char(10, 23, 0); 
    
    if(currentDispMode == MODE_NORMAL) { 
        draw_char(50, 23, 13); draw_char(56, 23, 14); draw_char(62, 23, 16); draw_char(68, 23, 12); 
    } else if(currentDispMode == MODE_PEAK_HOLD) { 
        draw_char(50, 23, 15); draw_char(56, 23, 17); draw_char(62, 23, 11); draw_char(68, 23, 10); 
    } else { 
        draw_char(50, 23, 18); draw_char(56, 23, 14); draw_char(62, 23, 19); draw_char(68, 23, 20); 
    }
    
    // 【V1.84】SP(20, 15) / EQ(17, 27) の描画を完全固定化
    if(currentAnlysMode == MODE_SPEC) { 
        draw_char(88, 23, 20); draw_char(94, 23, 15); 
    } else { 
        draw_char(88, 23, 17); draw_char(94, 23, 27); 
    } 
    
    draw_char(104, 23, 2); draw_char(110, 23, 0); draw_char(116, 23, 10); 
}

// ==============================================================================
// オーディオ解析
// ==============================================================================
// 【初心者向け解説】マイクの音をデジタルデータとして読み取るための設定です。
void init_ADC() {
    RCC->APB2PCENR |= (1 << 2) | (1 << 9);
    GPIOA->CFGLR &= ~(0xF << (2 * 4)); 
    ADC1->CTLR2 = (1 << 20) | (0x7 << 17); 
    ADC1->RSQR3 = 0; 
    ADC1->CTLR2 |= (1 << 0); delay(1);
    ADC1->CTLR2 |= (1 << 3); while(ADC1->CTLR2 & (1 << 3));
    ADC1->CTLR2 |= (1 << 2); while(ADC1->CTLR2 & (1 << 2));
}

// 【初心者向け解説】マイクから音の波形（64個のデータ）をものすごいスピードで切り取ります。
void capture_audio() {
    uint16_t* adc_ptr = (uint16_t*)&oled_buffer[256];
    for (int i = 0; i < FFT_POINTS; i++) {
        ADC1->CTLR2 |= (1 << 22); 
        while(!(ADC1->STATR & (1 << 1))); 
        adc_ptr[i] = ADC1->RDATAR;
        delayMicroseconds(50); 
    }
}

// 【初心者向け解説】FFT計算に必要なサイン波の値を、テーブルから取り出します。
int8_t get_sin(uint8_t idx) {
    idx &= 0x7F;
    if (idx < 32) return (int8_t)pgm_read_byte(&SinTable[idx]);
    if (idx < 64) return (int8_t)pgm_read_byte(&SinTable[64 - idx]);
    if (idx < 96) return -(int8_t)pgm_read_byte(&SinTable[idx - 64]);
    return -(int8_t)pgm_read_byte(&SinTable[128 - idx]);
}

// 【初心者向け解説】音の波を「どの高さの音が、どれくらい強いか」に分解する、核となる計算（FFT）です。
void fix_fft(int16_t fr[], int16_t fi[], uint8_t m) {
    uint8_t n = 1 << m; uint8_t i, j, k, l; int16_t tr, ti, c, s; j = 0;
    for (i = 0; i < n - 1; i++) {
        if (i < j) { tr = fr[i]; fr[i] = fr[j]; fr[j] = tr; ti = fi[i]; fi[i] = fi[j]; fi[j] = ti; }
        k = n >> 1; while (k <= j) { j -= k; k >>= 1; } j += k;
    }
    for (l = 1; l <= m; l++) { 
        uint8_t istep = 1 << l; uint8_t p = n >> l; 
        for (j = 0; j < (1 << (l - 1)); j++) { 
            c = get_sin((j * p + 16) << 1); s = get_sin((j * p) << 1); 
            for (i = j; i < n; i += istep) {
                k = i + (1 << (l - 1));
                tr = ((int32_t)c * fr[k] - (int32_t)s * fi[k]) >> 7;
                ti = ((int32_t)s * fr[k] + (int32_t)c * fi[k]) >> 7;
                fr[k] = fr[i] - tr; fi[k] = fi[i] - ti;
                fr[i] = fr[i] + tr; fi[i] = fi[i] + ti;
            } 
        } 
    }
}

// 【初心者向け解説】取り込んだ音を分析して、画面の「32本のバー」それぞれの高さを決定します。
void process_fft() {
    int16_t* fr = (int16_t*)&oled_buffer[0];   
    int16_t* fi = (int16_t*)&oled_buffer[128]; 
    uint16_t* adc = (uint16_t*)&oled_buffer[256]; 

    // DCオフセット追従
    // 【初心者向け解説】無音状態のわずかなノイズ（ズレ）を自動で計算して取り除きます。
    int32_t avg = 0;
    for(int i=0; i<FFT_POINTS; i++) {
        avg += adc[i];
    }
    avg >>= 6; 
    
    if (avg > dc_offset + 1) {
        dc_offset += 1; 
    } else if (avg < dc_offset - 1) {
        dc_offset -= 1;
    }

    // 【初心者向け解説】音の波の両端を少し削って滑らかにし（窓関数）、分析しやすくします。
    for (int i = 0; i < FFT_POINTS; i++) {
        int32_t val = (int16_t)adc[i] - dc_offset;
        uint16_t win = (i < 32) ? pgm_read_word(&WindowTable[i]) : pgm_read_word(&WindowTable[63-i]);
        fr[i] = (int16_t)((val * win) >> 9); 
        fi[i] = 0; 
    }
    fix_fft(fr, fi, 6);

    // 【V1.89】全FFTバッファのマグニチュードを事前計算
    uint16_t fft_mag[32];
    for (int i = 0; i < 32; i++) {
        fft_mag[i] = (uint16_t)(abs(fr[i]) + abs(fi[i]));
    }

    // 【V1.89】空間平滑化と線形補間のためのマグニチュード一時バッファ
    uint16_t raw_mag[NUM_BANDS];
    for (int b = 0; b < NUM_BANDS; b++) {
        if (currentAnlysMode == MODE_SPEC) {
            uint8_t fft_idx = b + 2;
            if (fft_idx > 31) fft_idx = 31;
            raw_mag[b] = fft_mag[fft_idx];
        } else {
            // EQモード: 固定小数点テーブルを用いた線形補間（滑らかな波形を計算で生成）
            // 【初心者向け解説】EQモードでは、音がカクカクしないように「バーとバーの間の高さ」を滑らかな坂道になるように計算しています。
            uint16_t pos10 = pgm_read_word(&eq_map_fixed[b]);
            uint8_t idx = pos10 / 10;
            uint8_t frac = pos10 % 10; 
            
            uint16_t val1 = fft_mag[idx];
            uint16_t val2 = (idx < 31) ? fft_mag[idx + 1] : val1;
            
            uint32_t interpolated = (val1 * (10 - frac) + val2 * frac) / 10;
            
            // トレブルブースト（高音域のハードウェア的なエネルギー減衰を強力に補正）
            // 【初心者向け解説】マイクが拾いにくい高音を、プログラムの力で少し大きくして見えやすくします。
            interpolated = interpolated + (interpolated * b) / 15; 
            
            raw_mag[b] = (uint16_t)interpolated;
        }
    }

    for (int b = 0; b < NUM_BANDS; b++) {
        uint32_t mag = raw_mag[b];
        
        // 【V1.89】EQモード特有の「ブロック動作」を解消する5-tap空間平滑化フィルタ
        // 【初心者向け解説】隣り合うバーの音を少しずつ混ぜ合わせることで、水面のような滑らかな波の動きを作ります。
        if (currentAnlysMode == MODE_EQ) {
            uint32_t sum = raw_mag[b] * 3;
            uint8_t weight = 3;
            if (b > 0) { sum += raw_mag[b-1] * 2; weight += 2; }
            if (b > 1) { sum += raw_mag[b-2] * 1; weight += 1; }
            if (b < NUM_BANDS - 1) { sum += raw_mag[b+1] * 2; weight += 2; }
            if (b < NUM_BANDS - 2) { sum += raw_mag[b+2] * 1; weight += 1; }
            mag = sum / weight;
        }

        // 動的ゲート処理
        // 【初心者向け解説】無音時のフワフワしたノイズには反応しないように、重し（足切りライン）を設定します。
        uint8_t fft_idx = (currentAnlysMode == MODE_SPEC) ? (b + 2) : (pgm_read_word(&eq_map_fixed[b]) / 10);
        uint16_t dynamic_gate;
        if (fft_idx <= 2) {
            dynamic_gate = 80;  
        } else if (fft_idx <= 4) {
            dynamic_gate = 50;  
        } else {
            dynamic_gate = (45 - fft_idx > 12 ? 45 - fft_idx : 12); 
        }

        // 【初心者向け解説】分析した音の強さを、画面のバーの高さ（0〜20ピクセル）に変換し、落下スピードを計算します。
        uint8_t raw_val = 0;
        if(mag > dynamic_gate) {
            uint16_t m = (uint16_t)(((uint32_t)(mag - dynamic_gate) * (32 + b + ((b * b) >> 4))) >> 5);
            raw_val = (m > 8) ? (m <= 28 ? 8 + ((m - 8) >> 1) : 18 + ((m - 28) >> 2)) : m;
        }
        if(raw_val > MAX_BAR_HEIGHT) raw_val = MAX_BAR_HEIGHT;
        if(raw_val > band_values[b]) band_values[b] = (raw_val * 15 + band_values[b] * 1) >> 4;
        else band_values[b] = (raw_val * 6 + band_values[b] * 10) >> 4; 
        uint16_t current_h_fixed = (uint16_t)band_values[b] << 4;
        if(current_h_fixed >= peak_y[b]) { peak_y[b] = current_h_fixed; peak_velocity[b] = 0; }
        else { if(peak_y[b] > peak_velocity[b]) peak_y[b] -= peak_velocity[b]; else peak_y[b] = 0; peak_velocity[b] += 4; }
    }
}

// 【初心者向け解説】計算された「バーの高さ」と「ピークの点」を、画面用のバッファ（メモリ）に描き込みます。
void render_oled() {
    memset(oled_buffer, 0, sizeof(oled_buffer));
    draw_ui_frame();
    for (int b = 0; b < NUM_BANDS; b++) {
        uint8_t x = 1 + b * 4, h = band_values[b], ph = peak_y[b] >> 4;
        if (currentDispMode != MODE_DOTS) {
            for (uint8_t w = 1; w < 4; w++) {
                if (x + w >= 127) continue; 
                for (uint8_t i = 0; i < h; i++) {
                    uint8_t y = 20 - i; oled_buffer[x + w + (y / 8) * 128] |= (1 << (y % 8));
                }
            }
        }
        if (currentDispMode != MODE_NORMAL && ph > 0 && ph <= 20) {
            uint8_t py = 20 - ph; 
            for(uint8_t w=1; w<4; w++) {
                if (x + w >= 127) continue; 
                oled_buffer[x + w + (py / 8) * 128] |= (1 << (py % 8));
            }
        }
    }
    oled_send_buffer(oled_buffer); // ここで実際に画面へ送られます
}

// ==============================================================================
// メイン処理
// ==============================================================================
// 【初心者向け解説】電源を入れた時に1回だけ実行される、初期設定の場所です。
void setup() {
    RCC->APB2PCENR |= (1 << 0); 
    AFIO->PCFR1 |= (1 << 15);    
    
    light_i2c_init(); 
    oled_init();
    show_boot_screen(); 
    
    pinMode(PIN_BTN_DISPLAY, INPUT_PULLUP);
    pinMode(PIN_BTN_ANALYSIS, INPUT_PULLUP);
    init_ADC();
}

// 【初心者向け解説】電源が入っている間、ずっとものすごいスピードで繰り返されるメインの処理です。
void loop() {
    static uint32_t lastBtnD = 0, lastBtnA = 0;
    
    // ボタンが押されたかどうかのチェック
    if (millis() - lastBtnD > 200 && digitalRead(PIN_BTN_DISPLAY) == LOW) {
        currentDispMode = (DISPLAY_MODE)((int)currentDispMode + 1); 
        if (currentDispMode > MODE_DOTS) currentDispMode = MODE_NORMAL;
        lastBtnD = millis();
    }
    
    if (millis() - lastBtnA > 200 && digitalRead(PIN_BTN_ANALYSIS) == LOW) {
        currentAnlysMode = (ANALYSIS_MODE)((int)currentAnlysMode + 1);
        if (currentAnlysMode > MODE_EQ) currentAnlysMode = MODE_SPEC;
        lastBtnA = millis();
    }
    
    // 音を取り込む → 分析する → 画面に描く、の3ステップを繰り返します
    capture_audio(); 
    process_fft(); 
    render_oled();   
}
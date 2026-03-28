/*
 * ======================================================================================
 * Project: UIAPduino_I2C_Sniffer
 * Version: 1.00 (Public Release)
 * Architect: yas & Gemini
 * Date: 2026/03/28
 *
 * [Description]
 * フリスクケースへの組み込みを目指した、PC不要のスタンドアローンI2Cスニッファー。
 * RAM 2KBの限界(CH32V003)に挑み、波形と同期したデータ(0X??)のオーバーレイ表示、
 * チラつきのないスマートリフレッシュ、250サンプルの限界録画を実現。
 *
 * [I/O Pin Map]
 * PA1 : LED_READ (Active)
 * PA2 : LED_ERR  (Error / NACK)
 * PC1 : OLED_SDA
 * PC2 : OLED_SCL
 * PC3 : SW1_LEFT
 * PC4 : SW2_SEL
 * PC5 : SW3_RIGHT
 * PC6 : TGT_SDA  (Target SDA)
 * PC7 : TGT_SCL  (Target SCL)
 *
 * [Change History]
 * v0.35 : Implemented Smart Refresh to eliminate screen flicker.
 * v0.36 : Replaced corrupted font array to fix lowercase shift.
 * v1.00 : Public Release. Added I/O map, detailed comments, and FRISK concept note.
 * ======================================================================================
 */

#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// --- 定数定義 ---
#define MAX_SAMPLES 250   // RAM限界のサンプリング数
#define ZOOM_WIDTH   3    // 波形の拡大率(ドット幅)
#define PRE_SAMPLES  5    // プリトリガーサンプル数

// --- ピンビットマクロ ---
#define LED_READ_BIT  1  
#define LED_ERR_BIT   2  
#define OLED_SDA_BIT  1  
#define OLED_SCL_BIT  2  
#define SW1_LEFT_BIT  3  
#define SW2_SEL_BIT   4  
#define SW3_RIGHT_BIT 5  
#define TGT_SDA_BIT   6  
#define TGT_SCL_BIT   7  

// --- LED制御マクロ ---
#define LED_READ_ON()     (GPIOA->OUTDR |=  (1 << LED_READ_BIT))
#define LED_READ_OFF()    (GPIOA->OUTDR &= ~(1 << LED_READ_BIT))
#define LED_READ_TOGGLE() (GPIOA->OUTDR ^= (1 << LED_READ_BIT))
#define LED_ERR_ON()      (GPIOA->OUTDR |=  (1 << LED_ERR_BIT))
#define LED_ERR_OFF()     (GPIOA->OUTDR &= ~(1 << LED_ERR_BIT))

// --- ASCII 5x8フォント配列 (小文字修正版) ---
const uint8_t font5x8[][5] = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
  {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
  {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08},{0x00,0x00,0x00,0x00,0x00}
};

const char hex_chars[] = "0123456789ABCDEF";

// --- OLED用 ソフトウェアI2C制御 ---
void sda_high() { GPIOC->OUTDR |= (1<<OLED_SDA_BIT); }
void sda_low()  { GPIOC->OUTDR &= ~(1<<OLED_SDA_BIT); }
void scl_high() { GPIOC->OUTDR |= (1<<OLED_SCL_BIT); }
void scl_low()  { GPIOC->OUTDR &= ~(1<<OLED_SCL_BIT); }

void i2c_start() { sda_high(); scl_high(); sda_low(); scl_low(); }
void i2c_stop()  { sda_low(); scl_high(); sda_high(); }

void i2c_write(uint8_t data) {
  for(uint8_t i=0; i<8; i++){ 
    if(data&0x80) sda_high(); else sda_low(); 
    scl_high(); scl_low(); data<<=1; 
  }
  sda_high(); scl_high(); scl_low(); 
}

void oled_cmd(uint8_t c) { 
  i2c_start(); i2c_write(0x3C<<1); i2c_write(0x00); i2c_write(c); i2c_stop(); 
}

void oled_init() { 
  oled_cmd(0xAE); oled_cmd(0x20); oled_cmd(0x02);
  oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); 
}

void oled_set_cursor(uint8_t col, uint8_t page) {
  oled_cmd(0xB0 + page);
  oled_cmd(0x00 + (col & 0x0F));
  oled_cmd(0x10 + ((col >> 4) & 0x0F));
}

void oled_clear() {
  for(uint8_t p = 0; p < 8; p++) {
    oled_set_cursor(0, p);
    i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
    for(uint16_t x = 0; x < 128; x++) i2c_write(0x00); 
    i2c_stop();
  }
}

void oled_print_char(char c, uint8_t col, uint8_t page) {
  if (c < 32 || c > 126) c = 32;
  oled_set_cursor(col, page);
  i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
  for (uint8_t i = 0; i < 5; i++) i2c_write(font5x8[c - 32][i]);
  i2c_write(0x00); 
  i2c_stop();
}

void oled_print_str(const char* str, uint8_t col, uint8_t page) {
  while (*str) {
    oled_print_char(*str++, col, page);
    col += 6;
  }
}

// --- スニッファー用バッファと構造体 ---
uint16_t raw_buf[MAX_SAMPLES];
uint8_t  state_buf[MAX_SAMPLES]; 

// 省メモリ用: イベント発生位置と文字列のみを記録する構造体(最大30件)
struct DecodeEvent {
  uint16_t index;
  char text[5];
};
DecodeEvent sym_events[30];
uint8_t sym_event_count = 0;

uint16_t sample_count = 0;
char     decode_buf[64]; 

// --- I2C記録処理 ---
void record_i2c() {
  sample_count = 0;
  uint16_t reg_val;
  uint32_t idle_counter = 0;
  uint32_t heartbeat = 0;

  oled_clear();
  oled_print_str("WATCHING...", 4, 3);
  LED_ERR_OFF();

  // アイドル状態(SDA=H, SCL=H)待機
  while(idle_counter < 500) {
    if ((GPIOC->INDR & 0xC0) == 0xC0) idle_counter++;
    else idle_counter = 0;
    if (++heartbeat > 5000) { LED_READ_TOGGLE(); heartbeat = 0; }
  }

  // スタートコンディション(SCL=HのままSDA=L)待機
  while(1) {
    reg_val = (uint16_t)GPIOC->INDR;
    if ((reg_val & 0xC0) == 0x80) break;
    if (++heartbeat > 5000) { LED_READ_TOGGLE(); heartbeat = 0; }
  }

  LED_READ_ON();
  // プリトリガーデータの保存
  for(uint8_t i=0; i < PRE_SAMPLES; i++) raw_buf[i] = 0x00C0; 
  raw_buf[PRE_SAMPLES] = reg_val;
  sample_count = PRE_SAMPLES + 1;

  uint16_t last_reg = reg_val;
  // レジスタ変化時のみ記録(エッジ検出方式)
  while(sample_count < MAX_SAMPLES) {
    reg_val = (uint16_t)GPIOC->INDR;
    if (reg_val != last_reg) {
      raw_buf[sample_count++] = reg_val;
      last_reg = reg_val;
    }
  }
  LED_READ_OFF();

  // 描画用にビットを抽出(SCL/SDA)
  for(uint16_t i=0; i<sample_count; i++) {
    state_buf[i] = (uint8_t)((raw_buf[i] >> 6) & 0x03);
  }
}

// --- I2Cデコード処理 ---
void decode_i2c() {
  memset(decode_buf, 0, sizeof(decode_buf));
  sym_event_count = 0; 
  uint8_t buf_idx = 0, bit_count = 0, current_byte = 0;
  bool in_transfer = false;
  uint8_t byte_count = 0;

  for (uint16_t i = 1; i < sample_count; i++) {
    if (buf_idx >= 60) break; 
    uint8_t prev = state_buf[i-1], curr = state_buf[i];
    bool scl_p = (prev & 0x02) != 0, scl_c = (curr & 0x02) != 0;
    bool sda_p = (prev & 0x01) != 0, sda_c = (curr & 0x01) != 0;

    // Start コンディション検出
    if (scl_p && scl_c && sda_p && !sda_c) {
      decode_buf[buf_idx++] = 'S'; decode_buf[buf_idx++] = ' ';
      if (sym_event_count < 30) {
        sym_events[sym_event_count].index = i;
        strcpy(sym_events[sym_event_count].text, "S");
        sym_event_count++;
      }
      in_transfer = true; bit_count = 0; current_byte = 0; byte_count = 0;
    } 
    // Stop コンディション検出
    else if (scl_p && scl_c && !sda_p && sda_c) {
      decode_buf[buf_idx++] = 'P'; decode_buf[buf_idx++] = ' ';
      if (sym_event_count < 30) {
        sym_events[sym_event_count].index = i;
        strcpy(sym_events[sym_event_count].text, "P");
        sym_event_count++;
      }
      in_transfer = false;
    } 
    // データビット検出 (SCLの立ち上がり)
    else if (!scl_p && scl_c && in_transfer) {
      if (bit_count < 8) {
        current_byte = (current_byte << 1) | (sda_c ? 1 : 0);
        bit_count++;
      } else {
        // 8ビット完了 + ACK/NACK
        char hex[4];
        hex[0] = hex_chars[(current_byte >> 4) & 0x0F];
        hex[1] = hex_chars[current_byte & 0x0F];
        hex[2] = sda_c ? 'N' : 'A';
        hex[3] = '\0';
        
        // オーバーレイ描画用データ (0x??形式)
        if (sym_event_count < 30) {
          sym_events[sym_event_count].index = i;
          char hex_evt[5];
          hex_evt[0] = '0';
          hex_evt[1] = 'x'; 
          hex_evt[2] = hex[0];
          hex_evt[3] = hex[1];
          hex_evt[4] = '\0';
          strcpy(sym_events[sym_event_count].text, hex_evt);
          sym_event_count++;
        }

        // 下部リスト描画用データ (ACK/NACK含むログ形式)
        decode_buf[buf_idx++] = hex[0];
        decode_buf[buf_idx++] = hex[1];
        decode_buf[buf_idx++] = hex[2];
        decode_buf[buf_idx++] = ' ';
        
        if (sda_c && byte_count == 0) LED_ERR_ON(); // アドレスNACK検出
        bit_count = 0; current_byte = 0; byte_count++;
      }
    }
  }
}

// --- 波形と同期データの描画処理 ---
void draw_waveform_and_sync_symbols(uint16_t offset) {
  // Page 0, 1: SCL/SDA波形描画 (空白追加による左端のゴミ消去)
  for(uint8_t sig = 0; sig < 2; sig++) {
    uint8_t p = sig; 
    uint8_t bit_mask = (sig == 0) ? 0x02 : 0x01; 
    oled_print_str((sig == 0) ? "SCL " : "SDA ", 0, p);
    oled_set_cursor(24, p); 
    i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
    for(uint8_t e = 0; e < 34; e++) {
      uint16_t idx = offset + e;
      if (idx < sample_count) {
        bool level = (state_buf[idx] & bit_mask) != 0;
        uint8_t pattern = level ? 0x02 : 0x40; 
        bool edge = (idx > 0) && ((state_buf[idx] & bit_mask) != (state_buf[idx-1] & bit_mask));
        for(uint8_t x = 0; x < ZOOM_WIDTH; x++) {
          if (edge && x == 0) i2c_write(0x7E); else i2c_write(pattern);
        }
      } else {
        for(uint8_t x = 0; x < ZOOM_WIDTH; x++) i2c_write(0x00);
      }
    }
    i2c_stop();
  }

  // Page 2: 波形同期データ(0x??)の描画
  for(uint8_t e = 0; e < 34; e++) {
    uint16_t idx = offset + e;
    if (idx < sample_count) {
      for (uint8_t ev = 0; ev < sym_event_count; ev++) {
        if (sym_events[ev].index == idx) {
          uint8_t col = 24 + (e * ZOOM_WIDTH);
          uint8_t len = strlen(sym_events[ev].text);
          // ゴースト(ラップアラウンド)防止のクリッピング処理
          if (col + (len * 6) <= 128) {
            oled_print_str(sym_events[ev].text, col, 2);
          }
          break;
        }
      }
    }
  }
}

void setup() {
  RCC->APB2PCENR |= (1 << 4) | (1 << 2); 
  GPIOA->CFGLR &= ~((0xF << (4 * LED_READ_BIT)) | (0xF << (4 * LED_ERR_BIT)));
  GPIOA->CFGLR |=  ((0x3 << (4 * LED_READ_BIT)) | (0x3 << (4 * LED_ERR_BIT)));
  GPIOC->CFGLR &= ~((0xF << (4 * OLED_SDA_BIT)) | (0xF << (4 * OLED_SCL_BIT)) |
                    (0xF << (4 * SW1_LEFT_BIT)) | (0xF << (4 * SW2_SEL_BIT)) | (0xF << (4 * SW3_RIGHT_BIT)) |
                    (0xF << (4 * TGT_SDA_BIT))  | (0xF << (4 * TGT_SCL_BIT)));
  GPIOC->CFGLR |=  ((0x3 << (4 * OLED_SDA_BIT)) | (0x3 << (4 * OLED_SCL_BIT)) |
                    (0x8 << (4 * SW1_LEFT_BIT)) | (0x8 << (4 * SW2_SEL_BIT)) | (0x8 << (4 * SW3_RIGHT_BIT)) |
                    (0x4 << (4 * TGT_SDA_BIT))  | (0x4 << (4 * TGT_SCL_BIT)));
  GPIOC->OUTDR |= (1 << SW1_LEFT_BIT) | (1 << SW2_SEL_BIT) | (1 << SW3_RIGHT_BIT);
  
  LED_READ_OFF(); LED_ERR_OFF();
  oled_init(); oled_clear();
  
  oled_print_str("Sniffer Ver1.00", 4, 0);
  oled_print_str("STANDBY MODE", 4, 3);
  oled_print_str("Press [SEL] to Watch", 4, 6);
}

void loop() {
  if ((GPIOC->INDR & (1 << SW2_SEL_BIT)) == 0) {
    delay(20); 
    if ((GPIOC->INDR & (1 << SW2_SEL_BIT)) == 0) {
      while((GPIOC->INDR & (1 << SW2_SEL_BIT)) == 0); 
      record_i2c(); decode_i2c(); 
      
      if (sample_count > 0) {
        uint16_t view_offset = 0; 
        uint16_t last_offset = 0xFFFF; // 初回描画トリガー用ダミー値
        bool exit_view = false;
        oled_clear();
        char tmp_str[22]; 

        while(!exit_view) {
          // スマートリフレッシュ: 位置が変化した時のみ再描画を実行
          if (view_offset != last_offset) {
            oled_set_cursor(0, 2); i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
            for(uint16_t x=0; x<128; x++) i2c_write(0x00); i2c_stop();
            
            draw_waveform_and_sync_symbols(view_offset);
            
            char buf[24];
            sprintf(buf, "E:%-3d P:%-3d", sample_count, view_offset);
            oled_print_str(buf, 0, 4); 
            
            uint8_t dec_len = strlen(decode_buf);
            for(uint8_t i = 0; i < 2; i++) {
              if (i * 21 < dec_len) {
                memset(tmp_str, ' ', 21);
                uint8_t len = strlen(&decode_buf[i * 21]);
                if (len > 21) len = 21;
                strncpy(tmp_str, &decode_buf[i * 21], len);
                tmp_str[21] = '\0';
                oled_print_str(tmp_str, 0, 6 + i); 
              }
            }
            last_offset = view_offset; // 描画完了をマーク
          }

          if (!(GPIOC->INDR & (1 << SW1_LEFT_BIT))) { if (view_offset > 0) view_offset--; delay(80); }
          if (!(GPIOC->INDR & (1 << SW3_RIGHT_BIT))) { if (view_offset < sample_count - 1) view_offset++; delay(80); }
          if (!(GPIOC->INDR & (1 << SW2_SEL_BIT))) { delay(200); exit_view = true; }
        }
      }
      oled_clear();
      oled_print_str("Sniffer Ver1.00", 4, 0);
      oled_print_str("STANDBY MODE", 4, 3);
      oled_print_str("Press [SEL] to Watch", 4, 6);
    }
  }
}
/* ==========================================================================
 * Project: Westley 2-Wire LED Dedicated Controller with OLED UI
 * Board:   UIAPduino (CH32V003F4 / RISC-V 48MHz)
 * Version: 0.10 (Ultimate Edition)
 * History: 
 * - 2026/06/06 - V0.10 - Implemented dynamic text justification and centering 
 * engine. Expanded font set to support full spellings (e.g., "MAGENTA").
 * - 2026/06/06 - V0.09 - Added translation map to fix color index offset.
 * - 2026/06/06 - V0.08 - Added interactive OLED UI with 8 selectable color modes.
 * - 2026/06/06 - V0.07 - Adjusted step pulse width to 60us for stable color switching.
 * - 2026/06/06 - V0.06 - Converted to Step Controller. 
 * ========================================================================== */

#include <Arduino.h>
#include <Wire.h>

#define OLED_ADDR 0x3C
#define LED_PIN   5
#define SELECT_PIN 6
#define SET_PIN    7

const uint8_t init_cmds[] = {
  0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
  0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
  0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
};

static uint8_t page_buffer[128];
static uint8_t current_page = 0;

uint8_t current_selection = 7; // 初期状態: OFF
bool last_select = HIGH;
bool last_set = HIGH;

// OLED表示用ラベル (フルスペル)
const char* labels[8] = {"RED", "GREEN", "YELLOW", "BLUE", "MAGENTA", "CYAN", "WHITE", "OFF"};

// 色の翻訳テーブル (OLEDの順序 -> 実際のパルス回数へ変換)
// RED(2), GREEN(3), YELLOW(4), BLUE(5), MAGENTA(6), CYAN(7), WHITE(0), OFF(1)
const uint8_t pulse_map[8] = {2, 3, 4, 5, 6, 7, 0, 1};

// 5x8 ドットフォント (必要な18文字に拡張)
const uint8_t min_font[18][5] = {
  {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 0: A
  {0x7F, 0x49, 0x49, 0x49, 0x36}, // 1: B
  {0x3E, 0x41, 0x41, 0x41, 0x22}, // 2: C
  {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 3: D
  {0x7F, 0x49, 0x49, 0x49, 0x41}, // 4: E
  {0x7F, 0x09, 0x09, 0x09, 0x01}, // 5: F
  {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 6: G
  {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 7: H
  {0x00, 0x41, 0x7F, 0x41, 0x00}, // 8: I
  {0x7F, 0x40, 0x40, 0x40, 0x40}, // 9: L
  {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 10: M
  {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 11: N
  {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 12: O
  {0x7F, 0x09, 0x19, 0x29, 0x46}, // 13: R
  {0x01, 0x01, 0x7F, 0x01, 0x01}, // 14: T
  {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 15: U
  {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 16: W
  {0x03, 0x0C, 0x70, 0x0C, 0x03}  // 17: Y
};

int getFontIdx(char c) {
  switch(c) {
    case 'A': return 0;  case 'B': return 1;  case 'C': return 2;
    case 'D': return 3;  case 'E': return 4;  case 'F': return 5;
    case 'G': return 6;  case 'H': return 7;  case 'I': return 8;
    case 'L': return 9;  case 'M': return 10; case 'N': return 11;
    case 'O': return 12; case 'R': return 13; case 'T': return 14;
    case 'U': return 15; case 'W': return 16; case 'Y': return 17;
    default: return 12; // 不明な文字は 'O'
  }
}

void initOLED() {
  for(uint8_t i = 0; i < sizeof(init_cmds); i++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(init_cmds[i]);
    Wire.endTransmission();
  }
}

// 文字列長を取得するヘルパー関数
int getStrLen(const char* str) {
  int len = 0;
  while(str[len] != '\0') len++;
  return len;
}

void renderMenu() {
  for (current_page = 0; current_page < 8; current_page++) {
    for (int i = 0; i < 128; i++) page_buffer[i] = 0;
    
    int row = current_page / 2;
    int sub_page = current_page % 2;
    int itemL = row * 2;
    int itemR = row * 2 + 1;
    
    // 枠線の描画 (64x16ピクセル / 1マス)
    for (int x = 0; x < 64; x++) {
      uint8_t bits = 0;
      if (x == 0 || x == 63) bits = 0xFF; // 左右の壁
      else if (sub_page == 0) bits = 0x01; // 上の壁
      else if (sub_page == 1) bits = 0x80; // 下の壁
      
      page_buffer[x] = bits;
      page_buffer[x+64] = bits;
    }
    
    // 均等割＆センタリング描画エンジン
    auto drawStrJustified = [&](int offset, const char* str) {
      int len = getStrLen(str);
      if (len == 0) return;
      
      int total_space = 62 - (5 * len);           // 枠内(62px)で自由に使える空白
      int gap = total_space / (len + 1);          // 文字間と両端の余白の基本単位
      int start_margin = (total_space - gap * (len - 1)) / 2; // 余りを左右の余白に均等配分
      
      int cursor_x = offset + 1 + start_margin;   // 描画開始ピクセル
      
      for (int i = 0; i < len; i++) {
        int font_idx = getFontIdx(str[i]);
        for (int fx = 0; fx < 5; fx++) {
          uint8_t f = min_font[font_idx][fx];
          uint8_t shifted = (sub_page == 0) ? (f << 4) : (f >> 4);
          page_buffer[cursor_x + fx] |= shifted;
        }
        cursor_x += 5 + gap; // 次の文字へ進む
      }
    };
    
    drawStrJustified(0, labels[itemL]);
    drawStrJustified(64, labels[itemR]);
    
    // 選択項目の白黒反転
    if (current_selection == itemL) {
      for(int x = 1; x < 63; x++) {
        uint8_t mask = 0xFF;
        if(sub_page == 0) mask &= ~0x01;
        if(sub_page == 1) mask &= ~0x80;
        page_buffer[x] ^= mask;
      }
    }
    if (current_selection == itemR) {
      for(int x = 65; x < 127; x++) {
        uint8_t mask = 0xFF;
        if(sub_page == 0) mask &= ~0x01;
        if(sub_page == 1) mask &= ~0x80;
        page_buffer[x] ^= mask;
      }
    }
    
    Wire.beginTransmission(OLED_ADDR); Wire.write(0x00); Wire.write(0xB0 + current_page); Wire.endTransmission();
    Wire.beginTransmission(OLED_ADDR); Wire.write(0x00); Wire.write(0x00); Wire.endTransmission();
    Wire.beginTransmission(OLED_ADDR); Wire.write(0x00); Wire.write(0x10); Wire.endTransmission();
    for (uint8_t i = 0; i < 128; i += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40);
      for (uint8_t j = 0; j < 16; j++) {
        Wire.write(page_buffer[i + j]);
      }
      Wire.endTransmission();
    }
  }
}

void executeColor(uint8_t menu_index) {
  uint8_t target_pulses = pulse_map[menu_index];

  // 1. 完全リセット (ICの電源を20ms遮断して初期状態へ戻す)
  digitalWrite(LED_PIN, LOW);
  delay(20);
  digitalWrite(LED_PIN, HIGH);
  delay(2); 
  
  // 2. 必要な回数だけ超高速パルスを発射してスキップ
  for (uint8_t i = 0; i < target_pulses; i++) {
    digitalWrite(LED_PIN, LOW);
    delayMicroseconds(60);
    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(100);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(SET_PIN, INPUT_PULLUP);
  
  delay(100); 
  Wire.begin();
  initOLED();
  
  renderMenu();
  executeColor(current_selection); // 起動直後に自動的にOFFを実行
}

void loop() {
  // SELECT ボタン処理
  bool select_state = digitalRead(SELECT_PIN);
  if (select_state == LOW && last_select == HIGH) {
    delay(20); // デバウンス
    if (digitalRead(SELECT_PIN) == LOW) {
      current_selection = (current_selection + 1) % 8;
      renderMenu();
    }
  }
  last_select = select_state;

  // SET ボタン処理
  bool set_state = digitalRead(SET_PIN);
  if (set_state == LOW && last_set == HIGH) {
    delay(20); // デバウンス
    if (digitalRead(SET_PIN) == LOW) {
      executeColor(current_selection);
    }
  }
  last_set = set_state;
}
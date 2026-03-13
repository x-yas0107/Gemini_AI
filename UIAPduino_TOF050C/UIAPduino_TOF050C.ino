/*
 * 名前: UIAPduino_TOF050C
 * バージョン: V1.00
 * 日付: 2026-03-13
 * 開発者: Gemini & yas
 * * ピンアサイン:
 * - PD5: キャリブレーション・スイッチ (50mm目標)
 * - PD6: キャリブレーション・スイッチ (150mm目標)
 * - I2C: PC1 (SDA), PC2 (SCL) [CH32V003標準]
 *
 * 説明:
 * VL6180X (TOF050C) センサーを使用した高精度距離測定システム。
 * リアルタイムの距離履歴グラフと、移動方向を認識するジェスチャー検出機能を備えています。
 * センサーの放射状の視野（FoV）を空間的なプロファイルとして捉え、
 * 物体がエリアに「入る（接近）」のか「出る（離脱）」のかを判定します。
 *
 * キャリブレーション方法:
 * 1. 平らな物体を50mmの距離に置き、PD5ボタンを押します。
 * 2. 物体を150mmの距離に移動させ、PD6ボタンを押します。
 * 3. 画面上の '50ok' および '150ok' インジケータで完了を確認します。
 */

#include <Wire.h>

// --- 定数定義 ---
#define CALIB_50_PIN  PD5
#define CALIB_150_PIN PD6
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_I2C_ADDR 0x3C
#define VL6180X_I2C_ADDR 0x29

// --- バッファ定義 ---
uint8_t page_buf[SCREEN_WIDTH];  // OLED ページバッファ (128x8 ピクセル/ページ)
uint8_t history[SCREEN_WIDTH];   // グラフ用距離履歴データ

// --- グローバル状態変数 ---
uint8_t current_render_page = 0;
uint8_t current_range = 255;
uint8_t last_range = 255;
uint8_t gesture_mode = 0; // 0:なし, 1:進入(←), 2:離脱(→)
unsigned long gesture_display_timer = 0;
char dist_str[4]; // 表示用文字列バッファ (レンダリングの高速化用)

// --- 校正データ ---
int16_t cal_R1 = 50;  
int16_t cal_R2 = 150; 
bool cal1_ok = false;
bool cal2_ok = false;

// --- 5x7 フォントテーブル ---
// インデックス: 0-9(数字), 10(-), 11(m), 12(>), 13(<), 14(:), 15(o), 16(k), 17(← 先端), 18(→ 先端)
const uint8_t font5x7[][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
  {0x08, 0x08, 0x08, 0x08, 0x08}, // - (10) シャフト部品
  {0x7C, 0x04, 0x18, 0x04, 0x78}, // m (11)
  {0x00, 0x41, 0x22, 0x14, 0x08}, // > (12)
  {0x08, 0x14, 0x22, 0x41, 0x00}, // < (13)
  {0x00, 0x00, 0x24, 0x00, 0x00}, // : (14)
  {0x38, 0x44, 0x44, 0x44, 0x38}, // o (15)
  {0x7F, 0x08, 0x14, 0x22, 0x41}, // k (16)
  {0x08, 0x1C, 0x3E, 0x08, 0x08}, // ← 先端 (17)
  {0x08, 0x08, 0x3E, 0x1C, 0x08}  // → 先端 (18)
};

// --- VL6180X 初期化シーケンス ---
struct VL_Init { uint16_t reg; uint8_t val; };
const VL_Init vl_init_data[] = {
  {0x0207, 0x01}, {0x0208, 0x01}, {0x0096, 0x00}, {0x0097, 0xFD},
  {0x00E3, 0x00}, {0x00E4, 0x04}, {0x00E5, 0x02}, {0x00E6, 0x01},
  {0x00E7, 0x03}, {0x00F5, 0x02}, {0x00D9, 0x05}, {0x00DB, 0xCE},
  {0x00DC, 0x03}, {0x00DD, 0xF8}, {0x009F, 0x00}, {0x00A3, 0x3C},
  {0x00B7, 0x00}, {0x00BB, 0x3C}, {0x00B2, 0x09}, {0x00CA, 0x09},
  {0x0198, 0x01}, {0x01B0, 0x17}, {0x01AD, 0x00}, {0x00FF, 0x05},
  {0x0100, 0x05}, {0x0199, 0x05}, {0x01A6, 0x1B}, {0x01AC, 0x3E},
  {0x01A7, 0x1F}, {0x0030, 0x00}, {0x0011, 0x10}, {0x010A, 0x30},
  {0x003F, 0x46}, {0x0031, 0xFF}, {0x0040, 0x63}, {0x002E, 0x01},
  {0x001B, 0x09}, {0x003E, 0x31}, {0x0014, 0x24}, {0x0016, 0x00},
  {0x001C, 0x3F}  
};

// --- 低レベルI2C/センサー制御 ---
void writeVL(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(VL6180X_I2C_ADDR);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readVL(uint16_t reg) {
  Wire.beginTransmission(VL6180X_I2C_ADDR);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)VL6180X_I2C_ADDR, (uint8_t)1);
  return Wire.read();
}

uint8_t getRawRange() {
  writeVL(0x0018, 0x01); // 測定開始
  while ((readVL(0x004F) & 0x04) == 0); // 完了待ち
  uint8_t raw = readVL(0x0062);
  writeVL(0x0015, 0x07); // 割り込みクリア
  return raw;
}

uint8_t readCalibratedRange() {
  uint8_t raw = getRawRange();
  if (raw == 255) return 255;
  // 50mm-150mmの2点校正を適用
  int32_t dist = 50 + (int32_t)(raw - cal_R1) * 100 / (cal_R2 - cal_R1);
  if (dist < 0) return 0;
  if (dist > 250) return 250;
  return (uint8_t)dist;
}

// --- OLED レンダリング関数 ---
void oled_cmd(uint8_t c) {
  Wire.beginTransmission(OLED_I2C_ADDR);
  Wire.write(0x00);
  Wire.write(c);
  Wire.endTransmission();
}

void oled_init() {
  const uint8_t init_sequence[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
    0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
  };
  for(uint8_t i = 0; i < sizeof(init_sequence); i++) oled_cmd(init_sequence[i]);
}

void drawPixel(int16_t x, int16_t y) {
  if ((x >= 0) && (x < SCREEN_WIDTH) && (y >= (current_render_page * 8)) && (y < ((current_render_page + 1) * 8))) {
    page_buf[x] |= (1 << (y & 7));
  }
}

void drawChar(int16_t x, int16_t y, char c, bool scale_x2) {
  uint8_t idx = 255;
  if (c >= '0' && c <= '9') idx = c - '0';
  else if (c == '-') idx = 10;
  else if (c == 'm') idx = 11;
  else if (c == 'o') idx = 15;
  else if (c == 'k') idx = 16;
  else if (c == 'L') idx = 17; 
  else if (c == 'R') idx = 18; 
  
  if (idx == 255) return; 
  for (int8_t i = 0; i < 5; i++) {
    uint8_t line = font5x7[idx][i];
    for (int8_t j = 0; j < 7; j++) {
      if (line & 0x1) {
        if (scale_x2) {
          drawPixel(x + i * 2, y + j * 2);
          drawPixel(x + i * 2 + 1, y + j * 2);
          drawPixel(x + i * 2, y + j * 2 + 1);
          drawPixel(x + i * 2 + 1, y + j * 2 + 1);
        } else { drawPixel(x + i, y + j); }
      }
      line >>= 1;
    }
  }
}

void draw_all_elements() {
  // 中央の距離表示 (事前に計算された文字列を使用)
  int len = (current_range >= 100) ? 3 : (current_range >= 10 ? 2 : 1);
  int16_t x_dist = (128 - (len * 12 + 16)) / 2;
  if (current_range == 255) {
    drawChar(x_dist, 0, '-', true); drawChar(x_dist + 12, 0, '-', true);
  } else {
    for(int i=0; dist_str[i]; i++) drawChar(x_dist + i*12, 0, dist_str[i], true);
    drawChar(x_dist + len * 12 + 2, 6, 'm', false);
    drawChar(x_dist + len * 12 + 8, 6, 'm', false);
  }

  // キャリブレーション情報
  drawChar(0, 0, '5', false); drawChar(6, 0, '0', false);
  if (cal1_ok) { drawChar(14, 0, 'o', false); drawChar(20, 0, 'k', false); }

  int16_t x_cal_r = 98;
  drawChar(x_cal_r, 0, '1', false); drawChar(x_cal_r + 6, 0, '5', false); drawChar(x_cal_r + 12, 0, '0', false);
  if (cal2_ok) { drawChar(x_cal_r + 20, 0, 'o', false); drawChar(x_cal_r + 26, 0, 'k', false); }

  // ジェスチャー表示 (接近 vs 離脱)
  if (gesture_mode == 1) { // 左矢印 (進入)
    drawChar(2, 11, 'L', true); drawChar(12, 11, '-', true);
  } else if (gesture_mode == 2) { // 右矢印 (離脱)
    drawChar(108, 11, '-', true); drawChar(118, 11, 'R', true);
  }

  // 画面分割線
  for(int16_t x = 0; x < 128; x++) drawPixel(x, 27);

  // 履歴グラフ描画
  for(int x = 0; x < SCREEN_WIDTH; x++) {
    int y = 63 - (history[x] * 34 / 250); 
    if (y < 29) y = 29; if (y > 63) y = 63;
    drawPixel(x, y);
    // グラフ下の塗りつぶしパターン
    for(int fill_y = y + 1; fill_y <= 63; fill_y++) {
      if ((x + fill_y) % 5 == 0 || (x - fill_y + 128) % 5 == 0) drawPixel(x, fill_y);
    }
  }
}

// --- メインループ ---
void setup() {
  pinMode(CALIB_50_PIN, INPUT_PULLUP);
  pinMode(CALIB_150_PIN, INPUT_PULLUP);
  Wire.begin(); 
  Wire.setClock(400000); // 表示速度向上のためI2Cを400kHzに設定
  oled_init();
  
  // VL6180X 初期化
  if (readVL(0x0016) == 1) { 
    for (int i = 0; i < sizeof(vl_init_data) / sizeof(VL_Init); i++) writeVL(vl_init_data[i].reg, vl_init_data[i].val); 
  }
  for(int i = 0; i < SCREEN_WIDTH; i++) history[i] = 255; 
}

void loop() {
  // 校正ボタンの処理
  if (digitalRead(CALIB_50_PIN) == LOW) { cal_R1 = getRawRange(); cal1_ok = true; delay(500); }
  if (digitalRead(CALIB_150_PIN) == LOW) { cal_R2 = getRawRange(); cal2_ok = true; delay(500); }

  // 距離の読み取りと推移の記録
  last_range = current_range;
  current_range = readCalibratedRange();
  
  // 表示用文字列の事前生成 (ページループ外で行い高速化)
  if (current_range != 255) itoa(current_range, dist_str, 10);

  // 進入/離脱ジェスチャー判定
  if (current_range < 140) {
    if (current_range < last_range - 2) { gesture_mode = 1; gesture_display_timer = millis(); }
    else if (current_range > last_range + 2 && last_range != 255) { gesture_mode = 2; gesture_display_timer = millis(); }
  }
  // 表示タイマー管理
  if (gesture_mode > 0 && (millis() - gesture_display_timer > 800)) gesture_mode = 0;

  // 履歴バッファの更新 (シフト)
  for(int i = 0; i < SCREEN_WIDTH - 1; i++) history[i] = history[i+1];
  history[SCREEN_WIDTH - 1] = current_range;

  // OLEDへのページ単位レンダリング
  for(current_render_page = 0; current_render_page < 8; current_render_page++) {
    for(int i = 0; i < 128; i++) page_buf[i] = 0;
    draw_all_elements();
    oled_cmd(0xB0 + current_render_page); oled_cmd(0x00); oled_cmd(0x10);
    // I2Cの安定性と速度を考慮した16バイト単位の転送
    for(uint8_t x = 0; x < 128; x += 16) {
      Wire.beginTransmission(OLED_I2C_ADDR); Wire.write(0x40);
      for(uint8_t i = 0; i < 16; i++) Wire.write(page_buf[x + i]);
      Wire.endTransmission();
    }
  }
}
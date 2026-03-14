/*
 * ==============================================================================
 * 名前: UIAPduino_TOF400C (VL53L1X)
 * バージョン: V0.30
 * 日付: 2026-03-14
 * 開発者: Gemini & yas
 *
 * [ピンアサイン]
 * - PC1: SDA (OLED & VL53L1X)
 * - PC2: SCL (OLED & VL53L1X)
 * - PD3: GPIO1 (測定完了割り込み) - ※未使用
 *
 * [簡単な説明]
 * UM2356 マニュアルの Ranging Flow (Figure 5) に完全準拠 [cite: 124]。
 * 「数値の反転」と「E-8/9 (Processing Fail)」を解決するため、
 * ST標準の Short Mode (最大1.3m) と Timing Budget (20ms) に固定 [cite: 219, 247]。
 * Repeated Start を継続使用し、I2C ポインタのズレを物理的に阻止。
 *
 * [改変履歴]
 * V0.00 - V0.28: 割愛。
 * V0.29: Repeated Start 導入。
 * V0.30:
 * - UM2356 Table 4 に基づき E-8 (Processing Fail) 対策を実施 [cite: 447]。
 * - Distance Mode を Short に固定し、近距離(50cm)の安定度を向上 [cite: 247]。
 * - Timing Budget を 20ms (0x14) に設定し、内部計算パンクを回避 [cite: 219]。
 * - 読み出し後に 0x0086 を叩くマニュアル通りの順序を徹底 [cite: 124]。
 * ==============================================================================
 */

#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_I2C_ADDR 0x3C
#define VL53L1X_I2C_ADDR 0x29 

#define SOFT_RESET 0x0000
#define FIRMWARE__SYSTEM_STATUS 0x00E5
#define IDENTIFICATION__MODEL_ID 0x010F
#define SYSTEM__INTERRUPT_CLEAR 0x0086
#define SYSTEM__MODE_START 0x0087
#define GPIO__TIO_HV_STATUS 0x0031
#define RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0 0x0096
#define RESULT__RANGE_STATUS 0x0089

uint8_t page_buf[SCREEN_WIDTH];
uint16_t current_dist = 0;
bool sensor_ready = false;
uint8_t active_arrow = 11; 
char dist_str[6] = "----";
uint8_t debug_state = 0;

typedef struct {
  uint16_t addr;
  uint8_t val;
} RegConfig;

// V0.30: UM2356 準拠、Short Mode / 20ms 最適化テーブル
const RegConfig init_table[] = {
  {0x002E, 0x01}, {0x0024, 0x0A}, {0x0031, 0x02}, {0x0036, 0x08},
  {0x0037, 0x10}, {0x0039, 0x01}, {0x003E, 0xFF}, {0x003F, 0x00},
  {0x0040, 0x02}, {0x0050, 0x00}, {0x0051, 0x00}, {0x0052, 0x00},
  {0x0053, 0x00}, {0x0054, 0xC8}, {0x0055, 0x00}, {0x0057, 0x38},
  {0x0064, 0x01}, {0x0065, 0x68}, {0x0066, 0x00}, {0x0067, 0xC0},
  {0x0071, 0x01}, {0x007C, 0x01}, {0x007E, 0x02}, {0x0082, 0x00},
  {0x0077, 0x01}, {0x0081, 0x8B}, 
  {0x004F, 0x02}, // Timing Budget パラメータ
  {0x004E, 0x14}, // Timing Budget: 20ms [cite: 219]
  {0x0060, 0x07}, // VCSEL Period A (Short) [cite: 247]
  {0x0063, 0x05}, // VCSEL Period B (Short) [cite: 247]
  {0x0069, 0x38}, 
  {0x0078, 0x07}, {0x0079, 0x05}, // Phase Thresholds
  {0x007A, 0x06}, {0x007B, 0x06} 
};

void writeReg(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(VL53L1X_I2C_ADDR);
  Wire.write(reg >> 8); Wire.write(reg & 0xFF); Wire.write(val);
  Wire.endTransmission();
}

void writeReg16(uint16_t reg, uint16_t val) {
  Wire.beginTransmission(VL53L1X_I2C_ADDR);
  Wire.write(reg >> 8); Wire.write(reg & 0xFF);
  Wire.write(val >> 8); Wire.write(val & 0xFF);
  Wire.endTransmission();
}

uint8_t readReg(uint16_t reg) {
  Wire.beginTransmission(VL53L1X_I2C_ADDR);
  Wire.write(reg >> 8); Wire.write(reg & 0xFF);
  Wire.endTransmission(false); // Repeated Start
  Wire.requestFrom(VL53L1X_I2C_ADDR, 1);
  return Wire.read();
}

uint16_t readReg16(uint16_t reg) {
  Wire.beginTransmission(VL53L1X_I2C_ADDR);
  Wire.write(reg >> 8); Wire.write(reg & 0xFF);
  Wire.endTransmission(false); // Repeated Start
  Wire.requestFrom(VL53L1X_I2C_ADDR, 2);
  uint16_t val = Wire.read() << 8;
  val |= Wire.read();
  return val;
}

void oled_cmd(uint8_t c) {
  Wire.beginTransmission(OLED_I2C_ADDR);
  Wire.write(0x00); Wire.write(c);
  Wire.endTransmission();
}

uint8_t current_page = 0;
void drawPixel(int x, int y) {
  if (x >= 0 && x < 128 && y >= current_page * 8 && y < (current_page + 1) * 8) {
    page_buf[x] |= (1 << (y & 7));
  }
}

void drawLargeArrow(int x, int y, uint8_t dir) {
  for (int i = 0; i < 15; i++) { 
    for (int j = -4; j <= 4; j++) { 
      bool paint = false;
      if (i < 5) { if (abs(j) <= i) paint = true; }
      else { if (abs(j) <= 1) paint = true; }
      if (paint) {
        if (dir == 11) drawPixel(x + j, y - (7 - i));
        else if (dir == 12) drawPixel(x + j, y + (7 - i));
        else if (dir == 13) drawPixel(x - (7 - i), y + j);
        else if (dir == 14) drawPixel(x + (7 - i), y + j);
      }
    }
  }
}

void drawChar(int x, int y, uint8_t idx) {
  const uint8_t font[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x08, 0x08, 0x08, 0x08, 0x08}, {0x7F, 0x49, 0x49, 0x49, 0x41} 
  };
  for(int i=0; i<5; i++) {
    uint8_t line = font[idx][i];
    for(int j=0; j<7; j++) {
      if (line & 0x1) {
        drawPixel(x+i*2, y+j*2); drawPixel(x+i*2+1, y+j*2);
        drawPixel(x+i*2, y+j*2+1); drawPixel(x+i*2+1, y+j*2+1);
      }
      line >>= 1;
    }
  }
}

void quick_debug(uint8_t state) {
  if (debug_state == state) return;
  debug_state = state;
  uint8_t old_page = current_page;
  current_page = 0;
  for (int i = 0; i < 16; i++) page_buf[i] = 0;
  drawChar(0, 0, state / 10); drawChar(8, 0, state % 10);
  oled_cmd(0xB0); oled_cmd(0x00); oled_cmd(0x10);
  Wire.beginTransmission(OLED_I2C_ADDR);
  Wire.write(0x40);
  for (int i = 0; i < 16; i++) Wire.write(page_buf[i]);
  Wire.endTransmission();
  current_page = old_page;
}

void render_frame() {
  for(int i=0; i<=8; i++) {
    int vx = 64 + i*8; if(vx > 127) vx = 127;
    for(int y=0; y<64; y++) drawPixel(vx, y);
    int hy = i*8; if(hy > 63) hy = 63;
    for(int x=64; x<128; x++) drawPixel(x, hy);
  }
  drawChar(0, 0, debug_state / 10); drawChar(8, 0, debug_state % 10);
  
  if (current_dist > 0 && current_dist < 2000) {
    int px = 96; int py = 63 - (current_dist / 31);
    if (py < 0) py = 0; if (py > 63) py = 63;
    drawPixel(px, py); drawPixel(px-1, py); drawPixel(px+1, py);
    drawPixel(px, py-1); drawPixel(px, py+1);
  }
  
  drawLargeArrow(32, 22, active_arrow);
  for(int i=0; dist_str[i]; i++) {
    uint8_t c;
    if (dist_str[i] == '-') c = 10;
    else if (dist_str[i] == 'E') c = 11;
    else c = dist_str[i] - '0';
    drawChar(8 + i*12, 44, c);
  }
}

bool sensor_init() {
  quick_debug(21);
  if (readReg16(IDENTIFICATION__MODEL_ID) != 0xEACC) return false; 
  
  quick_debug(22);
  writeReg(SOFT_RESET, 0x00); delayMicroseconds(100);
  writeReg(SOFT_RESET, 0x01); delay(5);
  while ((readReg(FIRMWARE__SYSTEM_STATUS) & 0x01) == 0);
  
  quick_debug(24);
  for(uint8_t i=0; i < (sizeof(init_table)/sizeof(RegConfig)); i++) {
    writeReg(init_table[i].addr, init_table[i].val);
  }

  quick_debug(25);
  uint8_t vhv_init = readReg(0x000B);
  writeReg(0x000B, vhv_init & 0x7F); 
  writeReg(0x004D, 0x01); 
  
  writeReg(SYSTEM__INTERRUPT_CLEAR, 0x01);
  delay(50); 
  
  quick_debug(26); 
  writeReg(SYSTEM__MODE_START, 0x10); 
  return true;
}

void setup() {
  Wire.begin(); Wire.setClock(400000);
  quick_debug(10);
  const uint8_t oled_init[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
  for(uint8_t i=0; i<sizeof(oled_init); i++) oled_cmd(oled_init[i]);
  quick_debug(11);
  sensor_ready = sensor_init();
}

void loop() {
  quick_debug(70);
  bool trigger_next = false;
  
  if (sensor_ready) {
    uint8_t wait_count = 0;
    // Figure 5: Wait for data ready [cite: 143]
    while((readReg(GPIO__TIO_HV_STATUS) & 0x01) != 0 && wait_count < 100) {
      wait_count++; delay(1);
    }

    quick_debug(72);
    // Figure 5: Get Ranging Measurement Data [cite: 148]
    uint16_t dist_raw = readReg16(RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM_SD0);
    uint8_t range_status = readReg(RESULT__RANGE_STATUS);

    // V0.30: Valid(0), Signal(2), Wrap(7) を表示対象とする [cite: 447]
    if (range_status == 0 || range_status == 2 || range_status == 7) {
      current_dist = dist_raw;
      itoa(current_dist, dist_str, 10);
    } else {
      dist_str[0] = 'E';
      dist_str[1] = '-';
      itoa(range_status, &dist_str[2], 10);
    }

    // Figure 5: Clear Interrupt [cite: 150]
    writeReg(SYSTEM__INTERRUPT_CLEAR, 0x01);
    trigger_next = true;
  }

  for (current_page = 0; current_page < 8; current_page++) {
    for (int i = 0; i < 128; i++) page_buf[i] = 0;
    render_frame();
    oled_cmd(0xB0 + current_page); oled_cmd(0x00); oled_cmd(0x10);
    Wire.beginTransmission(OLED_I2C_ADDR); Wire.write(0x40);
    for (int i = 0; i < 128; i++) {
      Wire.write(page_buf[i]);
      if ((i + 1) % 16 == 0) { Wire.endTransmission(); Wire.beginTransmission(OLED_I2C_ADDR); Wire.write(0x40); }
    }
    Wire.endTransmission();
  }
  
  quick_debug(99);

  if (trigger_next) {
    quick_debug(81); 
    delay(10); 
    // 次の測定開始を明示的に指示
    writeReg(SYSTEM__MODE_START, 0x10); 
  }
}
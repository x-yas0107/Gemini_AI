/*
 * Project Name : AS5600 I2C to UART Angle Converter (Lightweight Version)
 * Target MCU   : CH32V003A4M6 (16-pin)
 * Development  : Arduino IDE (WCH Official Core)
 * Version      : Ver 0.05
 * 
 * [Pin Assignment]
 * - I2C SDA    : PC1 (Pin 1)
 * - I2C SCL    : PC2 (Pin 2)
 * - UART TX    : PD5 (Pin 9)
 * - UART RX    : PD6 (Pin 10)
 * 
 * [Change History]
 * - Ver 0.01 : Initial (Wire.h + float → Flash overflow)
 * - Ver 0.02 : Direct register (wrong names)
 * - Ver 0.03 : Correct register names + GPIO/clock init
 * - Ver 0.04 : Timeout + safer I2C sequence + clock note
 * - Ver 0.05 : Fix ACK re-enable, STOP timing, error recovery
 */
#include <Arduino.h>

// AS5600 I2C設定 (7-bit address 0x36)
#define AS5600_ADDR       0x36
#define AS5600_REG_ANGLE  0x0C

// 通信・タイマー設定
const unsigned long SEND_INTERVAL = 100; // 100ms
unsigned long previousMillis = 0;

// タイムアウト用
#define I2C_TIMEOUT  20000

// I2Cソフトウェアリセット（エラー復帰用）
void i2c_reset() {
  I2C1->CTLR1 |= I2C_CTLR1_SWRST;
  I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;
  
  // 再初期化（クロック設定はそのまま）
  I2C1->CTLR2  = 48;     // ★ 24MHzの場合は24に変更
  I2C1->CKCFGR = 240;    // ★ 24MHzの場合は120に変更
  I2C1->CTLR1 |= I2C_CTLR1_PE | I2C_CTLR1_ACK;
}

// I2C初期化（CH32V003向け）
void i2c_init() {
  // クロック有効化
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
  RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

  // PC1(SDA), PC2(SCL) を Alternate Function Open-Drain 10MHz
  GPIOC->CFGLR &= ~((0x0F << 4) | (0x0F << 8));
  GPIOC->CFGLR |=  ((0x0D << 4) | (0x0D << 8));

  // I2Cリセット＆設定
  i2c_reset();
}

bool i2c_read_angle(uint16_t *angle) {
  uint32_t timeout;

  // ===== 通信開始前に必ずACKを有効化 =====
  I2C1->CTLR1 |= I2C_CTLR1_ACK;

  // ===== BUSYチェック（前回の通信が残っていないか） =====
  timeout = I2C_TIMEOUT;
  while ((I2C1->STAR2 & I2C_STAR2_BUSY) && --timeout);
  if (!timeout) {
    i2c_reset();
    return false;
  }

  // ===== START =====
  I2C1->CTLR1 |= I2C_CTLR1_START;
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_SB) && --timeout);
  if (!timeout) { i2c_reset(); return false; }

  // ===== アドレス送信（書き込み） =====
  I2C1->DATAR = (AS5600_ADDR << 1);
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_ADDR) && --timeout);
  if (!timeout) { i2c_reset(); return false; }
  (void)I2C1->STAR1;  // ADDRクリア
  (void)I2C1->STAR2;

  // ===== レジスタアドレス送信 =====
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_TXE) && --timeout);
  if (!timeout) { i2c_reset(); return false; }
  I2C1->DATAR = AS5600_REG_ANGLE;

  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_BTF) && --timeout);
  if (!timeout) { i2c_reset(); return false; }

  // ===== Repeated START =====
  I2C1->CTLR1 |= I2C_CTLR1_START;
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_SB) && --timeout);
  if (!timeout) { i2c_reset(); return false; }

  // ===== アドレス送信（読み込み） =====
  I2C1->DATAR = (AS5600_ADDR << 1) | 0x01;
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_ADDR) && --timeout);
  if (!timeout) { i2c_reset(); return false; }
  (void)I2C1->STAR1;  // ADDRクリア
  (void)I2C1->STAR2;

  // ===== 1バイト目受信（ACK） =====
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_RXNE) && --timeout);
  if (!timeout) { i2c_reset(); return false; }
  uint8_t highByte = I2C1->DATAR;

  // ===== ★ 1バイト目受信直後に NACK + STOP を予約 =====
  I2C1->CTLR1 &= ~I2C_CTLR1_ACK;
  I2C1->CTLR1 |= I2C_CTLR1_STOP;

  // ===== 2バイト目受信 =====
  timeout = I2C_TIMEOUT;
  while (!(I2C1->STAR1 & I2C_STAR1_RXNE) && --timeout);
  if (!timeout) { i2c_reset(); return false; }
  uint8_t lowByte = I2C1->DATAR;

  // 12ビット値に結合
  *angle = (((uint16_t)highByte << 8) | lowByte) & 0x0FFF;
  return true;
}

void setup() {
  Serial.begin(9600);
  i2c_init();

  Serial.println("AS5600 ready");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= SEND_INTERVAL) {
    previousMillis = currentMillis;

    uint16_t rawAngle = 0;

    if (i2c_read_angle(&rawAngle)) {
      // 整数演算で 0.0〜359.9 度
      uint32_t totalTenths = ((uint32_t)rawAngle * 3600UL) / 4096UL;
      uint16_t integerPart = totalTenths / 10;
      uint16_t decimalPart = totalTenths % 10;

      Serial.print(integerPart);
      Serial.print('.');
      Serial.println(decimalPart);
    } else {
      Serial.println("ERR:I2C");
    }
  }
}

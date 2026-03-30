/*
 * UIAPduino_CineStream.ino
 * Version: 0.12
 * Date: 2026-03-31
 * Description: SPI Flashからの動画＆音声ストリーミング再生 (16MBフル容量解放版)
 * Change History:
 * - V0.00: RAM枯渇対策のためSPIからの直接転送に変更。
 * - V0.01: Wire.hとSPI.hを廃止。
 * - V0.02: Bad Appleのエンジンを移植。
 * - V0.03: データの並び順を修正、oled_clear()追加。
 * - V0.04: レジスタ直叩きによるPWM実装(コンパイルエラー発生)。
 * - V0.05: 512Bバッファ採用、映像音声同期処理を実装。
 * - V0.06: 音声出力ピンをPC4(TIM1_CH4)へ修正、BDTR設定追加。
 * - V0.07: 再生速度とピッチの正常化(delayMicroseconds追加)。
 * - V0.08: OLEDの向きを正位置(0xC0, 0xA0)に明示的に設定し反転を完全修正。
 * - V0.09: 動画再生速度(FPS)と音程を上げるため、同期ウェイトを55usから20usに短縮。
 * - V0.10: ダブルバッファ実装。TIM2ストップウォッチによる完全自動再生を追加。
 * - V0.11: OLEDコマンドを0xC0/0xA0に復元し画面反転を再修正。音声同期をバイト単位に軽減。
 * - V0.12: W25Q128の最大容量(16MB)までmaxAddressを拡張。最大約6分4秒の動画再生に対応。
 */

#include <Arduino.h>

// --- ピン定義 ---
#define SPI_CS_BIT   3  // PC3 (Flash CS)
#define SPI_SCK_BIT  5  // PC5 (Flash SCK)
#define SPI_MOSI_BIT 6  // PC6 (Flash MOSI)
#define SPI_MISO_BIT 7  // PC7 (Flash MISO)

#define TGT_SDA_BIT  1  // PC1 (OLED SDA)
#define TGT_SCL_BIT  2  // PC2 (OLED SCL)

// --- 定数 ---
#define FRAME_SIZE 1536
#define AUDIO_SIZE 512
#define OLED_SIZE  1024

uint32_t currentAddress = 0;
uint32_t maxAddress = 16777216; // 16MB (V0.12: 最大容量解放)

// --- ダブルバッファ(音声途切れ防止) ---
uint8_t audio_buf[2][512]; 
volatile uint8_t play_buf = 0;
volatile uint8_t load_buf = 0;
volatile uint16_t play_ptr = 0;
bool playing = false;

// --- Timer1 PWM Setup (PC4 / TIM1_CH4) ---
void pwm_init() {
  RCC->APB2PCENR |= (1 << 4) | (1 << 11); 
  GPIOC->CFGLR &= ~(0xF << (4 * 4));
  GPIOC->CFGLR |=  (0xB << (4 * 4)); 
  TIM1->PSC = 0;             
  TIM1->ATRLR = 255;         
  TIM1->CHCTLR2 |= 0x6000;   
  TIM1->CCER |= 0x1000;      
  TIM1->BDTR |= 0x8000;      
  TIM1->CTLR1 |= 0x01;       
}

inline void set_audio_sample(uint8_t val) {
  TIM1->CH4CVR = val;
}

// --- Timer2 Stopwatch Setup (1us tick) ---
void stopwatch_init() {
  RCC->APB1PCENR |= (1 << 0);
  TIM2->PSC = 47;             // 48MHz / 48 = 1MHz (1us)
  TIM2->ATRLR = 65535;
  TIM2->CTLR1 |= 0x01;
}

// --- 完全同期 オーディオ再生エンジン ---
inline void poll_audio() {
  if(!playing) return;
  if(TIM2->CNT >= 65) {      // 65us経過チェック (約15.38kHz)
    TIM2->CNT -= 65;         // タイマーをリセット(余剰分を残してズレ防止)
    set_audio_sample(audio_buf[play_buf][play_ptr]);
    play_ptr++;
    if(play_ptr >= 512) {
      play_ptr = 0;
      play_buf ^= 1;         // 次のバッファへ自動切り替え
    }
  }
}

// --- SPI Engine (W25Q128) ---
void spi_init() {
  RCC->APB2PCENR |= (1 << 4); 
  GPIOC->CFGLR &= ~((0xF << (4*3)) | (0xF << (4*5)) | (0xF << (4*6)) | (0xF << (4*7)));
  GPIOC->CFGLR |=  ((0x3 << (4*3)) | (0x3 << (4*5)) | (0x3 << (4*6)) | (0x8 << (4*7)));
  GPIOC->OUTDR |= (1 << SPI_CS_BIT) | (1 << SPI_MISO_BIT);
}

inline uint8_t spi_transfer(uint8_t data) {
  uint8_t rx = 0;
  for(uint8_t i=0; i<8; i++) {
    if(data & 0x80) GPIOC->OUTDR |= (1 << SPI_MOSI_BIT);
    else GPIOC->OUTDR &= ~(1 << SPI_MOSI_BIT);
    data <<= 1;
    GPIOC->OUTDR |= (1 << SPI_SCK_BIT);
    rx <<= 1;
    if(GPIOC->INDR & (1 << SPI_MISO_BIT)) rx |= 1;
    GPIOC->OUTDR &= ~(1 << SPI_SCK_BIT);
  }
  poll_audio(); // 通信の隙間に音声を更新
  return rx;
}

// --- I2C Bit-bang High Speed (SSD1306) ---
void i2c_gpio_init() {
  RCC->APB2PCENR |= (1 << 4); 
  GPIOC->CFGLR &= ~((0xF << (4*1)) | (0xF << (4*2)));
  GPIOC->CFGLR |=  ((0x3 << (4*1)) | (0x3 << (4*2))); 
  GPIOC->OUTDR |= (1 << 1) | (1 << 2);
}

inline void sda_high() { GPIOC->OUTDR |= (1<<TGT_SDA_BIT); }
inline void sda_low()  { GPIOC->OUTDR &= ~(1<<TGT_SDA_BIT); }
inline void scl_high() { GPIOC->OUTDR |= (1<<TGT_SCL_BIT); }
inline void scl_low()  { GPIOC->OUTDR &= ~(1<<TGT_SCL_BIT); }

inline void i2c_start() { sda_high(); scl_high(); sda_low(); scl_low(); }
inline void i2c_stop()  { sda_low(); scl_high(); sda_high(); }

inline void i2c_write(uint8_t data) {
  for(uint8_t i=0; i<8; i++){ 
    if(data&0x80) sda_high(); else sda_low(); 
    scl_high(); scl_low(); data<<=1; 
  }
  sda_high(); scl_high(); scl_low(); 
  poll_audio(); // 描画の隙間に音声を更新
}

// --- OLED Control ---
void oled_cmd(uint8_t c) { 
  i2c_start(); i2c_write(0x3C<<1); i2c_write(0x00); i2c_write(c); i2c_stop(); 
}

void oled_init() { 
  delay(100); 
  oled_cmd(0xAE); oled_cmd(0x20); oled_cmd(0x02); 
  oled_cmd(0xC0); oled_cmd(0xA0); 
  oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); 
}

void oled_clear() {
  for(uint8_t p = 0; p < 8; p++) {
    oled_cmd(0xB0 + p); oled_cmd(0x00); oled_cmd(0x10);
    i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
    for(uint16_t x = 0; x < 128; x++) i2c_write(0x00); 
    i2c_stop();
  }
}

void setup() {
  i2c_gpio_init();
  spi_init();
  pwm_init();
  stopwatch_init();
  oled_init();
  oled_clear();

  // 初期ストリームの準備 (最初の512B音声を読み込む)
  GPIOC->OUTDR &= ~(1 << SPI_CS_BIT); 
  spi_transfer(0x03);                 
  spi_transfer(0); spi_transfer(0); spi_transfer(0);

  for(uint16_t i = 0; i < AUDIO_SIZE; i++) {
    audio_buf[0][i] = spi_transfer(0x00) ^ 0x80;
  }
  
  // エンジン始動
  play_buf = 0;
  load_buf = 0;
  play_ptr = 0;
  TIM2->CNT = 0;
  playing = true;
}

void loop() {
  // 1. 映像データ(1024B)をOLEDへ転送 (この間も裏で音声は途切れず再生される)
  for(uint8_t p = 0; p < 8; p++) {
    oled_cmd(0xB0 + p); oled_cmd(0x00); oled_cmd(0x10);
    i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
    
    for(uint16_t x = 0; x < 128; x++) {
      i2c_write(spi_transfer(0x00)); 
    }
    i2c_stop();
  }
  
  // アドレス更新とループ処理
  currentAddress += FRAME_SIZE;
  if (currentAddress >= maxAddress) {
    currentAddress = 0;
    GPIOC->OUTDR |= (1 << SPI_CS_BIT); 
    
    GPIOC->OUTDR &= ~(1 << SPI_CS_BIT); 
    spi_transfer(0x03);
    spi_transfer(0); spi_transfer(0); spi_transfer(0);
  }

  // 2. 次のフレームの音声(512B)を空いているバッファへ先読み
  load_buf ^= 1;
  for(uint16_t i = 0; i < AUDIO_SIZE; i++) {
    audio_buf[load_buf][i] = spi_transfer(0x00) ^ 0x80;
  }

  // 3. 現在の音声が鳴り終わるまで待機し、完全な30FPS同期をとる
  while(play_buf != load_buf) { 
    poll_audio(); 
  }
}
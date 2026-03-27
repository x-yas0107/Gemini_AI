/*
 * ======================================================================================
 * Project: UIAPduino_BadApple_Player (Dedicated Release)
 * Version: 1.00
 * Architect: Gemini & yas
 * Date: 2026/03/27
 *
 * [Description]
 * CH32V00x & W25Q128 Dedicated Bad Apple!! High-Speed Player.
 * Achieves ~30FPS streaming playback by optimizing SPI burst reads and SSD1306 Page Mode.
 * Features hardware serial (PD5) synchronization with DFPlayer Mini for audio playback.
 * * CH32V00xとW25Q128を用いた専用ハードウェアプレイヤー。
 * SPIバーストリードとSSD1306のページモード転送を極限まで最適化し、約30FPSの動画再生を実現。
 * PD5からのハードウェアシリアル通信(9600bps)により、DFPlayer Miniと音声を完全同期します。
 *
 * [I/O Map]
 * PC1 : SSD1306 SDA  (I2C High-Speed Bit-bang)
 * PC2 : SSD1306 SCL  (I2C High-Speed Bit-bang)
 * PC3 : W25Q128 CS   (SPI Chip Select)
 * PC4 : Tact Switch  (Input Pull-up, Connect to GND)
 * PC5 : W25Q128 SCK  (SPI Clock)
 * PC6 : W25Q128 MOSI (SPI Data Out)
 * PC7 : W25Q128 MISO (SPI Data In)
 * PD5 : DFPlayer RX  (USART1 TX, 9600bps Fixed)
 *
 * [Change History]
 * v0.xx - v1.11 : Development and debugging phases (SD card & freeze bug fixes).
 * v1.00 : Initial public release version. Cleaned up code and added detailed I/O map.
 * ======================================================================================
 */

#include <Arduino.h>

// --- Configuration (設定項目) ---
#define NUM_VIDEOS 2

// Video Data Addresses in SPI Flash (映像データのFlashメモリ番地)
const uint32_t VIDEO_ADDR[NUM_VIDEOS]   = {0x200000, 0x800000};
// Total frames for each video (各映像の総フレーム数)
const uint16_t VIDEO_FRAMES[NUM_VIDEOS] = {6500,     6500};
// Video Names for Menu Display (メニュー表示名)
const char* VIDEO_NAMES[NUM_VIDEOS]  = {"1: Bad Apple!!", "2: Video 2"};

// Frame delay in milliseconds to adjust playback speed (~30fps: 10, ~15fps: 35)
// 再生速度調整用ウェイト(ミリ秒)。映像が音より早い場合は数値を増やしてください。
const uint8_t  FRAME_DELAY = 10;       

// --- Pin Definitions (ピン定義) ---
#define BTN_PIN       4  // PC4 (Tact Switch)
#define SPI_CS_BIT    3  // PC3 (Flash CS)
#define SPI_SCK_BIT   5  // PC5 (Flash SCK)
#define SPI_MOSI_BIT  6  // PC6 (Flash MOSI)
#define SPI_MISO_BIT  7  // PC7 (Flash MISO)

#define TGT_SDA_BIT   1  // PC1 (OLED SDA)
#define TGT_SCL_BIT   2  // PC2 (OLED SCL)

// SPI CS Macros
#define SPI_CS_LOW()  (GPIOC->OUTDR &= ~(1 << SPI_CS_BIT))
#define SPI_CS_HIGH() (GPIOC->OUTDR |=  (1 << SPI_CS_BIT))

char owner[16] = "";

// --- ASCII Font 5x8 (0x20 - 0x7E) ---
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
  {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x78},{0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08},{0x00,0x00,0x00,0x00,0x00}
};

// --- DFPlayer Mini Hardware Serial (PD5, 9600bps) ---
void dfplayer_cmd(uint8_t cmd, uint16_t arg) {
  uint8_t buf[10] = {0x7E, 0xFF, 0x06, cmd, 0x00, (uint8_t)(arg >> 8), (uint8_t)(arg & 0xFF), 0, 0, 0xEF};
  uint16_t sum = 0;
  for (uint8_t i = 1; i < 7; i++) sum += buf[i];
  sum = -sum;
  buf[7] = sum >> 8;
  buf[8] = sum & 0xFF;
  
  for (uint8_t i = 0; i < 10; i++) {
    // Wait for TXE (Transmit Data Register Empty)
    while(!(USART1->STATR & (1<<7))); 
    USART1->DATAR = buf[i];
  }
  delay(20);
}

// --- SPI Engine (W25Q128) ---
void spi_init() {
  GPIOC->CFGLR &= ~((0xF << (4*3)) | (0xF << (4*5)) | (0xF << (4*6)) | (0xF << (4*7)));
  GPIOC->CFGLR |=  ((0x3 << (4*3)) | (0x3 << (4*5)) | (0x3 << (4*6)) | (0x8 << (4*7)));
  GPIOC->OUTDR |= (1 << SPI_CS_BIT) | (1 << SPI_MISO_BIT);
}

uint8_t spi_transfer(uint8_t data) {
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
  return rx;
}

// --- I2C Bit-bang High Speed (SSD1306) ---
void sda_high() { GPIOC->OUTDR |= (1<<TGT_SDA_BIT); }
void sda_low()  { GPIOC->OUTDR &= ~(1<<TGT_SDA_BIT); }
void scl_high() { GPIOC->OUTDR |= (1<<TGT_SCL_BIT); }
void scl_low()  { GPIOC->OUTDR &= ~(1<<TGT_SCL_BIT); }

void i2c_start() { sda_high(); scl_high(); sda_low(); scl_low(); }
void i2c_stop()  { sda_low(); scl_high(); sda_high(); }

void i2c_write(uint8_t data) {
  for(uint8_t i=0; i<8; i++){ 
    if(data&0x80) sda_high(); else sda_low(); 
    scl_high(); scl_low(); data<<=1; 
  }
  sda_high(); scl_high(); scl_low(); 
}

// --- OLED Control & Text UI ---
void oled_cmd(uint8_t c) { 
  i2c_start(); i2c_write(0x3C<<1); i2c_write(0x00); i2c_write(c); i2c_stop(); 
}

void oled_init() { 
  oled_cmd(0xAE); oled_cmd(0x20); oled_cmd(0x02); // Page Addressing Mode
  oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); // Charge pump enable, Display ON
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
  i2c_write(0x00); // Letter spacing
  i2c_stop();
}

void oled_print_str(const char* str, uint8_t col, uint8_t page) {
  while (*str) {
    oled_print_char(*str++, col, page);
    col += 6;
  }
}

void draw_menu(uint8_t sel) {
  oled_clear();
  oled_print_str("--- SELECT VIDEO ---", 4, 0);
  for(uint8_t i = 0; i < NUM_VIDEOS; i++) {
    if(sel == i) oled_print_str(">", 4, 2 + i * 2);
    oled_print_str(VIDEO_NAMES[i], 16, 2 + i * 2);
  }
  oled_print_str("[HOLD TO PLAY]", 22, 7);
}

// --- Video Engine ---
void play_video(uint32_t startAddr, uint16_t frames) {
  for(uint16_t f = 0; f < frames; f++) {
    uint32_t fAddr = startAddr + (uint32_t)f * 1024; // 128x64 = 8192 bits = 1024 bytes/frame
    
    // Start SPI Burst Read
    SPI_CS_LOW();
    spi_transfer(0x03); // Read Data Command
    spi_transfer((fAddr >> 16) & 0xFF); spi_transfer((fAddr >> 8) & 0xFF); spi_transfer(fAddr & 0xFF);

    // Stream directly to OLED Pages
    for(uint8_t p = 0; p < 8; p++) {
      oled_cmd(0xB0 + p); oled_cmd(0x00); oled_cmd(0x10);
      i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
      for(uint16_t x = 0; x < 128; x++) {
        i2c_write(spi_transfer(0x00)); 
      }
      i2c_stop();
    }
    SPI_CS_HIGH();

    // Frame delay for sync
    if(FRAME_DELAY > 0) delay(FRAME_DELAY);

    // PC4 Button Interrupt Monitor (即時中断監視)
    if ((GPIOC->INDR & (1 << BTN_PIN)) == 0) {
      while ((GPIOC->INDR & (1 << BTN_PIN)) == 0) { delay(10); } // Wait for release
      return;
    }
  }
}

void setup() {
  // RCC APB2 Peripheral Clock Enable 
  // Bit 14 = USART1 (0x4000), Bit 5 = IOPD, Bit 4 = IOPC, Bit 3 = IOPB, Bit 2 = IOPA
  RCC->APB2PCENR |= 0x403D; 
  
  // GPIO Reset & Enable for PC
  GPIOC->CFGLR &= ~0xFF0; GPIOC->CFGLR |= 0x550; 
  
  // USART1 Hardware Serial Setup (PD5=TX for DFPlayer Mini)
  GPIOD->CFGLR &= ~(0xF << (4 * 5));
  GPIOD->CFGLR |=  (0xB << (4 * 5));      // Alternate Function Push-Pull (50MHz)
  USART1->BRR = 0x1388;                   // 9600bps @ 48MHz (48000000 / 9600 = 5000 = 0x1388)
  USART1->CTLR1 = 0x2008;                 // UART Enable (Bit13), TX Enable (Bit3)
  
  // PC4 Button Setup (Input Pull-up)
  GPIOC->CFGLR &= ~(0xF << (4 * BTN_PIN));
  GPIOC->CFGLR |=  (0x8 << (4 * BTN_PIN));
  GPIOC->OUTDR |=  (1 << BTN_PIN);

  spi_init();
  oled_init();

  // Validate Owner Signature from Flash (Security check)
  for (int i = 0; i < 3; i++) {
    SPI_CS_LOW(); spi_transfer(0x03); spi_transfer(0); spi_transfer(0); spi_transfer(i);
    owner[i] = spi_transfer(0); SPI_CS_HIGH();
  }
  owner[3] = '\0';

  if (strcmp(owner, "yas") == 0) {
    uint8_t sel = 0;
    
    while(true) {
      draw_menu(sel);
      
      // UI Control Loop
      while(true) {
        if ((GPIOC->INDR & (1 << BTN_PIN)) == 0) {
          delay(30); // Debounce
          if ((GPIOC->INDR & (1 << BTN_PIN)) == 0) {
            uint32_t press_time = millis();
            while ((GPIOC->INDR & (1 << BTN_PIN)) == 0) { delay(10); } // Wait release
            
            uint32_t duration = millis() - press_time;
            if (duration >= 500) {
              break; // Long press -> Exit menu and play
            } else if (duration > 30) {
              sel = (sel + 1) % NUM_VIDEOS; // Short press -> Toggle selection
              draw_menu(sel);
            }
          }
        }
      }
      
      // Start chosen video & audio
      oled_clear();
      dfplayer_cmd(0x03, sel + 1); // DFPlayer: Play track corresponding to sel (1 or 2)
      play_video(VIDEO_ADDR[sel], VIDEO_FRAMES[sel]); 
      dfplayer_cmd(0x16, 0);       // DFPlayer: Stop playback when video ends or is interrupted
    }
  }
}

void loop() {}
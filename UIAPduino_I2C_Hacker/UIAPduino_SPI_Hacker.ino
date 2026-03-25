/*
 * ======================================================================================
 * Project: UIAPduino_I2C_Hacker
 * Version: 1.00 (Public Release)
 * Architect: Gemini & yas
 * Date: 2026/03/22
 *
 * [Description]
 * CH32V006等のマイコンと外部SPI Flash(W25Q128)を組み合わせ、
 * I2Cデバイス（OLED等）をハッキング・制御するためのコンソール型OSツール。
 * シリアル通信経由でコマンドを送信し、Flashの読み書きや仮想マシン(VM)による
 * I2C直接操作を実現する。
 *
 * [Hardware I/O Map]
 * --- SPI (W25Q128 Flash) ---
 * PC3 : SPI_CS   (Chip Select)
 * PC5 : SPI_SCK  (Clock)
 * PC6 : SPI_MOSI (Master Out Slave In)
 * PC7 : SPI_MISO (Master In Slave Out)
 * * --- I2C (Target Device / OLED) ---
 * PC1 : TGT_SDA  (Data)
 * PC2 : TGT_SCL  (Clock)
 *
 * [TODO / Known Issues]
 * - Tコマンド(Misaki): 漢字フォントの読み出しとOLEDへの描画ロジックが未完成。
 * 現状、指定したインデックスの文字が正しく表示されず「横線」になるバグが存在する。
 * (美咲フォントの縦横データ構造と、OLEDのページアドレッシングの不一致が疑われる)
 * -> コントリビューターによる描画エンジンの改修を求む！
 * ======================================================================================
 */

#include <Arduino.h>

// --- W25Q128 SPIフラッシュの基本アドレス定義 ---
#define FLASH_SYS_ADDR   0x000000 
#define FLASH_IDX_ADDR   0x001000 
#define FLASH_APP_ADDR   0x010000 
#define FLASH_FONT_ADDR  0x100000 

// --- SPI通信用ピン定義 (W25Q128接続用) ---
#define SPI_CS_BIT    3  // PC3
#define SPI_SCK_BIT   5  // PC5
#define SPI_MOSI_BIT  6  // PC6
#define SPI_MISO_BIT  7  // PC7

#define SPI_CS_LOW()  (GPIOC->OUTDR &= ~(1 << SPI_CS_BIT))
#define SPI_CS_HIGH() (GPIOC->OUTDR |=  (1 << SPI_CS_BIT))

// --- I2C通信用ピン定義 (ターゲットデバイス接続用) ---
#define TGT_SDA_BIT 1  // PC1
#define TGT_SCL_BIT 2  // PC2

// --- グローバル変数 ---
char cmdBuf[128];
uint8_t cmdIdx = 0;
char owner[16] = "G";
uint8_t activeID = 0x00;

// --- 最小構成のASCIIフォント (5x7ドット) ---
const uint8_t font5x7[][5] = {
  {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46},
  {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
  {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x7C, 0x12, 0x11, 0x12, 0x7C}, {0x7F, 0x49, 0x49, 0x49, 0x36},
  {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
  {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
  {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
  {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
  {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
  {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
  {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F},
  {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
  {0x00, 0x00, 0x00, 0x00, 0x00}, {0x7F, 0x00, 0x00, 0x00, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
  {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
  {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
  {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
  {0x7F, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
  {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
  {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
  {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
  {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
  {0x44, 0x64, 0x54, 0x4C, 0x44}
};

uint8_t getCharIdx(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'Z') return (c - 'A') + 10;
  if (c >= 'a' && c <= 'z') return (c - 'a') + 38;
  return 36;
}

// --- [1] SPI Flash Engine ---
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

void spi_write_enable() { SPI_CS_LOW(); spi_transfer(0x06); SPI_CS_HIGH(); }

bool spi_is_busy() {
  SPI_CS_LOW(); spi_transfer(0x05);
  uint8_t status = spi_transfer(0x00); 
  SPI_CS_HIGH();
  return (status & 0x01);
}

uint8_t spi_read8(uint32_t addr) {
  SPI_CS_LOW();
  spi_transfer(0x03);
  spi_transfer((addr >> 16) & 0xFF); spi_transfer((addr >> 8) & 0xFF); spi_transfer(addr & 0xFF);
  uint8_t data = spi_transfer(0x00);
  SPI_CS_HIGH();
  return data;
}

void spi_write8(uint32_t addr, uint8_t data) {
  spi_write_enable();
  SPI_CS_LOW();
  spi_transfer(0x02);
  spi_transfer((addr >> 16) & 0xFF); spi_transfer((addr >> 8) & 0xFF); spi_transfer(addr & 0xFF);
  spi_transfer(data);
  SPI_CS_HIGH();
  while(spi_is_busy());
}

// --- [2] I2C Dictionary Engine ---
bool findInDict(uint8_t addr, char* name) {
  for(uint32_t p = FLASH_IDX_ADDR; p < FLASH_IDX_ADDR + 0x400; p += 32) {
    if(spi_read8(p) == addr) {
      for(int i=0; i<31; i++) name[i] = (char)spi_read8(p + 1 + i);
      name[31] = '\0';
      return true;
    }
  }
  return false;
}

// --- [3] I2C Bit-bang Control ---
void sda_high() { GPIOC->OUTDR |= (1<<TGT_SDA_BIT); }
void sda_low()  { GPIOC->OUTDR &= ~(1<<TGT_SDA_BIT); }
bool sda_read() { return (GPIOC->INDR & (1<<TGT_SDA_BIT)) != 0; }
void scl_high() { GPIOC->OUTDR |= (1<<TGT_SCL_BIT); }
void scl_low()  { GPIOC->OUTDR &= ~(1<<TGT_SCL_BIT); }
void i2c_delay() { delayMicroseconds(4); }

void i2c_start() { sda_high(); i2c_delay(); scl_high(); i2c_delay(); sda_low(); i2c_delay(); scl_low(); i2c_delay(); }
void i2c_stop()  { sda_low(); i2c_delay(); scl_high(); i2c_delay(); sda_high(); i2c_delay(); }

bool i2c_write(uint8_t data) {
  for(uint8_t i=0; i<8; i++){ if(data&0x80) sda_high(); else sda_low(); i2c_delay(); scl_high(); i2c_delay(); scl_low(); i2c_delay(); data<<=1; }
  sda_high(); i2c_delay(); scl_high(); i2c_delay(); 
  bool ack=!sda_read();
  scl_low(); i2c_delay(); 
  return ack;
}

uint8_t i2c_read(bool ack) {
  uint8_t data = 0; sda_high();
  for(uint8_t i=0; i<8; i++){ i2c_delay(); scl_high(); i2c_delay(); data = (data<<1)|sda_read(); scl_low(); }
  if(ack) sda_low(); else sda_high();
  i2c_delay(); scl_high(); i2c_delay(); scl_low(); i2c_delay(); sda_high();
  return data;
}

// --- [4] SSD1306 OLED Engine ---
void oled_cmd(uint8_t c) { i2c_start(); i2c_write(0x3C<<1); i2c_write(0x00); i2c_write(c); i2c_stop(); }
void oled_clear() {
  for (uint8_t p = 0; p < 8; p++) {
    oled_cmd(0xB0 + p); oled_cmd(0x00); oled_cmd(0x10);
    i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
    for (uint8_t i = 0; i < 128; i++) i2c_write(0x00);
    i2c_stop();
  }
}
void oled_init() { 
  oled_cmd(0xAE); oled_cmd(0xD5); oled_cmd(0x80); oled_cmd(0xA8); oled_cmd(0x3F); oled_cmd(0xD3); oled_cmd(0x00); oled_cmd(0x40); 
  oled_cmd(0xA1); oled_cmd(0xC8); oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); oled_cmd(0xA4); oled_cmd(0xA6); oled_clear(); 
}
void oled_print(uint8_t x, uint8_t page, const char* str) {
  oled_cmd(0xB0 + page); oled_cmd(x & 0x0F); oled_cmd(0x10 | (x >> 4));
  i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
  while (*str) {
    uint8_t idx = getCharIdx(*str++);
    for (uint8_t i = 0; i < 5; i++) i2c_write(font5x7[idx][i]);
    i2c_write(0x00);
  }
  i2c_stop();
}

// --- [5] Console UI & Command Parser ---
void printHex2(uint8_t val) { if(val<0x10) Serial.print("0"); Serial.print(val, HEX); }

void printMenu() {
  Serial.println("\n--- MENU ---");
  Serial.println("S:Scan L:List C:Clr");
  Serial.println("D [A]:Dump P [A]:Prog");
  Serial.println("B [A]:Bulk E [A]:Erase");
  Serial.println("F [P]:Fmt M [X P A]:Draw");
  Serial.println("T [I]:Misaki K:Base");
  Serial.println("W [A W D]:Scroll");
  Serial.println("X [A]:Exec VM");
  Serial.println("R:Run Active ID");
  Serial.println("H:Help");
}

void listDictionary() {
  Serial.println("\n[ROM DICT]");
  char devName[32];
  for(uint32_t p = FLASH_IDX_ADDR; p < FLASH_IDX_ADDR + 0x400; p += 32) {
    uint8_t regAddr = spi_read8(p);
    if(regAddr != 0xFF && regAddr != 0x00) {
      if(findInDict(regAddr, devName)) {
        Serial.print("0x"); printHex2(regAddr); Serial.print("|"); Serial.println(devName);
      }
    }
  }
}

void init_personality() {
  Serial.println("[SYS] Auth...");
  for (int i = 0; i < 3; i++) owner[i] = spi_read8(FLASH_SYS_ADDR + i);
  owner[3] = '\0';
  if (strcmp(owner, "yas") == 0) {
    Serial.println("[OK] yas");
    i2c_start(); if (i2c_write(0x3C << 1)) { i2c_stop(); oled_init(); } else i2c_stop();
  } else { Serial.print("[!] ? User:"); Serial.println(owner); }
}

void vm_exec(uint32_t addr) {
  Serial.print("[VM] Run 0x"); Serial.println(addr, HEX);
  uint8_t lastRx = 0;
  while(true) {
    uint8_t op = spi_read8(addr++);
    if (op == 0xFF) { Serial.println("\n[VM] End"); break; }
    switch(op) {
      case 0x01: i2c_start(); break;
      case 0x02: i2c_stop(); break;
      case 0x10: i2c_write(spi_read8(addr++)); break;
      case 0x11: lastRx = i2c_read(true); break;
      case 0x12: lastRx = i2c_read(false); break;
      case 0x20: Serial.print((char)spi_read8(addr++)); break;
      case 0x21: printHex2(lastRx); break;
      case 0x30: delay(spi_read8(addr++)); break;
    }
  }
}

void handleCommand(char* cmd) {
  while (*cmd == ' ') cmd++;
  char action = toupper(*cmd);

  // --- [ R / RUN ] (Execute Active ID App) ---
  if (action == 'R') {
    if (activeID == 0x00) {
      Serial.println("[!] No ID Scanned.");
    } else {
      uint32_t appAddr = FLASH_APP_ADDR + (activeID * 0x100);
      Serial.print("[RUN] ID:0x"); printHex2(activeID);
      vm_exec(appAddr);
    }
    return;
  }

  char* ptr = strtok(cmd, " "); 
  
  if (action == 'S') {
    Serial.println("\n[SCAN]");
    char devName[32];
    for(uint8_t a=0x03; a<=0x77; a++) {
      i2c_start(); bool found = i2c_write(a << 1); i2c_stop();
      if(found) {
        Serial.print("[!] 0x"); printHex2(a);
        activeID = a;
        if(findInDict(a, devName)) {
           Serial.print("->"); Serial.print(devName); 
           oled_clear(); 
           uint8_t len = strlen(devName);
           uint8_t x_pos = (len < 21) ? (128 - (len * 6)) / 2 : 0;
           oled_print(x_pos, 3, devName); 
        }
        Serial.println();
      }
    }
  } 
  else if (action == 'L') { listDictionary(); }
  else if (action == 'H' || action == '?') { printMenu(); }
  else if (action == 'P') { 
    char* addrStr = strtok(NULL, " ");
    uint32_t addr = addrStr ? strtol(addrStr, NULL, 16) : 0;
    if (addr < 0x100) addr += FLASH_IDX_ADDR;
    uint8_t buf[32]; uint8_t cnt=0;
    while((ptr=strtok(NULL, " "))!=NULL && cnt<32) buf[cnt++]=strtol(ptr, NULL, 16);
    for(int i=0; i<cnt; i++) spi_write8(addr + i, buf[i]);
    Serial.println("[OK] Prog");
  }
  else if (action == 'B') { 
    char* addrStr = strtok(NULL, " ");
    uint32_t addr = addrStr ? strtol(addrStr, NULL, 16) : 0;
    Serial.print("[Rdy] Hex->0x"); Serial.println(addr, HEX);
    uint8_t hexBuf[2]; uint8_t hexIdx = 0;
    while(true) {
      if (USART1->STATR & (1 << 5)) {
        char c = USART1->DATAR;
        if (c == 'X' || c == 'x') { Serial.println("\n[OK] Bulk"); break; }
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          hexBuf[hexIdx++] = c;
          if (hexIdx == 2) {
            uint8_t val = 0;
            for(int i=0; i<2; i++) {
              val <<= 4; char hc = hexBuf[i];
              if(hc >= '0' && hc <= '9') val += (hc - '0');
              else if(hc >= 'a' && hc <= 'f') val += (hc - 'a' + 10);
              else if(hc >= 'A' && hc <= 'F') val += (hc - 'A' + 10);
            }
            spi_write8(addr++, val); Serial.print("."); hexIdx = 0;
          }
        }
      }
    }
  }
  else if (action == 'E') { 
    char* addrStr = strtok(NULL, " ");
    uint32_t addr = addrStr ? strtol(addrStr, NULL, 16) : 0;
    Serial.print("[I] Ers 0x"); Serial.println(addr, HEX);
    spi_write_enable(); SPI_CS_LOW(); spi_transfer(0x20);
    spi_transfer((addr >> 16) & 0xFF); spi_transfer((addr >> 8) & 0xFF); spi_transfer(addr & 0xFF);
    SPI_CS_HIGH(); while(spi_is_busy());
    Serial.println("[OK] Ers");
  }
  else if (action == 'F') { 
    char* pinStr = strtok(NULL, " ");
    if (pinStr != NULL && strcmp(pinStr, "1995") == 0) {
      Serial.println("[I] Fmt...");
      for(int i=0; i<2; i++) {
        spi_write_enable(); SPI_CS_LOW(); spi_transfer(0x20); 
        spi_transfer(0x00); spi_transfer(i*0x10); spi_transfer(0x00);
        SPI_CS_HIGH(); while(spi_is_busy());
      }
      spi_write8(FLASH_SYS_ADDR + 0, 'y'); spi_write8(FLASH_SYS_ADDR + 1, 'a'); spi_write8(FLASH_SYS_ADDR + 2, 's'); spi_write8(FLASH_SYS_ADDR + 3, '\0');
      Serial.println("[OK] Fmt");
    } else { Serial.println("[!] PIN"); }
  }
  else if (action == 'M') { 
    char* xStr = strtok(NULL, " ");
    char* pStr = strtok(NULL, " ");
    char* addrStr = strtok(NULL, " ");
    if(xStr && pStr && addrStr) {
      uint8_t x = atoi(xStr), p = atoi(pStr); uint32_t addr = strtol(addrStr, NULL, 16);
      oled_cmd(0xB0 + p); oled_cmd(x & 0x0F); oled_cmd(0x10 | (x >> 4));
      i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
      for(uint8_t i=0; i<8; i++) i2c_write(spi_read8(addr + i));
      i2c_stop();
      Serial.print("[OK] M");
    }
  }
  // --- [ TODO ] T Command: Rendering logic needs fix for proper OLED output ---
  else if (action == 'T') {
    char* idxStr = strtok(NULL, " ");
    uint16_t startIdx = idxStr ? strtol(idxStr, NULL, 10) : 0;
    oled_clear();
    for(int c = 0; c < 16; c++) {
      uint8_t rowData[8], colData[8] = {0};
      for(int i = 0; i < 8; i++) rowData[i] = spi_read8(FLASH_FONT_ADDR + (startIdx + c) * 8 + i);
      for(int i = 0; i < 8; i++) { for(int j = 0; j < 8; j++) { if(rowData[i] & (0x80 >> j)) colData[j] |= (1 << i); } }
      uint8_t px = c * 8;
      oled_cmd(0xB0 + 3); oled_cmd(px & 0x0F); oled_cmd(0x10 | (px >> 4));
      i2c_start(); i2c_write(0x3C << 1); i2c_write(0x40);
      for (uint8_t i = 0; i < 8; i++) i2c_write(colData[i]);
      i2c_stop();
    }
    Serial.println("[OK] T");
  }
  else if (action == 'K') {
    oled_clear();
    uint8_t t_bmp[8] = {0x01, 0x01, 0x01, 0xFF, 0x01, 0x01, 0x01, 0x00}; 
    oled_cmd(0xB0 + 3); oled_cmd(0x00); oled_cmd(0x10);
    i2c_start(); i2c_write(0x3C << 1); i2c_write(0x40);
    for (uint8_t i = 0; i < 8; i++) i2c_write(t_bmp[i]);
    i2c_stop();
    Serial.println("[OK] K");
  }
  else if (action == 'W') { 
    char* addrStr = strtok(NULL, " ");
    char* widthStr = strtok(NULL, " ");
    char* dlyStr = strtok(NULL, " ");
    if(addrStr && widthStr) {
      uint32_t baseAddr = strtol(addrStr, NULL, 16);
      uint16_t totalWidth = strtol(widthStr, NULL, 10), dly = dlyStr ? strtol(dlyStr, NULL, 10) : 5;
      uint8_t frame[256];
      Serial.println("[I] Scroll... 'X' to stop");
      for(int offset = 0; offset <= totalWidth; offset++) {
        SPI_CS_LOW(); spi_transfer(0x03); 
        uint32_t readAddr = baseAddr + offset * 2;
        spi_transfer((readAddr >> 16) & 0xFF); spi_transfer((readAddr >> 8) & 0xFF); spi_transfer(readAddr & 0xFF);
        for(int i=0; i<256; i++) frame[i] = (offset + (i/2) < totalWidth) ? spi_transfer(0x00) : 0x00;
        SPI_CS_HIGH();
        for(uint8_t p = 0; p < 2; p++) {
          oled_cmd(0xB0 + 3 + p); oled_cmd(0x00); oled_cmd(0x10);
          i2c_start(); i2c_write(0x3C<<1); i2c_write(0x40);
          for(uint8_t x = 0; x < 128; x++) i2c_write(frame[x*2 + p]);
          i2c_stop();
        }
        delay(dly);
        if (USART1->STATR & (1 << 5)) { char rc = USART1->DATAR; if (rc == 'X' || rc == 'x') break; }
      }
      Serial.println("[OK] W");
    }
  }
  else if (action == 'X') {
    char* addrStr = strtok(NULL, " ");
    uint32_t addr = addrStr ? strtol(addrStr, NULL, 16) : 0;
    vm_exec(addr);
  }
  else if (action == 'D') { 
    char* addrStr = strtok(NULL, " ");
    uint32_t addr = addrStr ? strtol(addrStr, NULL, 16) : 0;
    uint32_t len = atoi(strtok(NULL, " ")); if(len==0) len=16;
    for(uint32_t i=0; i<len; i++) {
      if(i%16==0) { Serial.print("\n0x"); Serial.print(addr+i, HEX); Serial.print(": "); }
      printHex2(spi_read8(addr+i)); Serial.print(" ");
    } Serial.println();
  }
  else if (action == 'C') { oled_clear(); Serial.println("[OK] Clr"); }
  else Serial.println("[!] Cmd");
}

void setup() {
  RCC->APB2PCENR |= 0x3D; 
  GPIOC->CFGLR &= ~0xFF0; GPIOC->CFGLR |= 0x550; 
  USART1->BRR = 0x1A1; USART1->CTLR1 = 0x200C;
  Serial.begin(115200);
  Serial.println("\n*** UIAPduino v1.00 ***");
  spi_init();
  SPI_CS_LOW(); spi_transfer(0x9F);
  uint8_t mfg = spi_transfer(0x00), type = spi_transfer(0x00), cap = spi_transfer(0x00); 
  SPI_CS_HIGH();
  Serial.print("[SPI] ID:"); printHex2(mfg); printHex2(type); printHex2(cap);
  if(mfg == 0xEF) Serial.println(" OK"); else Serial.println(" ERR");
  init_personality();
  Serial.print("\n> ");
}

void loop() {
  if (USART1->STATR & (1 << 5)) {
    uint8_t c = USART1->DATAR; Serial.print((char)c);
    if (c == '\r' || c == '\n') {
      Serial.println(); 
      if (cmdIdx > 0) { 
        cmdBuf[cmdIdx] = 0;
        handleCommand(cmdBuf);
        cmdIdx = 0;
      }
      Serial.print("\n> ");
    } 
    else if (cmdIdx < 127) { cmdBuf[cmdIdx++] = c; }
  }
}
/* UIAPduino_GemOS V1.25 / I2C_Commander V0.42 */
/* * Change History:
 * V1.25/0.42 - Centered PC LINK MODE text coordinates for 128x64 OLED display.
 * V1.24/0.41 - Reordered COM menu top bar to: ESC, SEQ, MON, DMP, RED, WRT for better usability. Slowed DMP screen update interval to 500ms for readability.
 * V1.23/0.40 - Optimized ROM size by truncating UI strings to resolve 68-byte FLASH overflow. Changed MON screen update interval to 1000ms for stability. Fixed narrowing cast warning.
 * V1.22/0.39 - Fixed severe I2C bus freezing bug in MON screen by moving EEPROM and sensor read operations outside the 8-page OLED rendering loop and limiting updates to 100ms intervals.
 * V1.21/0.38 - Dieted code to fit within 16KB FLASH: Replaced division/modulo operations with bitwise shifts and subtraction loops in draw_num, draw_pixel, and UI elements. Shortened Serial messages to save ROM.
 * V1.20/0.37 - Replaced PNG (Ping) with MON (Monitor) mode in COM menu. Implemented generic UI display engine that reads rendering rules from EEPROM SEQ 15, allowing straight reading of sensors without CPU ROM bloat.
 * V1.19/0.36 - Adjusted joystick DEADZONE to 80 and increased scroll delay in DMP and SEQ menus to 400ms to prevent rapid scrolling.
 * V1.18/0.35 - Made DMP (Dump) screen real-time by updating dump_buffer every 100ms instead of only on page change.
 * V1.17/0.34 - Perfectly centered all UI text including the SYS info screen (-APP- I2C COMMANDER etc.), COM SELECT CMD string, and top bar menus.
 * V1.16/0.33 - Fixed UI layout: Widened popup windows (states 3,4,5) to prevent text overflow and limited device name display to 14 chars to fit within target frame.
 * V1.15/0.32 - Fixed UI state bug by clearing popup_state when exiting COM menu via top bar (ESC/SYS/SCN).
 * V1.14/0.31 - Added Serial 'F' command with PIN '1234' to safely format EEPROM dictionary slots 03-30.
 * V1.13/0.30 - Replaced Arduino Serial.read() with direct hardware USART1 register polling for rock-solid RX. Removed buggy buffer flush.
 * V1.10/0.27 - Optimized PC LINK MODE by drawing the OLED only once upon entry, eliminating rendering overhead for perfect Serial RX stability.
 * V1.09/0.26 - Added Serial status messages when entering and exiting PC LINK MODE.
 * V1.08/0.25 - Implemented PC LINK MODE via PC3 switch. Disables ADC during transfer for maximum Serial stability.
 * V1.06/0.23 - Added boot sequence messages to Serial output for connection debugging.
 * V1.05/0.22 - Removed pinMode redefinitions for PD5 (TX) and PD6 (RX) in setup() that blocked Serial communication.
 * V1.04/0.21 - Rewrote Serial parser for 'W' and 'S' commands to dynamically handle variable-length names and robustly find comma delimiters.
 * V1.03/0.20 - Reverted draw_char index shifting logic to fix font rendering corruption. Changed APP bracket style to avoid missing ']' character.
 * V1.02/0.19 - Fixed Serial dump (R command) column misalignment by replacing non-printable characters with spaces. Enhanced SEQ name display safety.
 * V1.01/0.18 - Fixed overlapping UI in SYS menu by hiding target frame. Restored missing ']' character in font renderer. Cleaned up About screen layout.
 * V1.00/0.17 - Transitioned base system to UIAPduino_GemOS. Updated SYS menu info screen to display 2-layer OS and App versions.
 * V0.16 - Implemented SEQ List Selection Menu (popup_state 6) and execution (popup_state 7). Added Serial 'S' command to save sequences. Refactored Serial reading to prevent buffer overflow.
 * V0.15 - Refactored COM menu to Context-Aware Top Bar (ESC, PNG, RED, WRT, DMP, SEQ). Added Sequence engine skeleton (popup_state 6).
 */
#include <Arduino.h>

#define SOFT_SDA 6
#define SOFT_SCL 7
#define EXT_SDA 1
#define EXT_SCL 2
#define OLED_ADDR 0x78
#define EEPROM_ADDR 0x50
#define DEADZONE 80

#define DEV_MEM_START 0x0800
#define DEV_MEM_SIZE  0x0800
#define DEV_LBL_OFS   0x0000
#define DEV_ID_OFS    0x0010
#define DEV_INIT_OFS  0x0020
#define DEV_CMD_OFS   0x00A0
#define DEV_DATA_OFS  0x0120
#define DEV_PAYLD_OFS 0x0220

uint8_t mouse_x = 64, mouse_y = 32;
uint16_t adc_offset_x = 512, adc_offset_y = 512;
bool eeprom_ok = false;
uint8_t oled_buffer[128]; 
uint8_t current_page = 0; 
uint8_t font_cache[158][5]; 

uint8_t menu_state = 0;
uint8_t popup_state = 0;
bool ping_success = false;
uint8_t edit_val = 0;
uint8_t target_reg = 0;
uint8_t last_io_val = 0;
uint8_t com_mode_select = 0;

uint8_t found_addrs[8];
uint8_t found_count = 0;
bool scan_done = false;
uint8_t sel_index = 0;
uint8_t target_device_addr = 0;
uint8_t target_device_slot = 0xFF;
char current_dev_name[17] = "UNKNOWN";

void neuron_delay_nop(volatile uint32_t count) { while(count--) __asm__("nop"); }
void soft_i2c_start() { GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); neuron_delay_nop(1); }
void soft_i2c_stop() { GPIOC->BSHR=(1<<(SOFT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SDA); }
void soft_i2c_write(uint8_t data) { for(int i=0;i<8;i++){ if(data&0x80)GPIOC->BSHR=(1<<SOFT_SDA); else GPIOC->BSHR=(1<<(SOFT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); data<<=1; } GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); }
uint8_t soft_i2c_read(bool ack) { uint8_t data=0; GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); for(int i=0;i<8;i++){ data<<=1; GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); if(GPIOC->INDR&(1<<SOFT_SDA))data|=1; GPIOC->BSHR=(1<<(SOFT_SCL+16)); neuron_delay_nop(1); } if(ack)GPIOC->BSHR=(1<<(SOFT_SDA+16)); else GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SDA); return data; }

void ext_i2c_start() { GPIOC->BSHR=(1<<EXT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(EXT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<(EXT_SCL+16)); neuron_delay_nop(1); }
void ext_i2c_stop() { GPIOC->BSHR=(1<<(EXT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SDA); }
bool ext_i2c_write_ping(uint8_t data) { for(int i=0;i<8;i++){ if(data&0x80)GPIOC->BSHR=(1<<EXT_SDA); else GPIOC->BSHR=(1<<(EXT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(EXT_SCL+16)); data<<=1; } GPIOC->BSHR=(1<<EXT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SCL); neuron_delay_nop(1); bool ack = !(GPIOC->INDR & (1<<EXT_SDA)); GPIOC->BSHR=(1<<(EXT_SCL+16)); return ack; }
uint8_t ext_i2c_read(bool ack) { uint8_t data=0; GPIOC->BSHR=(1<<EXT_SDA); neuron_delay_nop(1); for(int i=0;i<8;i++){ data<<=1; GPIOC->BSHR=(1<<EXT_SCL); neuron_delay_nop(1); if(GPIOC->INDR&(1<<EXT_SDA))data|=1; GPIOC->BSHR=(1<<(EXT_SCL+16)); neuron_delay_nop(1); } if(ack)GPIOC->BSHR=(1<<(EXT_SDA+16)); else GPIOC->BSHR=(1<<EXT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(EXT_SCL+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<EXT_SDA); return data; }

uint8_t ext_read_reg(uint8_t addr, uint8_t reg) {
    ext_i2c_start(); ext_i2c_write_ping((addr<<1)|0); ext_i2c_write_ping(reg);
    ext_i2c_start(); ext_i2c_write_ping((addr<<1)|1);
    uint8_t data = ext_i2c_read(false); ext_i2c_stop(); return data;
}
void ext_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    ext_i2c_start(); ext_i2c_write_ping((addr<<1)|0); ext_i2c_write_ping(reg); ext_i2c_write_ping(val); ext_i2c_stop();
}

uint8_t eeprom_read_byte(uint16_t addr) { soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write((uint8_t)(addr>>8)); soft_i2c_write((uint8_t)(addr&0xFF)); soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|1); uint8_t data=soft_i2c_read(false); soft_i2c_stop(); return data; }
void eeprom_write_byte(uint16_t addr, uint8_t data) { soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write((uint8_t)(addr>>8)); soft_i2c_write((uint8_t)(addr&0xFF)); soft_i2c_write(data); soft_i2c_stop(); delay(5); }
void oled_cmd(uint8_t cmd) { soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x00); soft_i2c_write(cmd); soft_i2c_stop(); }
void oled_init() { oled_cmd(0xAE); oled_cmd(0x20); oled_cmd(0x02); oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); }
void oled_set_pos(uint8_t x, uint8_t page) { oled_cmd(0xB0+page); oled_cmd(0x00+(x&0x0F)); oled_cmd(0x10+((x>>4)&0x0F)); }

void draw_pixel(uint8_t x, uint8_t y) { if(x>=128||y>=64)return; if((y>>3)==current_page) oled_buffer[x]|=(1<<(y&7)); }
void clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++)for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)>>3)==current_page)oled_buffer[x+i]&=~(1<<((y+j)&7)); }
void invert_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++)for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)>>3)==current_page)oled_buffer[x+i]^=(1<<((y+j)&7)); }
void draw_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++){draw_pixel(x+i,y);draw_pixel(x+i,y+h-1);} for(uint8_t i=0;i<h;i++){draw_pixel(x,y+i);draw_pixel(x+w-1,y+i);} }
void draw_char(uint8_t x, uint8_t y, uint8_t c) { 
    uint8_t font_idx = 0; if(c >= 32 && c <= 92) font_idx = c - 32; else if(c == 93) return; else if(c >= 94 && c <= 126) font_idx = c - 32 - 1; else if(c >= 0xA1 && c <= 0xDF) font_idx = 95 + (c - 0xA1); else return;
    uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + 7 < p_min) return; 
    for(uint8_t i=0;i<5;i++){ uint8_t line=font_cache[font_idx][i]; for(uint8_t j=0;j<7;j++)if(line&(1<<j))draw_pixel(x+i,y+j); } 
}
void draw_string(uint8_t x, uint8_t y, const char* str) { while(*str){draw_char(x,y,(uint8_t)*str++);x+=6;} }
void draw_hex(uint8_t x, uint8_t y, uint8_t val) { 
    char buf[5] = {'0', 'x', '0', '0', '\0'}; uint8_t h = val >> 4, l = val & 0x0F; 
    buf[2] = h > 9 ? 'A' + h - 10 : '0' + h; buf[3] = l > 9 ? 'A' + l - 10 : '0' + l; draw_string(x, y, buf); 
}
void draw_num(uint8_t x, uint8_t y, int16_t val) {
    char buf[8]; int idx = 0;
    if (val < 0) { buf[idx++] = '-'; val = -val; }
    uint16_t v = val; bool p = false;
    uint16_t d[] = {10000, 1000, 100, 10};
    for(int i=0; i<4; i++) {
        char c = '0'; while(v >= d[i]) { v -= d[i]; c++; }
        if(c != '0' || p) { buf[idx++] = c; p = true; }
    }
    buf[idx++] = '0' + (uint8_t)v; buf[idx] = '\0';
    draw_string(x, y, buf);
}
void draw_cursor(uint8_t x, uint8_t y) { invert_rect(x-3,y,7,1); invert_rect(x,y-3,1,3); invert_rect(x,y+1,1,3); }

void init_device_dictionary() {
    if(eeprom_read_byte(0x07FF) != 0x56) {
        uint16_t base = DEV_MEM_START; const char* n0 = "SSD1306         "; for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, n0[i]); eeprom_write_byte(base + DEV_ID_OFS, 0x3C);
        base = DEV_MEM_START + (1 * DEV_MEM_SIZE); const char* n1 = "EEPROM 24C      "; for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, n1[i]); eeprom_write_byte(base + DEV_ID_OFS, 0x50);
        base = DEV_MEM_START + (2 * DEV_MEM_SIZE); const char* n2 = "MPU-6050        "; for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, n2[i]); eeprom_write_byte(base + DEV_ID_OFS, 0x68);
        eeprom_write_byte(0x07FF, 0x56);
    }
}

void lookup_device_name(uint8_t target_addr) {
    target_device_slot = 0xFF;
    for(int i=0; i<15; i++) current_dev_name[i] = ' '; current_dev_name[15] = '\0'; bool found = false;
    for(int dev=0; dev<31; dev++) {
        uint16_t base_addr = DEV_MEM_START + (dev * DEV_MEM_SIZE);
        if(eeprom_read_byte(base_addr + DEV_ID_OFS) == target_addr) {
            target_device_slot = dev;
            for(int i=0; i<14; i++) { char c = eeprom_read_byte(base_addr + DEV_LBL_OFS + i); if(c == 0xFF || c == 0x00) { current_dev_name[i] = '\0'; break; } current_dev_name[i] = (c >= 32 && c <= 126) ? c : ' '; }
            current_dev_name[14] = '\0';
            found = true; break;
        }
    }
    if(!found) { const char* unk = "UNKNOWN       "; for(int i=0; i<14; i++) current_dev_name[i] = unk[i]; current_dev_name[14] = '\0'; }
}

uint8_t hex2byte(char h1, char h2) {
    uint8_t val = 0;
    if(h1 >= '0' && h1 <= '9') val += (h1 - '0') << 4; else if(h1 >= 'A' && h1 <= 'F') val += (h1 - 'A' + 10) << 4; else if(h1 >= 'a' && h1 <= 'f') val += (h1 - 'a' + 10) << 4;
    if(h2 >= '0' && h2 <= '9') val += (h2 - '0'); else if(h2 >= 'A' && h2 <= 'F') val += (h2 - 'A' + 10); else if(h2 >= 'a' && h2 <= 'f') val += (h2 - 'a' + 10);
    return val;
}

char s_read() { 
    uint32_t t=millis(); 
    while(!(USART1->STATR & (1 << 5))){
        if(millis()-t>50) return 0;
    } 
    return USART1->DATAR; 
}

void check_serial() {
    if (USART1->STATR & (1 << 5)) {
        char cmd = USART1->DATAR;
        if(cmd == 'R' || cmd == 'r') {
            Serial.println("--- DICT ---");
            for(int dev=0; dev<31; dev++) {
                uint16_t base_addr = DEV_MEM_START + (dev * DEV_MEM_SIZE); uint8_t id = eeprom_read_byte(base_addr + DEV_ID_OFS); if(id == 0xFF || id == 0x00) continue;
                Serial.print("Slot "); if(dev < 10) Serial.print("0"); Serial.print(dev); Serial.print(" [0x"); if(id < 0x10) Serial.print("0"); Serial.print(id, HEX); Serial.print("] : ");
                for(int i=0; i<16; i++) { char c = (char)eeprom_read_byte(base_addr + DEV_LBL_OFS + i); if(c >= 32 && c <= 126) Serial.print(c); else Serial.print(' '); } Serial.println();
            }
            Serial.println("--- END ---");
        } 
        else if(cmd == 'W' || cmd == 'w') {
            if(s_read() == ',') {
                int slot = (s_read() - '0')*10 + (s_read() - '0');
                if(s_read() == ',' && slot >= 0 && slot <= 30) {
                    uint8_t id = hex2byte(s_read(), s_read());
                    if(s_read() == ',') {
                        char name[16]; for(int i=0; i<16; i++) name[i] = ' ';
                        int n_idx = 0;
                        while(true) {
                            char c = s_read();
                            if(c == 0 || c == '\r' || c == '\n') break;
                            if(n_idx < 16) name[n_idx++] = c;
                        }
                        uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE); eeprom_write_byte(base + DEV_ID_OFS, id);
                        for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, name[i]);
                        Serial.print("Save S"); Serial.println(slot);
                    }
                }
            }
        }
        else if(cmd == 'S' || cmd == 's') {
            if(s_read() == ',') {
                int slot = (s_read() - '0')*10 + (s_read() - '0');
                if(s_read() == ',' && slot >= 0 && slot <= 30) {
                    int pat = (s_read() - '0')*10 + (s_read() - '0');
                    if(s_read() == ',' && pat >= 0 && pat <= 15) {
                        char name[8]; for(int i=0; i<8; i++) name[i] = ' ';
                        int n_idx = 0;
                        bool ok = false;
                        while(true) {
                            char c = s_read();
                            if(c == 0 || c == '\r' || c == '\n') break;
                            if(c == ',') { ok = true; break; }
                            if(n_idx < 8) name[n_idx++] = c;
                        }
                        if(ok) {
                            uint8_t count = hex2byte(s_read(), s_read());
                            uint8_t payload[22];
                            if(s_read() == ',') {
                                for(int i=0; i<count*2 && i<22; i++) {
                                    payload[i] = hex2byte(s_read(), s_read());
                                    if(i < count*2 - 1) s_read();
                                }
                                uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE) + DEV_CMD_OFS + (pat * 32);
                                for(int i=0; i<8; i++) eeprom_write_byte(base + i, name[i]);
                                eeprom_write_byte(base + 8, count);
                                for(int i=0; i<count*2 && i<22; i++) eeprom_write_byte(base + 9 + i, payload[i]);
                                Serial.print("Save Q"); Serial.println(pat);
                            }
                        }
                    }
                }
            }
        }
        else if(cmd == 'F' || cmd == 'f') {
            if(s_read() == ',') {
                if(s_read() == '1' && s_read() == '2' && s_read() == '3' && s_read() == '4') {
                    Serial.println("--- FMT ---");
                    for(int slot=3; slot<=30; slot++) {
                        uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE);
                        eeprom_write_byte(base + DEV_ID_OFS, 0xFF);
                        for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, 0xFF);
                    }
                    Serial.println("--- OK ---");
                } else {
                    Serial.println("--- ERR ---");
                }
            }
        }
    }
}

uint16_t adc_read(uint8_t ch) {
    ADC1->RSQR3=ch; ADC1->CTLR2|=ADC_SWSTART; while(!(ADC1->STATR&ADC_EOC));
    uint16_t dummy = ADC1->RDATAR; ADC1->RSQR3=ch; ADC1->CTLR2|=ADC_SWSTART; while(!(ADC1->STATR&ADC_EOC));
    return (uint16_t)ADC1->RDATAR;
}

void setup() {
    Serial.begin(115200); 
    USART1->BRR = 0x1A1; USART1->CTLR1 = 0x200C; 
    Serial.println("=== GemOS Boot ===");
    Serial.println("System: Ver 1.25");
    Serial.println("Serial OK");
    Serial.println("Ready!");
    RCC->APB2PCENR|=RCC_AFIOEN|RCC_IOPAEN|RCC_IOPCEN|RCC_IOPDEN|RCC_ADC1EN;
    GPIOC->CFGLR&=~((0xFF<<4)|(0xF<<16)|(0xFF<<24)); GPIOC->CFGLR|=((0x55<<4)|(0x8<<16)|(0x55<<24)); GPIOC->BSHR=(1<<4);
    ADC1->CTLR2|=(1<<20)|(7<<17)|ADC_ADON; oled_init();
    adc_offset_x=adc_read(1); adc_offset_y=adc_read(0);
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write(0); soft_i2c_write(0); soft_i2c_write(0xAA); soft_i2c_stop(); delay(5);
    if(eeprom_read_byte(0)==0xAA) { 
        eeprom_ok=true; 
        for(int i=0; i<95; i++){ uint16_t addr = 0x0100 + i*5; for(int j=0; j<5; j++) font_cache[i][j] = eeprom_read_byte(addr+j); } 
        for(int i=0; i<63; i++){ uint16_t addr = 0x02DB + i*5; for(int j=0; j<5; j++) font_cache[95+i][j] = eeprom_read_byte(addr+j); } 
        init_device_dictionary();
    }
    pinMode(PC0, OUTPUT); pinMode(PC3, INPUT_PULLUP); pinMode(PC5, OUTPUT); pinMode(PD2, OUTPUT);
}

void loop() {
    static bool last_pc3 = true;
    bool cur_pc3 = (GPIOC->INDR & (1<<3));
    bool pc3_clicked = (last_pc3 && !cur_pc3);
    last_pc3 = cur_pc3;
    static bool pc_link_mode = false;

    if (menu_state == 0 && pc3_clicked) {
        pc_link_mode = !pc_link_mode;
        if (pc_link_mode) {
            Serial.println("--- PC LINK ON ---");
            ADC1->CTLR2 &= ~ADC_ADON;
            delay(5);
            for (current_page = 0; current_page < 8; current_page++) {
                for(int i=0; i<128; i++) oled_buffer[i] = 0;
                draw_window(0,0,128,64);
                draw_string(43, 24, "PC LINK");
                draw_string(37, 40, "PC3: EXIT");
                oled_set_pos(0,current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40); for(int x=0;x<128;x++) soft_i2c_write(oled_buffer[x]); soft_i2c_stop();
            }
        } else {
            ADC1->CTLR2 |= ADC_ADON;
            delay(5);
            Serial.println("--- PC LINK OFF ---");
        }
    }

    if (pc_link_mode) {
        check_serial();
        return;
    }

    check_serial();
    uint16_t x_raw=adc_read(1), y_raw=adc_read(0); static bool last_sw=true; bool cur_sw=(GPIOC->INDR&(1<<4)); bool frame_clicked=(last_sw&&!cur_sw); last_sw=cur_sw;
    int16_t dx=0,dy=0, diff_x=(int16_t)x_raw-(int16_t)adc_offset_x, diff_y=(int16_t)y_raw-(int16_t)adc_offset_y;
    if(diff_x>DEADZONE)dx=diff_x/128+1; else if(diff_x<-DEADZONE)dx=diff_x/128-1;
    if(diff_y>DEADZONE)dy=diff_y/128+1; else if(diff_y<-DEADZONE)dy=diff_y/128-1;
    mouse_x = (uint8_t)constrain(mouse_x + dx, 0, 127); mouse_y = (uint8_t)constrain(mouse_y + dy, 0, 63);
    
    static uint8_t dump_page = 0, dump_buffer[16], last_dump_page = 0xFF, last_target = 0xFF, seq_offset = 0;
    static uint32_t last_dump_time = 0, last_mon_time = 0;
    bool clicked = frame_clicked;
    
    if(popup_state != 0 && clicked) {
        if(popup_state == 1 || popup_state == 2 || popup_state == 5 || popup_state == 7 || popup_state == 8) { popup_state = 0; clicked = false; }
    }
    
    if(popup_state == 3 || popup_state == 4) {
        if(dy < 0) { edit_val++; delay(100); } else if(dy > 0) { edit_val--; delay(100); }
        if(clicked) {
            if(popup_state == 3) { target_reg = edit_val; if(com_mode_select == 0) { last_io_val = ext_read_reg(target_device_addr, target_reg); popup_state = 5; } else { edit_val = 0; popup_state = 4; } }
            else if(popup_state == 4) { last_io_val = edit_val; ext_write_reg(target_device_addr, target_reg, last_io_val); popup_state = 5; }
            clicked = false;
        }
    }
    
    if(popup_state == 6) {
        if (dy > 0 && mouse_y >= 50 && seq_offset < 13) { seq_offset++; delay(400); }
        if (dy < 0 && mouse_y <= 25 && seq_offset > 0) { seq_offset--; delay(400); }
    }
    
    if (popup_state == 2) {
        if (dy > 0 && mouse_y >= 58 && dump_page < 0xF0) { dump_page += 0x10; delay(400); }
        if (dy < 0 && mouse_y <= 22 && dump_page > 0) { dump_page -= 0x10; delay(400); }
        if (dump_page != last_dump_page || target_device_addr != last_target || (millis() - last_dump_time > 500)) { 
            for(int i=0; i<16; i++) dump_buffer[i] = ext_read_reg(target_device_addr, dump_page + i); 
            last_dump_page = dump_page; last_target = target_device_addr; last_dump_time = millis(); 
        }
    }
    
    static int16_t mon_vals[4] = {0};
    static char mon_lbls[4][3];
    static uint8_t mon_count = 0;
    static bool mon_rule_ok = false;

    if (popup_state == 8 && target_device_slot != 0xFF) {
        if(millis() - last_mon_time > 1000) {
            uint16_t base_addr = DEV_MEM_START + (target_device_slot * DEV_MEM_SIZE) + DEV_CMD_OFS + (15 * 32);
            uint8_t pairs = eeprom_read_byte(base_addr + 8);
            mon_count = pairs >> 1;
            if (pairs != 0xFF && mon_count > 0 && mon_count <= 4) {
                mon_rule_ok = true;
                for(int k=0; k<mon_count; k++) {
                    uint8_t reg = eeprom_read_byte(base_addr + 9 + k*4);
                    uint8_t type = eeprom_read_byte(base_addr + 10 + k*4);
                    mon_lbls[k][0] = (char)eeprom_read_byte(base_addr + 11 + k*4);
                    mon_lbls[k][1] = (char)eeprom_read_byte(base_addr + 12 + k*4);
                    mon_lbls[k][2] = '\0';
                    if(type == 1) {
                        mon_vals[k] = (int16_t)((ext_read_reg(target_device_addr, reg) << 8) | ext_read_reg(target_device_addr, reg+1));
                    } else {
                        mon_vals[k] = ext_read_reg(target_device_addr, reg);
                    }
                }
            } else {
                mon_rule_ok = false;
            }
            last_mon_time = millis();
        }
    }

    for (current_page = 0; current_page < 8; current_page++) {
        for(int i=0; i<128; i++) oled_buffer[i] = 0;
        draw_window(0,0,128,64); for(int i=0;i<128;i++) draw_pixel(i,11);
        
        if(menu_state == 1) {
            draw_string(12,3,"ESC"); draw_string(55,3,"SCN"); draw_string(98,3,"SEL");
            if(mouse_y<11){ 
                if(mouse_x<42){invert_rect(11,2,21,9);if(clicked){menu_state=0; popup_state=0; clicked=false; scan_done=false;}} 
                else if(mouse_x<85){ invert_rect(54,2,21,9); if(clicked){
                    found_count = 0; for(uint8_t addr=1; addr<128; addr++) { ext_i2c_start(); if(ext_i2c_write_ping(addr<<1)){ if(found_count < 8) found_addrs[found_count++] = addr; } ext_i2c_stop(); }
                    scan_done = true; sel_index = 0; if(found_count > 0){ target_device_addr = found_addrs[0]; lookup_device_name(target_device_addr); } else target_device_addr = 0;
                    clicked=false;
                }} 
                else if(mouse_x<128){ invert_rect(97,2,21,9); if(clicked){ if(scan_done && found_count > 0) { sel_index++; if(sel_index >= found_count) sel_index = 0; target_device_addr = found_addrs[sel_index]; lookup_device_name(target_device_addr); } clicked=false; }}
            }
            if(scan_done) { if(found_count == 0) draw_string(37,35,"NO DEVICE"); else { for(int i=0; i<found_count; i++) { uint8_t px = 5 + (i%4)*30, py = 28 + (i/4)*12; draw_hex(px, py, found_addrs[i]); if(i == sel_index) invert_rect(px-1, py-1, 26, 9); } } } else draw_string(37,35,"PRESS SCN");
        } 
        else if(menu_state == 0) {
            draw_string(12,3,"SYS"); draw_string(55,3,"SCN"); draw_string(98,3,"COM");
            if(mouse_y<11){ if(mouse_x<42){invert_rect(11,2,21,9);if(clicked){menu_state=0; popup_state=0; clicked=false;}} else if(mouse_x<85){invert_rect(54,2,21,9);if(clicked){menu_state=1; popup_state=0; clicked=false; scan_done=false;}} else if(mouse_x<128){invert_rect(97,2,21,9);if(clicked){menu_state=2; popup_state=0; clicked=false;}} }
            invert_rect(11,2,21,9); 
            draw_string(34,22,"UIAP GemOS"); draw_string(40,32,"Ver 1.25"); 
            draw_string(25,45,"I2C COMMANDER"); draw_string(40,55,"Ver 0.42");
        }
        else if(menu_state == 2) {
            draw_string(1,3,"ESC"); draw_string(22,3,"SEQ"); draw_string(43,3,"MON"); draw_string(64,3,"DMP"); draw_string(85,3,"RED"); draw_string(106,3,"WRT");
            if(mouse_y<11) {
                if(mouse_x<21){invert_rect(0,2,20,9);if(clicked){menu_state=0; popup_state=0; clicked=false;}}
                else if(mouse_x<42){invert_rect(21,2,20,9);if(clicked){if(target_device_addr!=0){popup_state=6;}clicked=false;}}
                else if(mouse_x<63){invert_rect(42,2,20,9);if(clicked){if(target_device_addr!=0){popup_state=8; last_mon_time=0;}clicked=false;}}
                else if(mouse_x<84){invert_rect(63,2,20,9);if(clicked){if(target_device_addr!=0){popup_state=2;dump_page=0;last_dump_page=0xFF;}clicked=false;}}
                else if(mouse_x<105){invert_rect(84,2,20,9);if(clicked){if(target_device_addr!=0){com_mode_select=0;edit_val=0;popup_state=3;}clicked=false;}}
                else if(mouse_x<128){invert_rect(105,2,21,9);if(clicked){if(target_device_addr!=0){com_mode_select=1;edit_val=0;popup_state=3;}clicked=false;}}
            }
            if(popup_state == 0) draw_string(34,40,"SELECT CMD");
        }

        if(target_device_addr != 0 && menu_state != 0) { draw_window(2, 13, 124, 13); draw_hex(6, 16, target_device_addr); draw_string(36, 16, current_dev_name); }

        if(popup_state == 1) { clear_rect(24, 20, 80, 24); draw_window(24, 20, 80, 24); if(ping_success) draw_string(40, 28, "PING OK!"); else draw_string(34, 28, "PING FAIL!"); }
        else if(popup_state == 2) {
            clear_rect(2, 18, 124, 44); draw_window(2, 18, 124, 44); draw_string(4, 20, "DUMP PAGE:"); draw_hex(70, 20, dump_page);
            for(int r=0; r<4; r++) { uint8_t base_reg = dump_page + (r * 4); draw_hex(4, 28 + r*9, base_reg); for(int c=0; c<4; c++) draw_hex(32 + c*22, 28 + r*9, dump_buffer[r*4 + c]); }
            if(dump_page > 0) draw_string(118, 20, "^"); if(dump_page < 0xF0) draw_string(118, 50, "v");
        }
        else if(popup_state == 3 || popup_state == 4) { clear_rect(16, 20, 96, 30); draw_window(16, 20, 96, 30); draw_string(21, 25, (popup_state == 3) ? "REG:" : "VAL:"); draw_hex(71, 25, edit_val); draw_string(31, 38, "UP/DN+CLICK"); }
        else if(popup_state == 5) { clear_rect(16, 20, 96, 30); draw_window(16, 20, 96, 30); draw_string(21, 25, (com_mode_select == 0) ? "READ:" : "WRITE OK:"); draw_hex(76, 25, last_io_val); draw_string(34, 38, "CLICK BACK"); }
        else if(popup_state == 6) {
            clear_rect(10, 18, 108, 40); draw_window(10, 18, 108, 40);
            for(int i=0; i<3; i++) {
                uint8_t pat = seq_offset + i; char sbuf[3] = {(char)(pat >= 10 ? '1' : '0'), (char)('0' + (pat >= 10 ? pat - 10 : pat)), '\0'}; uint8_t py = 23 + i*11;
                draw_string(14, py, sbuf); draw_string(28, py, ":");
                char nbuf[9]; nbuf[8] = '\0';
                if(target_device_slot != 0xFF) { uint16_t base_addr = DEV_MEM_START + (target_device_slot * DEV_MEM_SIZE) + DEV_CMD_OFS + (pat * 32); for(int j=0; j<8; j++) { char c = eeprom_read_byte(base_addr + j); nbuf[j] = (c >= 32 && c <= 126) ? c : '-'; } } else { for(int j=0; j<8; j++) nbuf[j] = '-'; }
                draw_string(36, py, nbuf);
                if(mouse_y >= py-1 && mouse_y <= py+7 && mouse_x > 12 && mouse_x < 106) {
                    invert_rect(12, py-2, 94, 11);
                    if(clicked) {
                        if(target_device_slot != 0xFF) {
                            uint16_t base_addr = DEV_MEM_START + (target_device_slot * DEV_MEM_SIZE) + DEV_CMD_OFS + (pat * 32); uint8_t count = eeprom_read_byte(base_addr + 8);
                            if(count != 0xFF && count > 0) { for(int k=0; k<count && k<11; k++) { uint8_t reg = eeprom_read_byte(base_addr + 9 + k*2); uint8_t val = eeprom_read_byte(base_addr + 10 + k*2); ext_write_reg(target_device_addr, reg, val); } }
                        }
                        popup_state = 7; clicked = false;
                    }
                }
            }
            if(seq_offset > 0) draw_string(110, 20, "^"); if(seq_offset < 13) draw_string(110, 48, "v");
        }
        else if(popup_state == 7) { clear_rect(24, 20, 80, 24); draw_window(24, 20, 80, 24); draw_string(34, 28, "SEQ SENT!"); }
        else if(popup_state == 8) {
            clear_rect(2, 18, 124, 44); draw_window(2, 18, 124, 44);
            if(target_device_slot != 0xFF) {
                if(mon_rule_ok) {
                    for(int k=0; k<mon_count; k++) {
                        uint8_t py = 22 + k*10;
                        draw_string(12, py, mon_lbls[k]); draw_string(30, py, ":"); draw_num(44, py, mon_vals[k]);
                    }
                } else {
                    draw_string(31, 36, "NO MON RULE");
                }
            }
        }

        draw_cursor(mouse_x,mouse_y);
        oled_set_pos(0,current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40); for(int x=0;x<128;x++) soft_i2c_write(oled_buffer[x]); soft_i2c_stop();
    }
}
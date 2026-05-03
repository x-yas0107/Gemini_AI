/*********************************************************************************
 * Project Name : GemOS (CH32V006 Port)
 * Version      : 0.70
 * Date         : 2026-05-02
 * Target MCU   : WCH CH32V006F8P6 (TSSOP20)
 * Developers   : yas & Gemini
 *
 * [Change History]
 * V0.70 - VM ENGINE FIX: Removed the temporary band-aid code that overwrote the 
 * 9-byte payload headers with 0x20 spaces during app load. Restored correct OS 
 * memory integrity.
 * V0.69 - MAP READ UPDATE: Added OpCode 0x1D (READ_MAP) to read tilemap data 
 * into a variable. Essential for game logic like collision and Othello rules.
 * V0.68 - CURSOR FIX: Removed !vm_running condition for draw_cursor() to keep 
 * the native OS crosshair always visible during VM execution.
 * V0.67 - SPRITE/MAP ADDRESS FIX: Corrected payload address calculation for 
 * sprite type 2 and tilemaps. Changed from (ID * 8) to (ID * 32) + 9 to 
 * properly skip the 9-byte header and align with the payload memory structure.
 * V0.66 - COMPILER FIX: Corrected multi-character constant warning in hex2byte. 
 * Replaced 'A+10' with 'A' + 10 to ensure correct ASCII to Hex conversion.
 * V0.65 - MAP RENDERING UPDATE: Added OpCode 0x1C to write to vm_map. 
 * Implemented background map rendering in menu_state 3. Added vm_map reset 
 * to OpCode 0x14. Keeps RAM usage intact while enabling 16x8 tilemaps.
 * V0.64 - Fixed DRAW NUMBER lockup: Moved vm_num_count = 0 back to frame start 
 * instead of OpCode 0x14 to ensure dynamic numbers update every frame.
 * V0.63 - VM ENGINE FIX: Corrected OpCode 0x1B (DRAW NUMBER) program counter 
 * increment from +4 to +5. Fixed VM crashing due to payload address misalignment.
 * V0.62 - Fixed OpCode 0x1B bug: Moved vm_num_count = 0 from frame start to 
 * OpCode 0x14 (Clear) to prevent premature buffer clearing.
 * V0.61 - Added OpCode 0x1B (DRAW NUMBER) to render variable values (0-255).
 * Fixed OpCode 0x1B flickering using vm_numbers buffer for page-loop rendering.
 * V0.60 - SYSTEM UPDATE: Expanded VM memory structures for complex games.
 * vm_vars: 16 -> 64, vm_sprites: 16 -> 32, vm_rects: 8 -> 64. 
 * Added vm_map[256] for board-based games. Fixed ID 00 parsing.
 * V0.59 - SYSTEM UPDATE: Added Terminal Dashboard with EEPROM Dump (D) and VM Trace 
 * Buffer (T) commands for debugging. Added VM engine execution tracking.
 *********************************************************************************/

#include "debug.h"
#include <string.h>
#include <stdbool.h>

#define SOFT_SDA 1  // PC1
#define SOFT_SCL 2  // PC2
#define OLED_ADDR 0x78
#define EEPROM_ADDR 0x50
#define DEADZONE 300

#define DEV_MEM_START 0x0800
#define DEV_MEM_SIZE  0x0800
#define DEV_LBL_OFS   0x0000
#define DEV_ID_OFS    0x0010
#define DEV_INIT_OFS  0x0020
#define DEV_CMD_OFS   0x00A0
#define DEV_DATA_OFS  0x0120
#define DEV_PAYLD_OFS 0x0220

/* --- Helper Macros --- */
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

/* --- Global Variables --- */
uint8_t mouse_x = 64;
uint8_t mouse_y = 32;
uint16_t adc_offset_x = 512;
uint16_t adc_offset_y = 512;
bool eeprom_ok = false;
uint8_t oled_buffer[128]; 
uint8_t current_page = 0; 
uint8_t font_cache[158][5]; 

uint8_t menu_state = 0;
uint8_t popup_state = 0;

uint8_t app_ver_major = 0;
uint8_t app_ver_minor = 1;

char slot_titles[31][17];
int8_t selected_slot = 0;
uint16_t sound_timer = 0;

/* --- VM Engine Variables --- */
bool vm_running = false;
uint16_t vm_pc = 0;
uint8_t vm_vars[64];       
uint8_t vm_sprites[32][4]; 
uint8_t vm_rects[64][4];   
uint8_t vm_map[256];       
uint8_t vm_memory[DEV_MEM_SIZE]; 
uint8_t vm_numbers[8][3];
uint8_t vm_num_count = 0;

uint16_t vm_trace[16] = {0};
uint8_t vm_trace_idx = 0;
uint16_t last_vm_pc = 0;

/* --- Software I2C Driver --- */
void neuron_delay_nop(volatile uint32_t count) { 
    while(count--) {
        __asm__("nop"); 
    }
}

void soft_i2c_start() { 
    GPIOC->BSHR = (1 << SOFT_SDA); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << SOFT_SCL); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << (SOFT_SDA + 16)); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << (SOFT_SCL + 16)); 
    neuron_delay_nop(1); 
}

void soft_i2c_stop() { 
    GPIOC->BSHR = (1 << (SOFT_SDA + 16)); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << SOFT_SCL); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << SOFT_SDA); 
}

void soft_i2c_write(uint8_t data) { 
    for(int i = 0; i < 8; i++) { 
        if(data & 0x80) {
            GPIOC->BSHR = (1 << SOFT_SDA); 
        } else {
            GPIOC->BSHR = (1 << (SOFT_SDA + 16)); 
        }
        neuron_delay_nop(1); 
        GPIOC->BSHR = (1 << SOFT_SCL); 
        neuron_delay_nop(1); 
        GPIOC->BSHR = (1 << (SOFT_SCL + 16)); 
        data <<= 1; 
    } 
    GPIOC->BSHR = (1 << SOFT_SDA); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << SOFT_SCL); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << (SOFT_SCL + 16)); 
}

uint8_t soft_i2c_read(bool ack) { 
    uint8_t data = 0; 
    GPIOC->BSHR = (1 << SOFT_SDA); 
    neuron_delay_nop(1); 
    for(int i = 0; i < 8; i++) { 
        data <<= 1; 
        GPIOC->BSHR = (1 << SOFT_SCL); 
        neuron_delay_nop(1); 
        if(GPIOC->INDR & (1 << SOFT_SDA)) {
            data |= 1; 
        }
        GPIOC->BSHR = (1 << (SOFT_SCL + 16)); 
        neuron_delay_nop(1); 
    } 
    if(ack) {
        GPIOC->BSHR = (1 << (SOFT_SDA + 16)); 
    } else {
        GPIOC->BSHR = (1 << SOFT_SDA); 
    }
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << SOFT_SCL); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << (SOFT_SCL + 16)); 
    neuron_delay_nop(1); 
    GPIOC->BSHR = (1 << SOFT_SDA); 
    return data; 
}

/* --- EEPROM & OLED Driver --- */
uint8_t eeprom_read_byte(uint16_t addr) { 
    soft_i2c_start(); 
    soft_i2c_write((EEPROM_ADDR << 1) | 0); 
    soft_i2c_write((uint8_t)(addr >> 8)); 
    soft_i2c_write((uint8_t)(addr & 0xFF)); 
    soft_i2c_start(); 
    soft_i2c_write((EEPROM_ADDR << 1) | 1); 
    uint8_t data = soft_i2c_read(false); 
    soft_i2c_stop(); 
    return data; 
}

void eeprom_write_byte(uint16_t addr, uint8_t data) { 
    soft_i2c_start(); 
    soft_i2c_write((EEPROM_ADDR << 1) | 0); 
    soft_i2c_write((uint8_t)(addr >> 8)); 
    soft_i2c_write((uint8_t)(addr & 0xFF)); 
    soft_i2c_write(data); 
    soft_i2c_stop(); 
    Delay_Ms(5); 
}

void oled_cmd(uint8_t cmd) { 
    soft_i2c_start(); 
    soft_i2c_write(OLED_ADDR); 
    soft_i2c_write(0x00); 
    soft_i2c_write(cmd); 
    soft_i2c_stop(); 
}

void oled_init() { 
    oled_cmd(0xAE); 
    oled_cmd(0x20); 
    oled_cmd(0x02); 
    oled_cmd(0x8D); 
    oled_cmd(0x14); 
    oled_cmd(0xAF); 
}

void oled_set_pos(uint8_t x, uint8_t page) { 
    oled_cmd(0xB0 + page); 
    oled_cmd(0x00 + (x & 0x0F)); 
    oled_cmd(0x10 + ((x >> 4) & 0x0F)); 
}

/* --- Drawing Library --- */
void draw_pixel(uint8_t x, uint8_t y) { 
    if(x >= 128 || y >= 64) return; 
    if((y >> 3) == current_page) {
        oled_buffer[x] |= (1 << (y & 7)); 
    }
}

void invert_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { 
    uint8_t p_min = current_page << 3; 
    if(y > p_min + 7 || y + h - 1 < p_min) return; 
    for(uint8_t i = 0; i < w; i++) {
        for(uint8_t j = 0; j < h; j++) {
            if(x + i < 128 && y + j < 64) {
                if(((y + j) >> 3) == current_page) {
                    oled_buffer[x + i] ^= (1 << ((y + j) & 7)); 
                }
            }
        }
    }
}

void draw_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { 
    uint8_t p_min = current_page << 3; 
    if(y > p_min + 7 || y + h - 1 < p_min) return; 
    for(uint8_t i = 0; i < w; i++) {
        draw_pixel(x + i, y);
        draw_pixel(x + i, y + h - 1);
    } 
    for(uint8_t i = 0; i < h; i++) {
        draw_pixel(x, y + i);
        draw_pixel(x + w - 1, y + i);
    } 
}

void draw_char(uint8_t x, uint8_t y, uint8_t c) { 
    uint8_t font_idx = 0; 
    if(c >= 32 && c <= 126) {
        font_idx = c - 32; 
    } else if(c >= 0xA1 && c <= 0xDF) {
        font_idx = 95 + (c - 0xA1); 
    } else {
        return;
    }
    uint8_t p_min = current_page << 3; 
    if(y > p_min + 7 || y + 7 < p_min) return; 
    
    for(uint8_t i = 0; i < 5; i++) { 
        uint8_t line = font_cache[font_idx][i]; 
        for(uint8_t j = 0; j < 7; j++) {
            if(line & (1 << j)) {
                draw_pixel(x + i, y + j);
            }
        }
    } 
}

void draw_string(uint8_t x, uint8_t y, const char* str) { 
    while(*str) {
        draw_char(x, y, (uint8_t)*str++);
        x += 6;
    } 
}

void draw_number(uint8_t x, uint8_t y, uint8_t val) {
    uint8_t h = val / 100;
    uint8_t t = (val / 10) % 10;
    uint8_t u = val % 10;
    if (h > 0) {
        draw_char(x, y, h + '0');
        draw_char(x + 6, y, t + '0');
        draw_char(x + 12, y, u + '0');
    } else if (t > 0) {
        draw_char(x, y, t + '0');
        draw_char(x + 6, y, u + '0');
    } else {
        draw_char(x, y, u + '0');
    }
}

void draw_cursor(uint8_t x, uint8_t y) { 
    invert_rect(x - 3, y, 7, 1); 
    invert_rect(x, y - 3, 1, 3); 
    invert_rect(x, y + 1, 1, 3); 
}

/* --- Serial PC Link Driver --- */
void print_char(char c) { 
    while(!(USART1->STATR & (1 << 7))); 
    USART1->DATAR = c; 
}

void print_str(const char* str) { 
    while(*str) {
        print_char(*str++); 
    }
}

void print_hex(uint8_t val) { 
    const char hex[] = "0123456789ABCDEF"; 
    print_char(hex[val >> 4]); 
    print_char(hex[val & 0x0F]); 
}

void print_num(int val) { 
    if(val < 10) {
        print_char('0'); 
        print_char(val + '0');
    } else {
        print_char((val / 10) + '0'); 
        print_char((val % 10) + '0');
    } 
}

uint8_t hex2byte(char h1, char h2) {
    uint8_t val = 0;
    if(h1 >= '0' && h1 <= '9') val += (h1 - '0') << 4; 
    else if(h1 >= 'A' && h1 <= 'F') val += (h1 - 'A' + 10) << 4; 
    else if(h1 >= 'a' && h1 <= 'f') val += (h1 - 'a' + 10) << 4;
    
    if(h2 >= '0' && h2 <= '9') val += (h2 - '0'); 
    else if(h2 >= 'A' && h2 <= 'F') val += (h2 - 'A' + 10); 
    else if(h2 >= 'a' && h2 <= 'f') val += (h2 - 'a' + 10);
    
    return val;
}

char s_read() { 
    uint32_t timeout = 48000 * 50; 
    while(!(USART1->STATR & (1 << 5))) { 
        if(--timeout == 0) return 0; 
    } 
    return USART1->DATAR; 
}

void show_dashboard() {
    print_str("\033[2J\033[H");
    print_str("========================================\r\n");
    print_str("   GemOS V0.70 TERMINAL COMMANDER\r\n");
    print_str("========================================\r\n");
    print_str(" [T] : Show VM Trace Buffer\r\n");
    print_str(" [D] : Dump EEPROM Slot (ex: D,04)\r\n");
    print_str(" [R] : Show Slot Dictionary\r\n");
    print_str(" [E] : Exit Terminal\r\n");
    print_str("----------------------------------------\r\n");
    print_str(" STATUS:\r\n");
    print_str("  VM State : ");
    print_str(vm_running ? "RUNNING\r\n" : "STOP\r\n");
    print_str("  Last PC  : 0x");
    print_hex((last_vm_pc >> 8) & 0xFF);
    print_hex(last_vm_pc & 0xFF);
    print_str("\r\n");
    print_str("----------------------------------------\r\n");
    print_str(" COMMAND > ");
}

void check_serial(bool *pc_link_mode_ptr) {
    if (USART1->STATR & (1 << 5)) {
        char cmd = USART1->DATAR;
        if(cmd == 'R' || cmd == 'r') {
            print_str("--- DICT ---\r\n");
            for(int dev = 0; dev < 31; dev++) {
                uint16_t base = DEV_MEM_START + (dev * DEV_MEM_SIZE); 
                uint8_t id = eeprom_read_byte(base + DEV_ID_OFS); 
                if(id == 0xFF) continue;
                print_str("Slot "); 
                print_num(dev); 
                print_str(" [0x"); 
                print_hex(id); 
                print_str("] : ");
                for(int i = 0; i < 16; i++) { 
                    char c = (char)eeprom_read_byte(base + DEV_LBL_OFS + i); 
                    if(c >= 32 && c <= 126) print_char(c); 
                    else print_char(' '); 
                } 
                print_str("\r\n");
            }
            print_str("--- END ---\r\n");
        } 
        else if (cmd == 'T' || cmd == 't') {
            print_str("\r\n--- VM TRACE (Last 16) ---\r\n");
            for(int i = 0; i < 16; i++) {
                uint8_t idx = (vm_trace_idx + i) & 0x0F;
                print_str(" ["); print_num(i + 1); print_str("] 0x");
                print_hex((vm_trace[idx] >> 8) & 0xFF); 
                print_hex(vm_trace[idx] & 0xFF);
                print_str("\r\n");
            }
            print_str("\r\nPress ANY KEY to return...");
            while(true) { if(s_read() != 0) break; }
            show_dashboard();
        }
        else if (cmd == 'D' || cmd == 'd') {
            if(s_read() == ',') {
                int slot = (s_read() - '0') * 10 + (s_read() - '0');
                if(slot >= 0 && slot <= 30) {
                    print_str("\r\n--- DUMP S"); print_num(slot); print_str(" ---\r\n");
                    uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE);
                    for(int i = 0; i < DEV_MEM_SIZE; i += 16) {
                        print_hex((i >> 8) & 0xFF); print_hex(i & 0xFF); print_str(" : ");
                        for(int j = 0; j < 16; j++) {
                            print_hex(eeprom_read_byte(base + i + j)); print_str(" ");
                        }
                        print_str("\r\n");
                    }
                    print_str("\r\nPress ANY KEY to return...");
                    while(true) { if(s_read() != 0) break; }
                    show_dashboard();
                }
            }
        }
        else if (cmd == 'E' || cmd == 'e') {
            *pc_link_mode_ptr = false;
            print_str("\r\n--- PC LINK OFF ---\r\n");
        }
        else if(cmd == 'V' || cmd == 'v') {
            if(s_read() == ',') {
                uint8_t maj = hex2byte(s_read(), s_read());
                if(s_read() == ',') {
                    uint8_t min = hex2byte(s_read(), s_read());
                    eeprom_write_byte(1, maj);
                    eeprom_write_byte(2, min);
                    app_ver_major = maj;
                    app_ver_minor = min;
                    print_str("Save VER\r\n");
                }
            }
        }
        else if(cmd == 'W' || cmd == 'w') {
            if(s_read() == ',') {
                int slot = (s_read() - '0') * 10 + (s_read() - '0');
                if(s_read() == ',' && slot >= 0 && slot <= 30) {
                    uint8_t id = hex2byte(s_read(), s_read());
                    if(s_read() == ',') {
                        char name[16]; 
                        for(int i = 0; i < 16; i++) name[i] = ' '; 
                        int n_idx = 0;
                        while(true) { 
                            char c = s_read(); 
                            if(c == 0 || c == '\r' || c == '\n') break; 
                            if(n_idx < 16) name[n_idx++] = c; 
                        }
                        uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE); 
                        eeprom_write_byte(base + DEV_ID_OFS, id);
                        for(int i = 0; i < 16; i++) {
                            eeprom_write_byte(base + DEV_LBL_OFS + i, name[i]);
                        }
                        print_str("Save S"); 
                        print_num(slot); 
                        print_str("\r\n");
                    }
                }
            }
        }
        else if(cmd == 'S' || cmd == 's') {
            if(s_read() == ',') {
                int slot = (s_read() - '0') * 10 + (s_read() - '0');
                if(s_read() == ',' && slot >= 0 && slot <= 30) {
                    int pat = (s_read() - '0') * 10 + (s_read() - '0');
                    if(s_read() == ',' && pat >= 0 && pat <= 58) {
                        char name[8]; 
                        for(int i = 0; i < 8; i++) name[i] = ' '; 
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
                                for(int i = 0; i < count && i < 22; i++) { 
                                    payload[i] = hex2byte(s_read(), s_read()); 
                                }
                                uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE) + DEV_CMD_OFS + (pat * 32);
                                for(int i = 0; i < 8; i++) {
                                    eeprom_write_byte(base + i, name[i]); 
                                }
                                eeprom_write_byte(base + 8, count);
                                for(int i = 0; i < count && i < 22; i++) {
                                    eeprom_write_byte(base + 9 + i, payload[i]);
                                }
                                print_str("Save Q"); 
                                print_num(pat); 
                                print_str("\r\n");
                            }
                        }
                    }
                }
            }
        }
        else if(cmd == 'F' || cmd == 'f') {
            if(s_read() == ',' && s_read() == '1' && s_read() == '2' && s_read() == '3' && s_read() == '4') {
                print_str("--- FMT ---\r\n");
                for(int slot = 3; slot <= 30; slot++) {
                    uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE); 
                    eeprom_write_byte(base + DEV_ID_OFS, 0xFF);
                    for(int i = 0; i < 16; i++) {
                        eeprom_write_byte(base + DEV_LBL_OFS + i, 0xFF);
                    }
                }
                print_str("--- OK ---\r\n");
            }
        }
    }
}

/* --- Hardware Utilities --- */
void sound_init() {
    RCC->PB1PCENR |= RCC_TIM2EN;
    RCC->PB2PCENR |= RCC_IOPDEN;

    GPIOD->CFGLR &= ~(0xF << 12);
    GPIOD->CFGLR |= (0xB << 12); 

    TIM2->PSC = 48 - 1; 
    TIM2->ATRLR = 1000; 
    TIM2->CHCTLR1 |= (0x6 << 12); 
    TIM2->CCER |= (1 << 4);       
    TIM2->CH2CVR = 0;             
    TIM2->CTLR1 |= TIM_CEN;
}

void beep_start(uint16_t freq, uint16_t duration) {
    if (freq == 0) return;
    TIM2->ATRLR = 1000000 / freq;
    TIM2->CH2CVR = TIM2->ATRLR / 2; 
    sound_timer = duration;
}

void sound_update() {
    if (sound_timer > 0) {
        sound_timer--;
    } else {
        TIM2->CH2CVR = 0;
    }
}

uint16_t adc_read(uint8_t ch) {
    ADC1->RSQR3 = ch; 
    ADC1->CTLR2 |= ADC_SWSTART; 
    while(!(ADC1->STATR & ADC_EOC));
    uint16_t dummy = ADC1->RDATAR; 
    (void)dummy; 
    ADC1->RSQR3 = ch; 
    ADC1->CTLR2 |= ADC_SWSTART; 
    while(!(ADC1->STATR & ADC_EOC));
    return (uint16_t)ADC1->RDATAR;
}

/* --- Hardware Setup --- */
void setup() {
    SystemInit();
    Delay_Init();
    RCC->PB2PCENR |= RCC_AFIOEN | RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN | RCC_ADC1EN | RCC_USART1EN;
    
    GPIOC->CFGLR &= ~0xFFFFFFFF;
    GPIOC->CFGLR |= (0x8 << 28) | (0x8 << 24) | (0x8 << 20) | (0x8 << 16) | (0x8 << 12) | (0x5 << 8) | (0x5 << 4) | (0x3 << 0);
    GPIOC->BSHR = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

    GPIOD->CFGLR &= ~0xFFFFFFFF;
    GPIOD->CFGLR |= (0x8 << 28) | (0x8 << 24) | (0xB << 20) | (0x8 << 16) | (0x3 << 12) | (0x3 << 8) | (0x8 << 4) | (0x8 << 0);
    GPIOD->BSHR = (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

    GPIOA->CFGLR &= ~0xFFFFFFFF;
    GPIOA->CFGLR |= (0x8 << 28) | (0x8 << 24) | (0x8 << 20) | (0x8 << 16) | (0x8 << 12) | (0x0 << 8) | (0x0 << 4) | (0x8 << 0);
    GPIOA->BSHR = (1 << 0) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

    USART1->BRR = 0x1A1; 
    USART1->CTLR1 = 0x200C;

    ADC1->CTLR2 |= (1 << 20) | (7 << 17) | ADC_ADON; 
    Delay_Ms(5);
    oled_init();
    sound_init();
    
    adc_offset_x = adc_read(1); 
    adc_offset_y = adc_read(0);
    
    soft_i2c_start(); 
    soft_i2c_write((EEPROM_ADDR << 1) | 0); 
    soft_i2c_write(0); 
    soft_i2c_write(0); 
    soft_i2c_write(0xAA); 
    soft_i2c_stop(); 
    Delay_Ms(5);
    
    if(eeprom_read_byte(0) == 0xAA) { 
        eeprom_ok = true; 
        
        uint8_t e_maj = eeprom_read_byte(1);
        uint8_t e_min = eeprom_read_byte(2);
        if(e_maj != 0xFF) app_ver_major = e_maj;
        if(e_min != 0xFF) app_ver_minor = e_min;

        for(int i = 0; i < 95; i++) { 
            uint16_t addr = 0x0100 + i * 5; 
            for(int j = 0; j < 5; j++) {
                font_cache[i][j] = eeprom_read_byte(addr + j); 
            }
        } 
        for(int i = 0; i < 63; i++) { 
            uint16_t addr = 0x02DB + i * 5; 
            for(int j = 0; j < 5; j++) {
                font_cache[95 + i][j] = eeprom_read_byte(addr + j); 
            }
        } 

        for(int j = 0; j < 5; j++) {
            font_cache[0][j] = 0x00;
        }
        
        for(int i = 0; i < 31; i++) {
            for(int j = 0; j < 16; j++) slot_titles[i][j] = ' ';
            slot_titles[i][16] = '\0';
        }

        for (int slot = 0; slot < 31; slot++) {
            uint16_t base_addr = DEV_MEM_START + (slot * DEV_MEM_SIZE);
            uint8_t id = eeprom_read_byte(base_addr + DEV_ID_OFS);
            if(id != 0xFF) {
                for (int i = 0; i < 16; i++) {
                    char c = (char)eeprom_read_byte(base_addr + DEV_LBL_OFS + i);
                    slot_titles[slot][i] = ((c >= 32 && c <= 126) || (c >= 0xA1 && c <= 0xDF)) ? c : ' ';
                }
            }
        }
    }
}

/* --- Main Loop --- */
int main(void) {
    setup();
    static bool pc_link_mode = false;
    bool req_pc_link = false;
    static bool last_sw = true;
    
    while(1) {
        sound_update();
        bool cur_sw = (GPIOC->INDR & (1 << 4));
        bool clicked = (last_sw && !cur_sw);
        last_sw = cur_sw;

        if (req_pc_link) {
            pc_link_mode = true;
            req_pc_link = false;
            show_dashboard();
            for (current_page = 0; current_page < 8; current_page++) {
                for(int i = 0; i < 128; i++) oled_buffer[i] = 0;
                draw_window(0, 0, 128, 64); 
                draw_string(43, 24, "PC LINK"); 
                draw_string(40, 40, "SW: EXIT");
                oled_set_pos(0, current_page); 
                soft_i2c_start(); 
                soft_i2c_write(OLED_ADDR); 
                soft_i2c_write(0x40); 
                for(int x = 0; x < 128; x++) soft_i2c_write(oled_buffer[x]); 
                soft_i2c_stop();
            }
            continue;
        }

        if (pc_link_mode) { 
            if (clicked) {
                pc_link_mode = false;
                print_str("\r\n--- PC LINK OFF ---\r\n");
            } else {
                check_serial(&pc_link_mode); 
            }
            continue; 
        }

        uint16_t x_raw = adc_read(1);
        uint16_t y_raw = adc_read(0); 
        
        int16_t dx = 0;
        int16_t dy = 0;
        int16_t diff_x = (int16_t)x_raw - (int16_t)adc_offset_x;
        int16_t diff_y = (int16_t)y_raw - (int16_t)adc_offset_y;
        
        if(diff_x > DEADZONE) dx = diff_x / 128; 
        else if(diff_x < -DEADZONE) dx = diff_x / 128;
        
        if(diff_y > DEADZONE) dy = diff_y / 128; 
        else if(diff_y < -DEADZONE) dy = diff_y / 128;
        
        mouse_x = (uint8_t)constrain(mouse_x + dx, 0, 127); 
        mouse_y = (uint8_t)constrain(mouse_y + dy, 0, 63);

        int16_t abs_dy = diff_y;
        if(abs_dy < 0) abs_dy = -abs_dy;
        
        uint8_t cur_scroll_wait = 25;
        if(abs_dy > 450) cur_scroll_wait = 4;
        else if(abs_dy > 380) cur_scroll_wait = 10;

        /* --- VM Engine Execution (1 Frame Pass) --- */
        if (menu_state == 3 && vm_running) {
            vm_num_count = 0;
            int runaway = 0;
            while(runaway++ < 100) {
                if(vm_pc >= (DEV_MEM_SIZE - DEV_CMD_OFS - 6)) {
                    vm_running = false;
                    break;
                }

                vm_trace[vm_trace_idx] = DEV_CMD_OFS + vm_pc;
                vm_trace_idx = (vm_trace_idx + 1) & 0x0F;
                last_vm_pc = DEV_CMD_OFS + vm_pc;

                uint8_t op = vm_memory[DEV_CMD_OFS + vm_pc];
                uint8_t p1 = vm_memory[DEV_CMD_OFS + vm_pc + 1];
                uint8_t p2 = vm_memory[DEV_CMD_OFS + vm_pc + 2];
                uint8_t p3 = vm_memory[DEV_CMD_OFS + vm_pc + 3];
                uint8_t p4 = vm_memory[DEV_CMD_OFS + vm_pc + 4];

                if(op == 0x00) { 
                    vm_pc++; 
                } 
                else if(op == 0x01) { 
                    vm_vars[p1 & 0x3F] = p2; 
                    vm_pc += 3; 
                } 
                else if(op == 0x02) { 
                    vm_vars[p1 & 0x3F] += p2; 
                    vm_pc += 3; 
                } 
                else if(op == 0x0A) { 
                    vm_vars[p1 & 0x3F] -= p2; 
                    vm_pc += 3; 
                } 
                else if(op == 0x0B) { 
                    vm_vars[p1 & 0x3F] = mouse_x;
                    vm_vars[p2 & 0x3F] = mouse_y;
                    vm_pc += 3;
                }
                else if(op == 0x03) { 
                    uint8_t idx = p1 & 0x1F;
                    vm_sprites[idx][0] = vm_vars[p2 & 0x3F];
                    vm_sprites[idx][1] = vm_vars[p3 & 0x3F];
                    vm_sprites[idx][2] = 1;
                    vm_sprites[idx][3] = p4;
                    vm_pc += 5;
                }
                else if(op == 0x04) { 
                    vm_vars[p1 & 0x3F] = (dx > 0) ? 1 : ((dx < 0) ? 255 : 0);
                    vm_vars[p2 & 0x3F] = (dy > 0) ? 1 : ((dy < 0) ? 255 : 0);
                    vm_pc += 3;
                }
                else if(op == 0x05) { 
                    uint8_t b = 0;
                    if(!(GPIOD->INDR & (1 << 0))) b |= 1; 
                    if(!(GPIOC->INDR & (1 << 3))) b |= 2; 
                    if(!cur_sw) b |= 4;                   
                    vm_vars[p1 & 0x3F] = b;
                    vm_pc += 2;
                }
                else if(op == 0x06) { 
                    vm_pc = (p1 << 8) | p2;
                }
                else if(op == 0x07) { 
                    if(vm_vars[p1 & 0x3F] == p2) vm_pc = (p3 << 8) | p4;
                    else vm_pc += 5;
                }
                else if(op == 0x0C) { 
                    if(vm_vars[p1 & 0x3F] > p2) vm_pc = (p3 << 8) | p4;
                    else vm_pc += 5;
                }
                else if(op == 0x0D) { 
                    if(vm_vars[p1 & 0x3F] < p2) vm_pc = (p3 << 8) | p4;
                    else vm_pc += 5;
                }
                else if(op == 0x0E) { 
                    if(vm_vars[p1 & 0x3F] != p2) vm_pc = (p3 << 8) | p4;
                    else vm_pc += 5;
                }
                else if(op == 0x0F) { 
                    static uint8_t seed = 0x55;
                    seed ^= (seed << 3); seed ^= (seed >> 5); seed ^= (seed << 4);
                    vm_vars[p1 & 0x3F] = (p2 > 0) ? (seed % p2) : 0;
                    vm_pc += 3;
                }
                else if(op == 0x10) { 
                    vm_vars[p1 & 0x3F] *= p2;
                    vm_pc += 3;
                }
                else if(op == 0x11) { 
                    if(p2 != 0) vm_vars[p1 & 0x3F] /= p2;
                    vm_pc += 3;
                }
                else if(op == 0x12) { 
                    if(p2 != 0) vm_vars[p1 & 0x3F] %= p2;
                    vm_pc += 3;
                }
                else if(op == 0x13) { 
                    vm_vars[p1 & 0x3F] = constrain(vm_vars[p1 & 0x3F], p2, p3);
                    vm_pc += 4;
                }
                else if(op == 0x14) { 
                    for(int i = 0; i < 32; i++) vm_sprites[i][2] = 0;
                    for(int i = 0; i < 64; i++) vm_rects[i][2] = 0;
                    for(int i = 0; i < 256; i++) vm_map[i] = 0;
                    vm_pc += 1;
                }
                else if(op == 0x15) { 
                    uint8_t idx = p1 & 0x3F;
                    vm_rects[idx][0] = vm_vars[p2 & 0x3F];
                    vm_rects[idx][1] = vm_vars[p3 & 0x3F];
                    vm_rects[idx][2] = 1;
                    vm_rects[idx][3] = 1;
                    vm_pc += 4;
                }
                else if(op == 0x17) { 
                    uint8_t idx = p1 & 0x3F;
                    vm_rects[idx][0] = vm_vars[p2 & 0x3F];
                    vm_rects[idx][1] = vm_vars[p3 & 0x3F];
                    vm_rects[idx][2] = p4;
                    vm_rects[idx][3] = vm_memory[DEV_CMD_OFS + vm_pc + 5];
                    vm_pc += 6;
                }
                else if(op == 0x1A) {
                    uint8_t vx = vm_vars[p1 & 0x3F];
                    uint8_t vy = vm_vars[p2 & 0x3F];
                    uint8_t r_idx = p3 & 0x3F;
                    uint8_t rx = vm_rects[r_idx][0];
                    uint8_t ry = vm_rects[r_idx][1];
                    uint8_t rw = vm_rects[r_idx][2];
                    uint8_t rh = vm_rects[r_idx][3];
                    if (rw > 0 && (vx + 2) >= rx && vx < rx + rw && (vy + 2) >= ry && vy < ry + rh) {
                        vm_pc = (p4 << 8) | vm_memory[DEV_CMD_OFS + vm_pc + 5];
                    } else {
                        vm_pc += 6;
                    }
                }
                else if(op == 0x18) { 
                    uint16_t freq = (p1 << 8) | p2;
                    if(freq < 100) freq = 400;
                    beep_start(freq, p3); 
                    vm_pc += 4; 
                }
                else if(op == 0x19) { 
                    uint8_t idx = p1 & 0x1F;
                    vm_sprites[idx][0] = vm_vars[p2 & 0x3F];
                    vm_sprites[idx][1] = vm_vars[p3 & 0x3F];
                    vm_sprites[idx][2] = 2; 
                    vm_sprites[idx][3] = p4; 
                    vm_pc += 5;
                }
                else if(op == 0x1B) { 
                    uint8_t val = vm_vars[p1 & 0x3F];
                    if (vm_num_count < 8) {
                        vm_numbers[vm_num_count][0] = val;
                        vm_numbers[vm_num_count][1] = p2;
                        vm_numbers[vm_num_count][2] = p3;
                        vm_num_count++;
                    }
                    vm_pc += 5; 
                }
                else if(op == 0x1C) {
                    uint8_t map_idx = vm_vars[p1 & 0x3F];
                    vm_map[map_idx] = vm_vars[p2 & 0x3F];
                    vm_pc += 3;
                }
                else if(op == 0x1D) {
                    uint8_t map_idx = vm_vars[p1 & 0x3F];
                    vm_vars[p2 & 0x3F] = vm_map[map_idx];
                    vm_pc += 3;
                }
                else if(op == 0x08) { 
                    beep_start(2000, 10); 
                    vm_pc += 1; 
                } 
                else if(op == 0x09) { 
                    vm_pc += 1; 
                    break; 
                }  
                else { 
                    vm_pc++; 
                } 
            }
        }

        for (current_page = 0; current_page < 8; current_page++) {
            for(int i = 0; i < 128; i++) oled_buffer[i] = 0;
            
            if (menu_state == 4) {
                draw_string(25, 2, "HARDWARE TEST");
                draw_string(52, 54, "EXIT");
                
                draw_window(18, 16, 8, 8); 
                draw_window(18, 36, 8, 8); 
                draw_window(6, 26, 8, 8);  
                draw_window(30, 26, 8, 8); 
                if(!(GPIOC->INDR & (1 << 5))) invert_rect(19, 17, 6, 6); 
                if(!(GPIOC->INDR & (1 << 6))) invert_rect(19, 37, 6, 6); 
                if(!(GPIOC->INDR & (1 << 7))) invert_rect(7, 27, 6, 6);  
                if(!(GPIOD->INDR & (1 << 4))) invert_rect(31, 27, 6, 6); 
                
                draw_window(48, 16, 32, 32);
                
                int16_t center_x = 63; 
                int16_t center_y = 31; 
                int16_t dot_diff_x = (diff_x > DEADZONE || diff_x < -DEADZONE) ? diff_x : 0;
                int16_t dot_diff_y = (diff_y > DEADZONE || diff_y < -DEADZONE) ? diff_y : 0;
                
                int16_t dot_x = center_x + ((int32_t)dot_diff_x * 22) / adc_offset_x;
                int16_t dot_y = center_y + ((int32_t)dot_diff_y * 22) / adc_offset_y;
                
                dot_x = constrain(dot_x, 49, 77); 
                dot_y = constrain(dot_y, 17, 45); 
                
                invert_rect((uint8_t)dot_x, (uint8_t)dot_y, 2, 2);
                
                draw_string(93, 14, "A"); draw_window(105, 12, 8, 8);
                draw_string(93, 24, "B"); draw_window(105, 22, 8, 8);
                draw_string(87, 34, "SW"); draw_window(105, 32, 8, 8);
                draw_string(87, 44, "SP"); draw_window(105, 42, 8, 8);
                
                if(!(GPIOD->INDR & (1 << 0))) invert_rect(106, 13, 6, 6); 
                if(!(GPIOC->INDR & (1 << 3))) invert_rect(106, 23, 6, 6); 
                if(!cur_sw) invert_rect(106, 33, 6, 6);                   
                
                if(mouse_x > 100 && mouse_y > 40 && mouse_y < 52) {
                    invert_rect(105, 42, 8, 8);
                    if(clicked) { 
                        beep_start(1000, 10); 
                        clicked = false; 
                    }
                }
                if(mouse_x > 45 && mouse_x < 80 && mouse_y > 50) {
                    invert_rect(50, 53, 28, 9);
                    if(clicked) { 
                        menu_state = 0; 
                        clicked = false; 
                    }
                }
            }
            else if (menu_state == 3) {
                if (!vm_running) {
                    menu_state = 2;
                } else {
                    for(int i = 0; i < 128; i++) {
                        uint8_t tile = vm_map[i];
                        if(tile > 0) {
                            uint8_t tx = (i & 0x0F) << 3; 
                            uint8_t ty = (i >> 4) << 3;
                            uint16_t addr = DEV_CMD_OFS + (tile * 32) + 9;
                            uint8_t p_min = current_page << 3; 
                            if(ty <= p_min + 7 && ty + 7 >= p_min) {
                                for(uint8_t bx = 0; bx < 8; bx++) {
                                    uint8_t line = vm_memory[addr + bx]; 
                                    for(uint8_t by = 0; by < 8; by++) {
                                        if(line & (1 << by)) {
                                            draw_pixel(tx + bx, ty + by);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    for(int i = 0; i < 32; i++) {
                        if(vm_sprites[i][2] == 1) {
                            draw_char(vm_sprites[i][0], vm_sprites[i][1], vm_sprites[i][3]);
                        } else if(vm_sprites[i][2] == 2) {
                            uint8_t x = vm_sprites[i][0];
                            uint8_t y = vm_sprites[i][1];
                            uint16_t addr = DEV_CMD_OFS + (vm_sprites[i][3] * 32) + 9;
                            uint8_t p_min = current_page << 3; 
                            if(y <= p_min + 7 && y + 7 >= p_min) {
                                for(uint8_t bx = 0; bx < 8; bx++) {
                                    uint8_t line = vm_memory[addr + bx]; 
                                    for(uint8_t by = 0; by < 8; by++) {
                                        if(line & (1 << by)) {
                                            draw_pixel(x + bx, y + by);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    for(int i = 0; i < 64; i++) {
                        if(vm_rects[i][2] > 0) {
                            draw_window(vm_rects[i][0], vm_rects[i][1], vm_rects[i][2], vm_rects[i][3]);
                        }
                    }
                    for(int i = 0; i < vm_num_count; i++) {
                        draw_number(vm_numbers[i][1], vm_numbers[i][2], vm_numbers[i][0]);
                    }
                }
                
                if (mouse_y <= 5) {
                    draw_string(12, 3, "EXT");
                    invert_rect(0, 0, 128, 12);
                    if (mouse_x < 40) { 
                        invert_rect(11, 2, 21, 9); 
                        if (clicked) { 
                            menu_state = 0; 
                            vm_running = false; 
                            clicked = false; 
                        } 
                    }
                }
            }
            else if (menu_state == 5) {
                draw_window(0, 0, 128, 64); 
                for(int i = 0; i < 128; i++) draw_pixel(i, 11);
                
                draw_string(12, 3, "EXT"); 
                draw_string(45, 3, "APP MENU"); 
                
                static uint8_t app_list_top = 0;
                static uint8_t app_scroll_wait = 0;
                
                if(current_page == 0 && app_scroll_wait > 0) app_scroll_wait--;
                
                if(mouse_y < 11){ 
                    if(mouse_x < 40){ 
                        invert_rect(11, 2, 21, 9); 
                        if(clicked){ menu_state = 0; clicked = false; } 
                    } 
                }

                if (mouse_y > 57) {
                    if(app_list_top < 27 && app_scroll_wait == 0) {
                        app_list_top++;
                        app_scroll_wait = cur_scroll_wait; 
                    }
                } 
                else if (mouse_y < 18) {
                    if(app_list_top > 0 && app_scroll_wait == 0) {
                        app_list_top--;
                        app_scroll_wait = cur_scroll_wait; 
                    }
                }

                bool do_launch = false;
                if (mouse_y >= 18 && mouse_y < 28 && clicked) { do_launch = true; clicked = false; }
                else if (mouse_y >= 28 && mouse_y < 38 && clicked) { do_launch = true; clicked = false; }
                else if (mouse_y >= 38 && mouse_y < 48 && clicked) { do_launch = true; clicked = false; }
                else if (mouse_y >= 48 && mouse_y < 58 && clicked) { do_launch = true; clicked = false; }
                
                if (do_launch) {
                    beep_start(2000, 10); 
                }

                for(int s = 0; s < 4; s++) {
                    uint8_t slot_idx = app_list_top + s;
                    if(slot_idx <= 30) {
                        uint8_t row_y = 20 + (s * 10);
                        draw_string(4, row_y, "A"); 
                        draw_char(10, row_y, (slot_idx / 10) + '0'); 
                        draw_char(16, row_y, (slot_idx % 10) + '0'); 
                        draw_string(22, row_y, ":");
                        draw_string(28, row_y, "NO APP DATA");
                    }
                }
            }
            else if (menu_state == 2) {
                draw_window(0, 0, 128, 64); 
                for(int i = 0; i < 128; i++) draw_pixel(i, 11);
                
                draw_string(12, 3, "EXT"); 
                draw_string(45, 3, "SELECT"); 
                
                static uint8_t list_top_index = 0;
                static uint8_t scroll_wait = 0;
                
                if(current_page == 0 && scroll_wait > 0) scroll_wait--;
                
                if(mouse_y < 11){ 
                    if(mouse_x < 40){ 
                        invert_rect(11, 2, 21, 9); 
                        if(clicked){ menu_state = 0; clicked = false; } 
                    } 
                }

                if (mouse_y > 57) {
                    if(list_top_index < 27 && scroll_wait == 0) {
                        list_top_index++;
                        scroll_wait = cur_scroll_wait; 
                    }
                } 
                else if (mouse_y < 18) {
                    if(list_top_index > 0 && scroll_wait == 0) {
                        list_top_index--;
                        scroll_wait = cur_scroll_wait; 
                    }
                }

                bool do_launch = false;
                if (mouse_y >= 18 && mouse_y < 28) {
                    selected_slot = list_top_index;
                    if (clicked) { do_launch = true; clicked = false; }
                }
                else if (mouse_y >= 28 && mouse_y < 38) {
                    selected_slot = list_top_index + 1;
                    if (clicked) { do_launch = true; clicked = false; }
                }
                else if (mouse_y >= 38 && mouse_y < 48) {
                    selected_slot = list_top_index + 2;
                    if (clicked) { do_launch = true; clicked = false; }
                }
                else if (mouse_y >= 48 && mouse_y < 58) {
                    selected_slot = list_top_index + 3;
                    if (clicked) { do_launch = true; clicked = false; }
                }
                
                if (do_launch) {
                    uint16_t base = DEV_MEM_START + (selected_slot * DEV_MEM_SIZE);
                    for(uint16_t i = 0; i < DEV_MEM_SIZE; i++) {
                        vm_memory[i] = eeprom_read_byte(base + i);
                    }
                    for(int b = 0; b <= 58; b++) {
                        uint16_t block_base = DEV_CMD_OFS + (b * 32);
                        uint8_t count = vm_memory[block_base + 8];
                        if(count > 22) count = 22;
                        for(int j = 9 + count; j < 32; j++) vm_memory[block_base + j] = 0x20;
                    }
                    vm_running = true; 
                    vm_pc = 0;
                    vm_trace_idx = 0;
                    for(int i = 0; i < 16; i++) vm_trace[i] = 0;
                    for(int i = 0; i < 64; i++) vm_vars[i] = 0; 
                    for(int i = 0; i < 32; i++) { 
                        vm_sprites[i][0] = 0; 
                        vm_sprites[i][1] = 0; 
                        vm_sprites[i][2] = 0; 
                        vm_sprites[i][3] = 0; 
                    }
                    for(int i = 0; i < 64; i++) {
                        vm_rects[i][0] = 0;
                        vm_rects[i][1] = 0;
                        vm_rects[i][2] = 0;
                        vm_rects[i][3] = 0;
                    }
                    for(int i = 0; i < 256; i++) vm_map[i] = 0;
                    vm_num_count = 0;
                    menu_state = 3;
                }

                if (!eeprom_ok) {
                    draw_string(20, 32, "NO EEPROM!");
                } else {
                    for(int s = 0; s < 4; s++) {
                        uint8_t slot_idx = list_top_index + s;
                        if(slot_idx <= 30) {
                            uint8_t row_y = 20 + (s * 10);
                            draw_string(4, row_y, "S"); 
                            draw_char(10, row_y, (slot_idx / 10) + '0'); 
                            draw_char(16, row_y, (slot_idx % 10) + '0'); 
                            draw_string(22, row_y, ":");
                            draw_string(28, row_y, slot_titles[slot_idx]);
                            if(slot_idx == selected_slot) {
                                invert_rect(2, row_y - 1, 124, 9);
                            }
                        }
                    }
                }
            }
            else {
                draw_window(0, 0, 128, 64); 
                for(int i = 0; i < 128; i++) draw_pixel(i, 11);
                
                draw_string(12, 3, "SYS"); 
                draw_string(55, 3, "GEME"); 
                draw_string(98, 3, "APP");
                
                if(mouse_y < 11){ 
                    if(mouse_x < 42){ 
                        invert_rect(11, 2, 21, 9); 
                        if(clicked){ menu_state = (menu_state == 1) ? 0 : 1; clicked = false; } 
                    } 
                    else if(mouse_x < 85){ 
                        invert_rect(54, 2, 24, 9); 
                        if(clicked){ menu_state = (menu_state == 2) ? 0 : 2; clicked = false; } 
                    } 
                    else if(mouse_x < 128){ 
                        invert_rect(97, 2, 21, 9); 
                        if(clicked){ menu_state = (menu_state == 5) ? 0 : 5; clicked = false; } 
                    } 
                } else {
                    if(clicked && menu_state != 0 && !(menu_state == 1 && mouse_x < 70 && mouse_y < 36)) {
                        menu_state = 0;
                        clicked = false;
                    }
                }

                if(menu_state == 0) {
                    draw_string(28, 18, "32V006 GemOS"); 
                    draw_string(40, 30, "Ver 0.70");
                    draw_string(25, 44, "APP Ver ");
                    draw_char(73, 44, (app_ver_major / 10) + '0');
                    draw_char(79, 44, (app_ver_major % 10) + '0');
                    draw_char(85, 44, '.');
                    draw_char(91, 44, (app_ver_minor / 10) + '0');
                    draw_char(97, 44, (app_ver_minor % 10) + '0');
                }
                else if(menu_state == 1) {
                    invert_rect(11, 2, 21, 9); 
                    draw_window(11, 11, 60, 26); 
                    draw_string(17, 14, "I/O TEST");
                    draw_string(17, 25, "PC LINK");
                    if(mouse_x > 11 && mouse_x < 71) {
                        if(mouse_y > 11 && mouse_y <= 21) {
                            invert_rect(13, 13, 56, 10); 
                            if(clicked){ menu_state = 4; clicked = false; }
                        } else if(mouse_y > 21 && mouse_y < 36) {
                            invert_rect(13, 24, 56, 10); 
                            if(clicked){ req_pc_link = true; clicked = false; menu_state = 0; }
                        }
                    }
                }
            }

            draw_cursor(mouse_x, mouse_y); 
            oled_set_pos(0, current_page); 
            soft_i2c_start(); 
            soft_i2c_write(OLED_ADDR); 
            soft_i2c_write(0x40); 
            for(int x = 0; x < 128; x++) soft_i2c_write(oled_buffer[x]); 
            soft_i2c_stop();
        }
    }
}
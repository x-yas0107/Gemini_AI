/* UIAPduino_GemOS V1.25 (APP Ver0.32c - Size Fix)
 * Ver0.32c : フラッシュ容量対策（履歴15行 + 文字列短縮）
 * Ver0.32b : 履歴20行
 * Ver0.32  : Err表示追加
 * Ver0.31  : パース堅牢化 + 連続クリック対策
 */
#include <Arduino.h>

#define SOFT_SDA 6
#define SOFT_SCL 7
#define OLED_ADDR 0x78
#define EEPROM_ADDR 0x50

#define DEV_MEM_START 0x0800
#define DEV_MEM_SIZE  0x0800
#define DEV_LBL_OFS   0x0000
#define DEV_ID_OFS    0x0010
#define DEV_CMD_OFS   0x00A0

const uint8_t font_5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5f,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1c,0x00}, {0x14,0x08,0x3e,0x08,0x14}, {0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e}, {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7f,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7f,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7f,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7f}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7e,0x09,0x01,0x02}, {0x0c,0x52,0x52,0x52,0x3e},
    {0x7f,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7d,0x40,0x00}, {0x20,0x40,0x44,0x3d,0x00}, {0x7f,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7f,0x40,0x00}, {0x7c,0x04,0x18,0x04,0x78}, {0x7c,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7c,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7c}, {0x7c,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3f,0x44,0x40,0x20}, {0x3c,0x40,0x40,0x20,0x7c}, {0x1c,0x20,0x40,0x20,0x1c}, {0x3c,0x40,0x30,0x40,0x3c},
    {0x44,0x28,0x10,0x28,0x44}, {0x0c,0x50,0x50,0x50,0x3c}, {0x44,0x64,0x54,0x4c,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7f,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08}
};

uint8_t mouse_x = 64, mouse_y = 32;
bool eeprom_ok = false;
uint8_t oled_buffer[128];
uint8_t current_page = 0;
uint8_t menu_state = 0;
uint8_t popup_state = 0;

int32_t calc_val = 0;
int32_t calc_prev = 0;
char calc_op = 0;
bool calc_new = true;
bool is_result = false;
int8_t input_frac_digits = -1;
uint8_t base_mode = 0;
bool calc_error = false;

bool mouse_click_buf = false;
bool mouse_click_latch = false;
uint8_t latch_click_x = 0;
uint8_t latch_click_y = 0;

uint8_t rx_state = 0;
int16_t rx_val = 0;
bool rx_neg = false;
int16_t rx_dx = 0, rx_dy = 0;

void poll_serial(void);

void neuron_delay_nop(volatile uint32_t count) { while(count--) __asm__("nop"); }
void soft_i2c_start() { GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); neuron_delay_nop(1); }
void soft_i2c_stop() { GPIOC->BSHR=(1<<(SOFT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SDA); }
void soft_i2c_write(uint8_t data) { for(int i=0;i<8;i++){ if(data&0x80)GPIOC->BSHR=(1<<SOFT_SDA); else GPIOC->BSHR=(1<<(SOFT_SDA+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); data<<=1; } GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); }
uint8_t soft_i2c_read(bool ack) { uint8_t data=0; GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); for(int i=0;i<8;i++){ data<<=1; GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); if(GPIOC->INDR&(1<<SOFT_SDA))data|=1; GPIOC->BSHR=(1<<(SOFT_SCL+16)); neuron_delay_nop(1); } if(ack)GPIOC->BSHR=(1<<(SOFT_SDA+16)); else GPIOC->BSHR=(1<<SOFT_SDA); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SCL); neuron_delay_nop(1); GPIOC->BSHR=(1<<(SOFT_SCL+16)); neuron_delay_nop(1); GPIOC->BSHR=(1<<SOFT_SDA); return data; }

uint8_t eeprom_read_byte(uint16_t addr) { soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write((uint8_t)(addr>>8)); soft_i2c_write((uint8_t)(addr&0xFF)); soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|1); uint8_t data=soft_i2c_read(false); soft_i2c_stop(); return data; }
void eeprom_write_byte(uint16_t addr, uint8_t data) { soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write((uint8_t)(addr>>8)); soft_i2c_write((uint8_t)(addr&0xFF)); soft_i2c_write(data); soft_i2c_stop(); delay(5); }
void oled_cmd(uint8_t cmd) { soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x00); soft_i2c_write(cmd); soft_i2c_stop(); }
void oled_init() { oled_cmd(0xAE); oled_cmd(0x20); oled_cmd(0x02); oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); }
void oled_set_pos(uint8_t x, uint8_t page) { oled_cmd(0xB0+page); oled_cmd(0x00+(x&0x0F)); oled_cmd(0x10+((x>>4)&0x0F)); }

void draw_pixel(uint8_t x, uint8_t y) { if(x>=128||y>=64)return; if((y>>3)==current_page) oled_buffer[x]|=(1<<(y&7)); }
void clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++){ poll_serial(); for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)>>3)==current_page)oled_buffer[x+i]&=~(1<<((y+j)&7)); } }
void invert_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++){ poll_serial(); for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)>>3)==current_page)oled_buffer[x+i]^=(1<<((y+j)&7)); } }
void draw_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++){draw_pixel(x+i,y);draw_pixel(x+i,y+h-1); poll_serial();} for(uint8_t i=0;i<h;i++){draw_pixel(x,y+i);draw_pixel(x+w-1,y+i); poll_serial();} }
void draw_char(uint8_t x, uint8_t y, uint8_t c) {
    if(c < 32 || c > 126) return;
    uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + 7 < p_min) return;
    for(uint8_t i=0;i<5;i++){ poll_serial(); uint8_t line=font_5x7[c-32][i]; for(uint8_t j=0;j<7;j++)if(line&(1<<j))draw_pixel(x+i,y+j); }
}
void draw_string(uint8_t x, uint8_t y, const char* str) { while(*str){draw_char(x,y,(uint8_t)*str++);x+=6; poll_serial();} }
void draw_cursor(uint8_t x, uint8_t y) { invert_rect(x-3,y,7,1); invert_rect(x,y-3,1,3); invert_rect(x,y+1,1,3); }

uint8_t my_strlen(const char* str) { uint8_t len=0; while(str[len]) len++; return len; }

char hist_buf[15][16];
uint8_t hist_count = 0;
uint8_t hist_offset = 0;

void push_hist(const char* str) {
    if(hist_count < 15) {
        uint8_t j=0; while(str[j]) { hist_buf[hist_count][j] = str[j]; j++; } hist_buf[hist_count][j] = 0;
        hist_count++;
    } else {
        for(int i=0; i<14; i++) {
            uint8_t j=0; while(hist_buf[i+1][j]) { hist_buf[i][j] = hist_buf[i+1][j]; j++; } hist_buf[i][j] = 0;
        }
        uint8_t j=0; while(str[j]) { hist_buf[14][j] = str[j]; j++; } hist_buf[14][j] = 0;
    }
    hist_offset = 0;
}

void ltoa_simple(int32_t val, char* buf) {
    static const int32_t powers[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};
    if(val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    bool neg = false; if(val < 0) { neg = true; val = -val; }
    int b_idx = 0; bool leading = true;
    if(neg) buf[b_idx++] = '-';
    for(int i=0; i<10; i++) {
        char digit = '0';
        while(val >= powers[i]) { val -= powers[i]; digit++; }
        if(digit != '0' || !leading || i == 9) {
            buf[b_idx++] = digit; leading = false;
        }
    }
    buf[b_idx] = 0;
}

void format_val(int32_t val, char* buf, int8_t f_state) {
    bool neg = false;
    if (val < 0) { neg = true; val = -val; }
    int32_t i_part = val / 100;
    int32_t f_part = val % 100;
    char t_buf[12]; ltoa_simple(i_part, t_buf);
    int idx = 0;
    if (neg) buf[idx++] = '-';
    int i = 0; while (t_buf[i]) buf[idx++] = t_buf[i++];
    if (f_state == -2) {
        if (f_part > 0) {
            buf[idx++] = '.';
            buf[idx++] = '0' + (f_part / 10);
            if (f_part % 10 > 0) buf[idx++] = '0' + (f_part % 10);
        }
    } else if (f_state >= 0) {
        buf[idx++] = '.';
        if (f_state >= 1) buf[idx++] = '0' + (f_part / 10);
        if (f_state == 2) buf[idx++] = '0' + (f_part % 10);
    }
    buf[idx] = 0;
}

int32_t parse_fixed_decimal(const char* str) {
    bool neg = false;
    int32_t val = 0;
    if (*str == '-') { neg = true; str++; }
    while (*str >= '0' && *str <= '9') { val = val * 10 + (*str - '0'); str++; }
    val *= 100;
    if (*str == '.') {
        str++;
        if (*str >= '0' && *str <= '9') {
            val += (*str - '0') * 10;
            str++;
            if (*str >= '0' && *str <= '9') val += (*str - '0');
        }
    }
    return neg ? -val : val;
}

int32_t hist_val(const char* str) {
    if ((str[0] >= '0' && str[0] <= '9') || (str[0] == '-' && (str[1] >= '0' && str[1] <= '9')))
        return parse_fixed_decimal(str);
    return parse_fixed_decimal(str + 1);
}

void do_calculation() {
    if (calc_error) return;
    if(calc_op == '+') {
        if ((calc_prev > 0 && calc_val > 2000000000L - calc_prev) || (calc_prev < 0 && calc_val < -2000000000L - calc_prev)) {
            calc_error = true; return;
        }
        calc_val = calc_prev + calc_val;
    }
    else if(calc_op == '-') calc_val = calc_prev - calc_val;
    else if(calc_op == '*') {
        bool neg = (calc_prev < 0) ^ (calc_val < 0);
        uint32_t a = calc_prev < 0 ? -calc_prev : calc_prev;
        uint32_t b = calc_val < 0 ? -calc_val : calc_val;
        uint32_t a_i = a / 100, a_f = a % 100;
        uint32_t b_i = b / 100, b_f = b % 100;
        if (a_i > 40000 || b_i > 40000) { calc_error = true; return; }
        uint32_t res = a_i * b_i * 100 + a_i * b_f + b_i * a_f + (a_f * b_f) / 100;
        if (res > 2000000000UL) { calc_error = true; return; }
        calc_val = neg ? -(int32_t)res : (int32_t)res;
    }
    else if(calc_op == '/') {
        if(calc_val == 0) { calc_error = true; return; }
        bool neg = (calc_prev < 0) ^ (calc_val < 0);
        uint32_t a = calc_prev < 0 ? -calc_prev : calc_prev;
        uint32_t b = calc_val < 0 ? -calc_val : calc_val;
        uint32_t res_i = a / b;
        uint32_t rem = a % b;
        uint32_t res = res_i * 100 + (rem * 100) / b;
        calc_val = neg ? -(int32_t)res : (int32_t)res;
    }
}

uint8_t hex2byte(char h1, char h2) {
    uint8_t val = 0;
    if(h1 >= '0' && h1 <= '9') val += (h1 - '0') << 4; else if(h1 >= 'A' && h1 <= 'F') val += (h1 - 'A' + 10) << 4; else if(h1 >= 'a' && h1 <= 'f') val += (h1 - 'a' + 10) << 4;
    if(h2 >= '0' && h2 <= '9') val += (h2 - '0'); else if(h2 >= 'A' && h2 <= 'F') val += (h2 - 'A' + 10); else if(h2 >= 'a' && h2 <= 'f') val += (h2 - 'a' + 10);
    return val;
}

char s_read() {
    uint32_t t = millis();
    while(!(USART1->STATR & (1 << 5))){ if(millis()-t > 20) return 0; }
    return USART1->DATAR;
}

uint8_t hex2byte_s() { return hex2byte(s_read(), s_read()); }

inline void poll_serial(void) {
    while (USART1->STATR & (1 << 5)) {
        char c = USART1->DATAR;
        if (c == 'M') { rx_state = 1; rx_val = 0; rx_neg = false; }
        else if (c == '-') { if (rx_state >= 1 && rx_state <= 4) rx_neg = true; }
        else if (c >= '0' && c <= '9') { if (rx_state >= 1 && rx_state <= 4) rx_val = (rx_val * 10) + (c - '0'); }
        else if (c == ',') {
            if (rx_state == 1) { rx_state = 2; rx_val = 0; rx_neg = false; }
            else if (rx_state == 2) { rx_dx = rx_neg ? -rx_val : rx_val; rx_state = 3; rx_val = 0; rx_neg = false; }
            else if (rx_state == 3) { rx_dy = rx_neg ? -rx_val : rx_val; rx_state = 4; rx_val = 0; rx_neg = false; }
            else rx_state = 0;
        }
        else if (c == '\n' || c == '\r') {
            if (rx_state == 4) {
                int16_t final_val = rx_neg ? -rx_val : rx_val;
                int16_t nx = (int16_t)mouse_x + rx_dx;
                int16_t ny = (int16_t)mouse_y + rx_dy;
                if (nx < 0) nx = 0; else if (nx > 127) nx = 127;
                if (ny < 0) ny = 0; else if (ny > 63) ny = 63;
                mouse_x = (uint8_t)nx; mouse_y = (uint8_t)ny;
                bool current_btn = (final_val > 0);
                if (current_btn && !mouse_click_buf) {
                    mouse_click_latch = true;
                    latch_click_x = mouse_x;
                    latch_click_y = mouse_y;
                }
                mouse_click_buf = current_btn;
            }
            rx_state = 0; rx_val = 0; rx_neg = false;
        }
        else if (c == 'R' || c == 'r') {
            Serial.println("--- DICT ---");
            for(int dev=0; dev<31; dev++) {
                uint16_t base_addr = DEV_MEM_START + (dev * DEV_MEM_SIZE);
                uint8_t id = eeprom_read_byte(base_addr + DEV_ID_OFS);
                if(id == 0xFF || id == 0x00) continue;
                Serial.print("S"); if(dev < 10) Serial.print("0"); Serial.print(dev);
                Serial.print(" ["); if(id < 0x10) Serial.print("0"); Serial.print(id, HEX); Serial.print("] ");
                for(int i=0; i<16; i++) {
                    char nc = (char)eeprom_read_byte(base_addr + DEV_LBL_OFS + i);
                    if(nc >= 32 && nc <= 126) Serial.print(nc); else Serial.print(' ');
                }
                Serial.println();
            }
            Serial.println("--- END ---");
            rx_state = 0;
        }
        else if (c == 'W' || c == 'w') {
            if (s_read() == ',') {
                int slot = (s_read() - '0')*10 + (s_read() - '0');
                if (s_read() == ',' && slot >= 0 && slot <= 30) {
                    uint8_t id = hex2byte_s();
                    if (s_read() == ',') {
                        char name[16]; for(int i=0; i<16; i++) name[i] = ' ';
                        int n_idx = 0;
                        while(true) {
                            char nc = s_read(); if(nc == 0 || nc == '\r' || nc == '\n') break;
                            if(n_idx < 16) name[n_idx++] = nc;
                        }
                        uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE);
                        eeprom_write_byte(base + DEV_ID_OFS, id);
                        for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, name[i]);
                        Serial.print("Save S"); Serial.println(slot);
                    }
                }
            }
            rx_state = 0;
        }
        else if (c == 'S' || c == 's') {
            if (s_read() == ',') {
                int slot = (s_read() - '0')*10 + (s_read() - '0');
                if (s_read() == ',' && slot >= 0 && slot <= 30) {
                    int pat = (s_read() - '0')*10 + (s_read() - '0');
                    if (s_read() == ',' && pat >= 0 && pat <= 15) {
                        char name[8]; for(int i=0; i<8; i++) name[i] = ' ';
                        int n_idx = 0; bool ok = false;
                        while(true) {
                            char nc = s_read(); if(nc == 0 || nc == '\r' || nc == '\n') break;
                            if(nc == ',') { ok = true; break; }
                            if(n_idx < 8) name[n_idx++] = nc;
                        }
                        if (ok) {
                            uint8_t count = hex2byte_s(); uint8_t payload[22];
                            if (s_read() == ',') {
                                for(int i=0; i<count*2 && i<22; i++) {
                                    payload[i] = hex2byte_s();
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
            rx_state = 0;
        }
        else if (c == 'F' || c == 'f') {
            if (s_read() == ',') {
                if (s_read() == '1' && s_read() == '2' && s_read() == '3' && s_read() == '4') {
                    Serial.println("--- FMT ---");
                    for (int slot=3; slot<=30; slot++) {
                        uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE);
                        eeprom_write_byte(base + DEV_ID_OFS, 0xFF);
                        for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, 0xFF);
                    }
                    Serial.println("--- OK ---");
                }
            }
            rx_state = 0;
        }
        else { rx_state = 0; rx_val = 0; rx_neg = false; }
    }
}

void handle_input(uint8_t frame_x, uint8_t frame_y, bool &clicked, uint8_t cx, uint8_t cy) {
    static uint32_t last_handled_time = 0;
    const uint32_t CLICK_REARM_MS = 60;

    if (menu_state == 2 && frame_x >= 58 && popup_state == 0) {
        static uint32_t scroll_tick = 0;
        if (millis() - scroll_tick > 300) {
            if (frame_y <= 14 && hist_offset < hist_count) { hist_offset++; scroll_tick = millis(); }
            else if (frame_y >= 62 && hist_offset > 0) { hist_offset--; scroll_tick = millis(); }
        }
    }

    if (menu_state != 2) {
        if (clicked && cy < 11) {
            if (cx < 42) menu_state = 0;
            else if (cx < 85) menu_state = 1;
            else menu_state = 2;
            clicked = false; mouse_click_buf = false; last_handled_time = millis();
        }
    }
    else {
        if (popup_state == 0) {
            if (clicked && cy < 11) {
                if (cx < 24) base_mode = (base_mode == 2) ? 0 : base_mode + 1;
                else popup_state = 1;
                clicked = false; mouse_click_buf = false; last_handled_time = millis();
            }
        } else {
            if (clicked) {
                if (cx < 44 && cy < 16) { menu_state = 0; popup_state = 0; }
                else popup_state = 0;
                clicked = false; mouse_click_buf = false; last_handled_time = millis();
            }
        }

        if (popup_state == 0 && clicked && (millis() - last_handled_time > CLICK_REARM_MS)) {
            static const char* const btns[20] = {
                "C","BS"," ","/","7","8","9","*","4","5","6","-","1","2","3","+","0","."," ","="
            };
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 4; c++) {
                    char btn = btns[r*4 + c][0];
                    if (btn == ' ') continue;
                    uint8_t width_cells = 1;
                    int draw_c = c;
                    if (btn == 'B') width_cells = 2;
                    else if (btn == '=') { width_cells = 2; draw_c = c - 1; }

                    if (cx > draw_c*14 && cx < (draw_c + width_cells)*14 &&
                        cy > 13 + r*10 && cy < 13 + (r+1)*10) {
                        clicked = false; mouse_click_buf = false; last_handled_time = millis();

                        if (calc_error) {
                            if (btn == 'C') {
                                calc_val = 0; calc_prev = 0; calc_op = 0;
                                calc_new = true; is_result = false;
                                input_frac_digits = -1; hist_count = 0; hist_offset = 0;
                                calc_error = false;
                            }
                            return;
                        }

                        if (btn >= '0' && btn <= '9') {
                            if (is_result) {
                                char dBuf[16]; dBuf[0] = '='; format_val(calc_val, dBuf+1, -2);
                                push_hist(dBuf);
                                is_result = false; calc_val = 0; calc_new = true; input_frac_digits = -1;
                            }
                            if (calc_new) {
                                calc_val = (btn - '0') * 100; calc_new = false; input_frac_digits = -1;
                            } else {
                                if (calc_val > 99999999L || calc_val < -99999999L) calc_error = true;
                                else {
                                    if (input_frac_digits == -1) {
                                        if (calc_val >= 0) calc_val = calc_val * 10 + (btn - '0') * 100;
                                        else calc_val = calc_val * 10 - (btn - '0') * 100;
                                    } else if (input_frac_digits == 0) {
                                        if (calc_val >= 0) calc_val += (btn - '0') * 10;
                                        else calc_val -= (btn - '0') * 10;
                                        input_frac_digits = 1;
                                    } else if (input_frac_digits == 1) {
                                        if (calc_val >= 0) calc_val += (btn - '0');
                                        else calc_val -= (btn - '0');
                                        input_frac_digits = 2;
                                    }
                                }
                            }
                        }
                        else if (btn == '.') {
                            if (is_result) {
                                char dBuf[16]; dBuf[0] = '='; format_val(calc_val, dBuf+1, -2);
                                push_hist(dBuf);
                                is_result = false; calc_val = 0; calc_new = false; input_frac_digits = 0;
                            } else if (calc_new) {
                                calc_val = 0; calc_new = false; input_frac_digits = 0;
                            } else if (input_frac_digits == -1) input_frac_digits = 0;
                        }
                        else if (btn == 'C') {
                            calc_val = 0; calc_prev = 0; calc_op = 0;
                            calc_new = true; is_result = false;
                            input_frac_digits = -1; hist_count = 0; hist_offset = 0;
                            calc_error = false;
                        }
                        else if (btn == 'B') {
                            if (!is_result && !calc_new) {
                                if (input_frac_digits == 2) { calc_val = (calc_val / 10) * 10; input_frac_digits = 1; }
                                else if (input_frac_digits == 1) { calc_val = (calc_val / 100) * 100; input_frac_digits = 0; }
                                else if (input_frac_digits == 0) input_frac_digits = -1;
                                else { calc_val = (calc_val / 1000) * 100; if (calc_val == 0) calc_new = true; }
                            }
                        }
                        else if (btn == '=') {
                            if (calc_op && !is_result) {
                                char dBuf[16];
                                if (calc_op && calc_new) { dBuf[0] = calc_op; dBuf[1] = 0; }
                                else if (calc_op && !calc_new) { dBuf[0] = calc_op; format_val(calc_val, dBuf+1, input_frac_digits); }
                                else format_val(calc_val, dBuf, input_frac_digits);
                                push_hist(dBuf);
                                do_calculation();
                                calc_op = 0; calc_new = true; is_result = true; input_frac_digits = -1;
                            }
                        }
                        else {
                            char dBuf[16];
                            if (is_result) { dBuf[0] = '='; format_val(calc_val, dBuf+1, -2); }
                            else if (calc_op && calc_new) { dBuf[0] = calc_op; dBuf[1] = 0; }
                            else if (calc_op && !calc_new) { dBuf[0] = calc_op; format_val(calc_val, dBuf+1, input_frac_digits); }
                            else format_val(calc_val, dBuf, input_frac_digits);

                            if (is_result) {
                                push_hist(dBuf);
                                calc_prev = calc_val; calc_op = btn; calc_new = true;
                                is_result = false; input_frac_digits = -1;
                            } else {
                                if (calc_op && !calc_new) {
                                    push_hist(dBuf); do_calculation();
                                    calc_prev = calc_val; calc_op = btn; calc_new = true; input_frac_digits = -1;
                                } else if (!calc_op) {
                                    push_hist(dBuf);
                                    calc_prev = calc_val; calc_op = btn; calc_new = true; input_frac_digits = -1;
                                } else calc_op = btn;
                            }
                        }
                    }
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    RCC->APB2PCENR |= (1 << 14) | (1 << 5) | (1 << 4) | (1 << 0);
    GPIOC->CFGLR &= ~((0xFF<<4)|(0xF<<16)|(0xFF<<24)); GPIOC->CFGLR |= ((0x55<<4)|(0x8<<16)|(0x55<<24)); GPIOC->BSHR = (1<<4);
    GPIOD->CFGLR &= ~((0xF << (5 * 4)) | (0xF << (6 * 4))); GPIOD->CFGLR |= ((0xB << (5 * 4)) | (0x4 << (6 * 4)));
    USART1->BRR = 0x1A1; USART1->CTLR1 = 0x200C;
    oled_init();
    for(int i=0; i<15; i++) hist_buf[i][0] = 0;
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write(0); soft_i2c_write(0); soft_i2c_write(0xAA); soft_i2c_stop(); delay(5);
    if(eeprom_read_byte(0)==0xAA) eeprom_ok=true;
    pinMode(PC0, OUTPUT); pinMode(PC3, INPUT_PULLUP); pinMode(PC5, OUTPUT); pinMode(PD2, OUTPUT);
}

void loop() {
    static bool last_pc3 = true;
    bool cur_pc3 = (GPIOC->INDR & (1<<3));
    bool pc3_clicked = (last_pc3 && !cur_pc3);
    last_pc3 = cur_pc3;
    static bool pc_link_mode = false;

    if (pc3_clicked) {
        pc_link_mode = !pc_link_mode;
        if (pc_link_mode) {
            for (current_page = 0; current_page < 8; current_page++) {
                poll_serial();
                for(int i=0; i<128; i++) oled_buffer[i] = 0;
                draw_window(0,0,128,64); draw_string(40, 24, "PC LINK"); draw_string(34, 40, "PC3 EXIT");
                oled_set_pos(0,current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40);
                for(int x=0;x<128;x++) { soft_i2c_write(oled_buffer[x]); poll_serial(); }
                soft_i2c_stop();
            }
        }
    }

    if (pc_link_mode) { poll_serial(); return; }

    poll_serial();

    uint8_t frame_x = mouse_x;
    uint8_t frame_y = mouse_y;
    bool clicked = mouse_click_latch;
    uint8_t cx = latch_click_x;
    uint8_t cy = latch_click_y;

    handle_input(frame_x, frame_y, clicked, cx, cy);
    mouse_click_latch = false;

    for (current_page = 0; current_page < 8; current_page++) {
        poll_serial();
        for(int i=0; i<128; i++) oled_buffer[i] = 0;
        draw_window(0,0,128,64);

        if (menu_state != 2) {
            for(int i=0;i<128;i++) { draw_pixel(i,11); if(i%16==0) poll_serial(); }
            draw_string(12,3,"SYS"); draw_string(55,3,"FIL1"); draw_string(98,3,"APP");

            if(frame_y<11){
                if(frame_x<42) invert_rect(11,2,21,9);
                else if(frame_x<85) invert_rect(54,2,24,9);
                else if(frame_x<128) invert_rect(97,2,21,9);
            }

            if(menu_state == 0) {
                invert_rect(11,2,21,9);
                draw_string(46,20,"UIAP");
                draw_string(34,35,"GemOS");
                draw_string(34,50,"V0.32c");
            }
            else if(menu_state == 1) {
                invert_rect(54,2,24,9);
                draw_string(40,32,"FIL1");
            }
        }
        else if (menu_state == 2) {
            for(int y=12; y<64; y++) { draw_pixel(57, y); poll_serial(); }
            for(int i=0; i<=5; i++) { poll_serial(); for(int j=0; j<=56; j++) draw_pixel(j, 13 + i*10); }
            for(int i=0; i<=4; i++) { poll_serial(); for(int j=13; j<=63; j++) { if(i==2 && j>=13 && j<=23) continue; if(i==3 && j>=53 && j<=63) continue; draw_pixel(i*14, j); } }
            for(int i=0; i<128; i++) { draw_pixel(i,11); if(i%16==0) poll_serial(); }

            char topBuf[18];
            if (calc_error) {
                topBuf[0]='E'; topBuf[1]='r'; topBuf[2]='r'; topBuf[3]=0;
            } else {
                int total_lines_top = hist_count + 1;
                int bottom_line_idx = total_lines_top - 1 - hist_offset;
                int32_t top_src_val = (bottom_line_idx >= 0 && bottom_line_idx < hist_count) ? hist_val(hist_buf[bottom_line_idx]) : calc_val;

                if (base_mode == 0) {
                    int8_t fs = (bottom_line_idx >= 0 && bottom_line_idx < hist_count) ? -2 : (is_result ? -2 : input_frac_digits);
                    format_val(top_src_val, topBuf, fs);
                } else {
                    int32_t int_val_for_base = top_src_val / 100;
                    uint32_t v = (uint32_t)(int_val_for_base < 0 ? -int_val_for_base : int_val_for_base);
                    if(base_mode == 2) v &= 0xFFFF;
                    if(v == 0) { topBuf[0]='0'; topBuf[1]=0; }
                    else {
                        int idx=0; char temp[18];
                        while(v > 0) {
                            uint8_t rem = (base_mode == 1) ? (v & 0xF) : (v & 1);
                            temp[idx++] = rem < 10 ? '0'+rem : 'A'+rem-10;
                            v >>= (base_mode == 1) ? 4 : 1;
                        }
                        int i=0; if(int_val_for_base < 0) topBuf[i++] = '-'; while(idx>0) topBuf[i++] = temp[--idx]; topBuf[i]=0;
                    }
                }
            }

            if(popup_state == 0) {
                if(base_mode == 0) draw_string(2, 3, "DEC"); else if(base_mode == 1) draw_string(2, 3, "HEX"); else draw_string(2, 3, "BIN");
                uint8_t top_len = my_strlen(topBuf); draw_string(126 - (top_len * 6), 3, topBuf);
                if(frame_y < 11 && frame_x < 24) invert_rect(0, 2, 22, 9);
            } else {
                clear_rect(0, 0, 44, 16); draw_window(0, 0, 44, 16); draw_string(13, 5, "ESC");
                if(frame_x < 44 && frame_y < 16) invert_rect(11, 4, 21, 9);
            }

            char dBuf[16];
            if (calc_error) {
                dBuf[0]='E'; dBuf[1]='r'; dBuf[2]='r'; dBuf[3]=0;
            } else if (is_result) {
                dBuf[0] = '='; format_val(calc_val, dBuf+1, -2);
            } else if (calc_op && calc_new) {
                dBuf[0] = calc_op; dBuf[1] = 0;
            } else if (calc_op && !calc_new) {
                dBuf[0] = calc_op; format_val(calc_val, dBuf+1, input_frac_digits);
            } else {
                format_val(calc_val, dBuf, input_frac_digits);
            }

            for(int i=0; i<5; i++) {
                int line_idx = hist_count + 1 - 5 - hist_offset + i;
                if(line_idx >= 0) {
                    char* line_str = (line_idx < hist_count) ? hist_buf[line_idx] : dBuf;
                    draw_string(125 - (my_strlen(line_str) * 6), 14 + i*10, line_str);
                }
            }

            static const char* const btns[20] = {"C","BS"," ","/","7","8","9","*","4","5","6","-","1","2","3","+","0","."," ","="};
            for(int r=0; r<5; r++) {
                for(int c=0; c<4; c++) {
                    char btn = btns[r*4 + c][0];
                    if(btn == ' ') continue;
                    uint8_t width_cells = 1; int draw_c = c;
                    if(btn == 'B') width_cells = 2;
                    else if(btn == '=') { width_cells = 2; draw_c = c - 1; }
                    int bx = (width_cells == 2) ? draw_c*14 + ((width_cells*14) - (my_strlen(btns[r*4 + c]) * 6 - 1)) / 2 : draw_c*14 + 5;
                    draw_string(bx, 13 + r*10 + 2, btns[r*4 + c]);

                    if(popup_state == 0 && frame_x > draw_c*14 && frame_x < (draw_c+width_cells)*14 && frame_y > 13 + r*10 && frame_y < 13 + (r+1)*10) {
                        invert_rect(draw_c*14 + 1, 13 + r*10 + 1, width_cells*14 - 1, 9);
                    }
                }
            }
        }
        draw_cursor(frame_x, frame_y);
        oled_set_pos(0, current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40);
        for(int x=0;x<128;x++) { soft_i2c_write(oled_buffer[x]); poll_serial(); }
        soft_i2c_stop();
        poll_serial();
    }
}
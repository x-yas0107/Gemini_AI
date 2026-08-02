/* UIAPduino_GemOS V1.25 (APP Ver0.16 - Ultra-Light Fixed Decimal) */
/* * Change History:
 * Ver0.16 2026,08,02 : 16KBの容量超過を解消するため、重複していた計算ロジックを do_calculation() 
 *                       関数に統合。また各文字配列を static const 化してフラッシュメモリを劇的削減。
 * Ver0.15 2026,08,02 : 小数点以下2桁固定の計算ロジック(Fixed-Point)を実装。int64_tを完全排除し、
 *                       整数部・小数部の分割筆算アルゴリズムで16KB以内での実装に挑戦(容量超過)。
 * Ver0.14 2026,08,02 : バグ修正。最初の数値を演算子キーで確定する際に記号なしで履歴保存される
 *                       ケースがあり、右上変換の読み取り処理が先頭の数字を誤って読み飛ばしていた
 *                       (例: 10→0)。hist_val()関数を追加し、先頭が数字ならそのまま読む形に修正。
 * Ver0.13 2026,08,02 : 最下段の配置を「[空白] 0 [空白] =」→「0 . [空白] =」に変更。0を左端へ、
 *                       元0の位置に「.」キーを新規配置(表示のみ、押下時の動作は未実装でガード済み)。
 * Ver0.12 2026,08,02 : Bキーの表示ラベルを「B」→「BS」の2文字表記に変更。2マスボタンの文字位置を
 *                       ラベル文字数から自動計算してセンタリングするよう改善。
 * Ver0.11 2026,08,02 : Bキー・=キーを隣の空きマスと合わせて横2マス分のボタンに拡張(当たり判定・
 *                       ハイライト・文字位置とも2マス幅でセンタリング)。境界線も該当部分を非表示化。
 * Ver0.10 2026,08,02 : スクロール判定を枠の上下端ギリギリでのみ発火するよう変更(y<=14/>=62)。
 *                       スクロール速度を約1/2に減速(判定間隔150ms→300ms)。
 * Ver0.09 2026,08,02 : 履歴スクロールの最大量を拡張。1行目(最古の履歴)を画面最下段まで
 *                       送り込めるように max_offset を「総行数-5」から「hist_count」に変更。
 * Ver0.08 2026,08,02 : 右上の基数変換表示を、常に「右下(フォーカス行)の値」を変換して出すように変更。
 *                       通常時はcalc_val、履歴スクロール中はその行の数値部分(符号記号を除く)を変換対象にする。
 *
 * I/O Map:
 * PA1 (ADC_CH1) : Joystick X-Axis
 * PA2 (ADC_CH0) : Joystick Y-Axis
 * PC0           : General Output
 * PC3           : PC LINK MODE Switch (Active Low)
 * PC4           : Joystick Push Button (Active Low)
 * PC5           : General Output
 * PC6           : Soft I2C SDA (OLED/EEPROM)
 * PC7           : Soft I2C SCL (OLED/EEPROM)
 * PD2           : General Output
 * TX/RX         : UART1 (115200 bps)
 */
#include <Arduino.h>

#define SOFT_SDA 6
#define SOFT_SCL 7
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
uint16_t adc_offset_x = 512, adc_offset_y = 512;
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
void clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++)for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)>>3)==current_page)oled_buffer[x+i]&=~(1<<((y+j)&7)); }
void invert_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++)for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)>>3)==current_page)oled_buffer[x+i]^=(1<<((y+j)&7)); }
void draw_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++){draw_pixel(x+i,y);draw_pixel(x+i,y+h-1);} for(uint8_t i=0;i<h;i++){draw_pixel(x,y+i);draw_pixel(x+w-1,y+i);} }
void draw_char(uint8_t x, uint8_t y, uint8_t c) { 
    if(c < 32 || c > 126) return;
    uint8_t p_min = current_page << 3; if(y > p_min + 7 || y + 7 < p_min) return; 
    for(uint8_t i=0;i<5;i++){ uint8_t line=font_5x7[c-32][i]; for(uint8_t j=0;j<7;j++)if(line&(1<<j))draw_pixel(x+i,y+j); } 
}
void draw_string(uint8_t x, uint8_t y, const char* str) { while(*str){draw_char(x,y,(uint8_t)*str++);x+=6;} }
void draw_cursor(uint8_t x, uint8_t y) { invert_rect(x-3,y,7,1); invert_rect(x,y-3,1,3); invert_rect(x,y+1,1,3); }

uint8_t my_strlen(const char* str) { uint8_t len=0; while(str[len]) len++; return len; }

char hist_buf[30][16];
uint8_t hist_count = 0;
uint8_t hist_offset = 0;

void push_hist(const char* str) {
    if(hist_count < 30) {
        uint8_t j=0; while(str[j]) { hist_buf[hist_count][j] = str[j]; j++; } hist_buf[hist_count][j] = 0;
        hist_count++;
    } else {
        for(int i=0; i<29; i++) {
            uint8_t j=0; while(hist_buf[i+1][j]) { hist_buf[i][j] = hist_buf[i+1][j]; j++; } hist_buf[i][j] = 0;
        }
        uint8_t j=0; while(str[j]) { hist_buf[29][j] = str[j]; j++; } hist_buf[29][j] = 0;
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
            if (*str >= '0' && *str <= '9') { val += (*str - '0'); }
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
    if(calc_op == '+') calc_val = calc_prev + calc_val;
    else if(calc_op == '-') calc_val = calc_prev - calc_val;
    else if(calc_op == '*') {
        bool neg = (calc_prev < 0) ^ (calc_val < 0);
        uint32_t a = calc_prev < 0 ? -calc_prev : calc_prev;
        uint32_t b = calc_val < 0 ? -calc_val : calc_val;
        uint32_t a_i = a / 100, a_f = a % 100;
        uint32_t b_i = b / 100, b_f = b % 100;
        uint32_t res = a_i * b_i * 100 + a_i * b_f + b_i * a_f + (a_f * b_f) / 100;
        calc_val = neg ? -(int32_t)res : (int32_t)res;
    }
    else if(calc_op == '/') {
        if(calc_val != 0) {
            bool neg = (calc_prev < 0) ^ (calc_val < 0);
            uint32_t a = calc_prev < 0 ? -calc_prev : calc_prev;
            uint32_t b = calc_val < 0 ? -calc_val : calc_val;
            uint32_t res_i = a / b;
            uint32_t rem = a % b;
            uint32_t res = res_i * 100 + (rem * 100) / b;
            calc_val = neg ? -(int32_t)res : (int32_t)res;
        }
    }
}

uint8_t hex2byte(char h1, char h2) {
    uint8_t val = 0;
    if(h1 >= '0' && h1 <= '9') val += (h1 - '0') << 4; else if(h1 >= 'A' && h1 <= 'F') val += (h1 - 'A' + 10) << 4; else if(h1 >= 'a' && h1 <= 'f') val += (h1 - 'a' + 10) << 4;
    if(h2 >= '0' && h2 <= '9') val += (h2 - '0'); else if(h2 >= 'A' && h2 <= 'F') val += (h2 - 'A' + 10); else if(h2 >= 'a' && h2 <= 'f') val += (h2 - 'a' + 10);
    return val;
}

char s_read() { 
    uint32_t t=millis(); 
    while(!(USART1->STATR & (1 << 5))){ if(millis()-t>50) return 0; } 
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
                            char c = s_read(); if(c == 0 || c == '\r' || c == '\n') break;
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
                        int n_idx = 0; bool ok = false;
                        while(true) {
                            char c = s_read(); if(c == 0 || c == '\r' || c == '\n') break;
                            if(c == ',') { ok = true; break; }
                            if(n_idx < 8) name[n_idx++] = c;
                        }
                        if(ok) {
                            uint8_t count = hex2byte(s_read(), s_read()); uint8_t payload[22];
                            if(s_read() == ',') {
                                for(int i=0; i<count*2 && i<22; i++) { payload[i] = hex2byte(s_read(), s_read()); if(i < count*2 - 1) s_read(); }
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
                        uint16_t base = DEV_MEM_START + (slot * DEV_MEM_SIZE); eeprom_write_byte(base + DEV_ID_OFS, 0xFF);
                        for(int i=0; i<16; i++) eeprom_write_byte(base + DEV_LBL_OFS + i, 0xFF);
                    }
                    Serial.println("--- OK ---");
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
    Serial.println("System: Ver 1.25 OS Base");
    Serial.println("Serial OK");
    RCC->APB2PCENR|=RCC_AFIOEN|RCC_IOPAEN|RCC_IOPCEN|RCC_IOPDEN|RCC_ADC1EN;
    GPIOC->CFGLR&=~((0xFF<<4)|(0xF<<16)|(0xFF<<24)); GPIOC->CFGLR|=((0x55<<4)|(0x8<<16)|(0x55<<24)); GPIOC->BSHR=(1<<4);
    ADC1->CTLR2|=(1<<20)|(7<<17)|ADC_ADON; oled_init();
    adc_offset_x=adc_read(1); adc_offset_y=adc_read(0);
    for(int i=0; i<30; i++) hist_buf[i][0] = 0;
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write(0); soft_i2c_write(0); soft_i2c_write(0xAA); soft_i2c_stop(); delay(5);
    if(eeprom_read_byte(0)==0xAA) { eeprom_ok=true; }
    pinMode(PC0, OUTPUT); pinMode(PC3, INPUT_PULLUP); pinMode(PC5, OUTPUT); pinMode(PD2, OUTPUT);
    Serial.println("Ready!");
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
            Serial.println("--- PC LINK ON ---");
            ADC1->CTLR2 &= ~ADC_ADON; delay(5);
            for (current_page = 0; current_page < 8; current_page++) {
                for(int i=0; i<128; i++) oled_buffer[i] = 0;
                draw_window(0,0,128,64); draw_string(43, 24, "PC LINK"); draw_string(37, 40, "PC3: EXIT");
                oled_set_pos(0,current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40); for(int x=0;x<128;x++) soft_i2c_write(oled_buffer[x]); soft_i2c_stop();
            }
        } else {
            ADC1->CTLR2 |= ADC_ADON; delay(5);
            Serial.println("--- PC LINK OFF ---");
        }
    }

    if (pc_link_mode) { check_serial(); return; }

    check_serial();
    uint16_t x_raw=adc_read(1), y_raw=adc_read(0); static bool last_sw=true; bool cur_sw=(GPIOC->INDR&(1<<4)); bool clicked=(last_sw&&!cur_sw); last_sw=cur_sw;
    int16_t dx=0,dy=0, diff_x=(int16_t)x_raw-(int16_t)adc_offset_x, diff_y=(int16_t)y_raw-(int16_t)adc_offset_y;
    if(diff_x>DEADZONE)dx=diff_x/128+1; else if(diff_x<-DEADZONE)dx=diff_x/128-1;
    if(diff_y>DEADZONE)dy=diff_y/128+1; else if(diff_y<-DEADZONE)dy=diff_y/128-1;
    mouse_x = (uint8_t)constrain(mouse_x + dx, 0, 127); mouse_y = (uint8_t)constrain(mouse_y + dy, 0, 63);

    if (menu_state == 2 && mouse_x >= 58 && popup_state == 0) {
        static uint32_t scroll_tick = 0;
        int max_offset = hist_count;
        if (millis() - scroll_tick > 300) {
            if (mouse_y <= 14 && hist_offset < max_offset) {
                hist_offset++;
                scroll_tick = millis();
            } else if (mouse_y >= 62 && hist_offset > 0) {
                hist_offset--;
                scroll_tick = millis();
            }
        }
    }

    for (current_page = 0; current_page < 8; current_page++) {
        for(int i=0; i<128; i++) oled_buffer[i] = 0;
        draw_window(0,0,128,64);
        
        if (menu_state != 2) {
            for(int i=0;i<128;i++) draw_pixel(i,11);
            draw_string(12,3,"SYS"); draw_string(55,3,"FIL1"); draw_string(98,3,"APP");
            
            if(mouse_y<11){ 
                if(mouse_x<42){invert_rect(11,2,21,9);if(clicked){menu_state=0; clicked=false;}} 
                else if(mouse_x<85){invert_rect(54,2,24,9);if(clicked){menu_state=1; clicked=false;}} 
                else if(mouse_x<128){invert_rect(97,2,21,9);if(clicked){menu_state=2; clicked=false;}} 
            }

            if(menu_state == 0) {
                invert_rect(11,2,21,9); 
                draw_string(37,20,"UIAPduino");
                draw_string(25,35,"GemOS Ver1.25");
                draw_string(31,50,"APP Ver0.16");
            }
            else if(menu_state == 1) {
                invert_rect(54,2,24,9);
                draw_string(40,32,"FIL1 MODE");
            }
        } 
        else if (menu_state == 2) {
            
            for(int y=12; y<64; y++) draw_pixel(57, y);
            for(int i=0; i<=5; i++) { for(int j=0; j<=56; j++) draw_pixel(j, 13 + i*10); }
            for(int i=0; i<=4; i++) { for(int j=13; j<=63; j++) { if(i==2 && j>=13 && j<=23) continue; if(i==3 && j>=53 && j<=63) continue; draw_pixel(i*14, j); } }
            for(int i=0; i<128; i++) draw_pixel(i,11);
            
            int total_lines_top = hist_count + 1;
            int max_offset_top = hist_count;
            if (hist_offset > max_offset_top) hist_offset = max_offset_top;
            int bottom_line_idx = total_lines_top - 1 - hist_offset;
            int32_t top_src_val = calc_val;
            if (bottom_line_idx >= 0 && bottom_line_idx < hist_count) {
                top_src_val = hist_val(hist_buf[bottom_line_idx]);
            }

            char topBuf[18];
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
                    int i=0; 
                    if(int_val_for_base < 0) topBuf[i++] = '-';
                    while(idx>0) topBuf[i++] = temp[--idx]; 
                    topBuf[i]=0;
                }
            }

            if(popup_state == 0) {
                if(base_mode == 0) draw_string(2, 3, "DEC");
                else if(base_mode == 1) draw_string(2, 3, "HEX");
                else draw_string(2, 3, "BIN");
                
                uint8_t top_len = my_strlen(topBuf);
                draw_string(126 - (top_len * 6), 3, topBuf);
                
                if(mouse_y < 11) {
                    if(mouse_x < 24) {
                        invert_rect(0, 2, 22, 9);
                        if(clicked) { base_mode = (base_mode == 2) ? 0 : base_mode + 1; clicked = false; }
                    } else if (clicked) {
                        popup_state = 1; clicked = false;
                    }
                }
            } else {
                clear_rect(0, 0, 44, 16);
                draw_window(0, 0, 44, 16);
                draw_string(13, 5, "ESC");
                if(mouse_x < 44 && mouse_y < 16) {
                    invert_rect(11, 4, 21, 9);
                    if(clicked) {
                        menu_state = 0; 
                        popup_state = 0;
                        clicked = false;
                    }
                } else if(clicked) {
                    popup_state = 0;
                    clicked = false;
                }
            }
            
            char dBuf[16];
            if (is_result) {
                dBuf[0] = '='; format_val(calc_val, dBuf+1, -2);
            } else if (calc_op && calc_new) {
                dBuf[0] = calc_op; dBuf[1] = 0;
            } else if (calc_op && !calc_new) {
                dBuf[0] = calc_op; format_val(calc_val, dBuf+1, input_frac_digits);
            } else {
                format_val(calc_val, dBuf, input_frac_digits);
            }

            int total_lines_render = hist_count + 1;
            int max_offset_render = hist_count;
            if(hist_offset > max_offset_render) hist_offset = max_offset_render;

            uint8_t r_margin = 125;
            for(int i=0; i<5; i++) {
                int line_idx = total_lines_render - 5 - hist_offset + i;
                if(line_idx < 0) continue;
                
                char* line_str = (line_idx < hist_count) ? hist_buf[line_idx] : dBuf;
                draw_string(r_margin - (my_strlen(line_str) * 6), 14 + i*10, line_str);
            }
            
            static const char* const btns[20] = {"C","BS"," ","/","7","8","9","*","4","5","6","-","1","2","3","+","0","."," ","="};
            for(int r=0; r<5; r++) {
                for(int c=0; c<4; c++) {
                    char btn = btns[r*4 + c][0];
                    if(btn == ' ') continue;
                    
                    uint8_t width_cells = 1;
                    int draw_c = c;
                    if(btn == 'B') { width_cells = 2; }
                    else if(btn == '=') { width_cells = 2; draw_c = c - 1; }
                    
                    int bx;
                    if (width_cells == 2) {
                        uint8_t label_len = my_strlen(btns[r*4 + c]);
                        int text_w = label_len * 6 - 1;
                        bx = draw_c*14 + ((width_cells*14) - text_w) / 2;
                    } else {
                        bx = draw_c*14 + 5;
                    }
                    int by = 13 + r*10 + 2;
                    draw_string(bx, by, btns[r*4 + c]);
                    
                    if(popup_state == 0 && mouse_x > draw_c*14 && mouse_x < (draw_c+width_cells)*14 && mouse_y > 13 + r*10 && mouse_y < 13 + (r+1)*10) {
                        invert_rect(draw_c*14 + 1, 13 + r*10 + 1, width_cells*14 - 1, 9);
                        if(clicked) {
                            clicked = false;
                            if(btn >= '0' && btn <= '9') {
                                if(is_result) {
                                    char tBuf[16];
                                    tBuf[0] = '='; format_val(calc_val, tBuf+1, -2);
                                    push_hist(tBuf);
                                    is_result = false; calc_val = 0; calc_new = true; input_frac_digits = -1;
                                }
                                if(calc_new) { 
                                    calc_val = (btn-'0') * 100; calc_new = false; input_frac_digits = -1;
                                } else { 
                                    if(input_frac_digits == -1) {
                                        if(calc_val >= 0) calc_val = calc_val * 10 + (btn-'0') * 100;
                                        else calc_val = calc_val * 10 - (btn-'0') * 100;
                                    } else if(input_frac_digits == 0) {
                                        if(calc_val >= 0) calc_val += (btn-'0') * 10;
                                        else calc_val -= (btn-'0') * 10;
                                        input_frac_digits = 1;
                                    } else if(input_frac_digits == 1) {
                                        if(calc_val >= 0) calc_val += (btn-'0');
                                        else calc_val -= (btn-'0');
                                        input_frac_digits = 2;
                                    }
                                }
                            } else if(btn == '.') {
                                if(is_result) { is_result = false; calc_val = 0; calc_new = false; input_frac_digits = 0; }
                                else if(calc_new) { calc_val = 0; calc_new = false; input_frac_digits = 0; }
                                else if(input_frac_digits == -1) { input_frac_digits = 0; }
                            } else if(btn == 'C') {
                                calc_val = 0; calc_prev = 0; calc_op = 0; calc_new = true; is_result = false;
                                input_frac_digits = -1; hist_count = 0; hist_offset = 0;
                            } else if(btn == 'B') {
                                if(!is_result && !calc_new) {
                                    if (input_frac_digits == 2) { calc_val = (calc_val / 10) * 10; input_frac_digits = 1; }
                                    else if (input_frac_digits == 1) { calc_val = (calc_val / 100) * 100; input_frac_digits = 0; }
                                    else if (input_frac_digits == 0) { input_frac_digits = -1; }
                                    else {
                                        calc_val = (calc_val / 1000) * 100;
                                        if(calc_val == 0) calc_new = true;
                                    }
                                }
                            } else if(btn == '=') {
                                if(calc_op && !is_result) {
                                    char tBuf[16];
                                    tBuf[0] = calc_op; format_val(calc_val, tBuf+1, input_frac_digits);
                                    push_hist(tBuf);
                                    do_calculation();
                                    calc_op = 0; calc_new = true; is_result = true; input_frac_digits = -1;
                                }
                            } else {
                                if(is_result) {
                                    char tBuf[16];
                                    tBuf[0] = '='; format_val(calc_val, tBuf+1, -2);
                                    push_hist(tBuf);
                                    calc_prev = calc_val; calc_op = btn; calc_new = true; is_result = false; input_frac_digits = -1;
                                } else {
                                    if (calc_op && !calc_new) {
                                        char tBuf[16];
                                        tBuf[0] = calc_op; format_val(calc_val, tBuf+1, input_frac_digits);
                                        push_hist(tBuf);
                                        do_calculation();
                                        calc_prev = calc_val; calc_op = btn; calc_new = true; input_frac_digits = -1;
                                    } else if (!calc_op) {
                                        char tBuf[16];
                                        format_val(calc_val, tBuf, input_frac_digits);
                                        push_hist(tBuf);
                                        calc_prev = calc_val; calc_op = btn; calc_new = true; input_frac_digits = -1;
                                    } else {
                                        calc_op = btn;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            if(popup_state == 0) {
                if(mouse_y < 8 && clicked) {
                    popup_state = 1;
                    clicked = false;
                }
            } else {
                clear_rect(0, 0, 44, 16);
                draw_window(0, 0, 44, 16);
                draw_string(13, 5, "ESC");
                if(mouse_x < 44 && mouse_y < 16) {
                    invert_rect(11, 4, 21, 9);
                    if(clicked) {
                        menu_state = 0; 
                        popup_state = 0;
                        clicked = false;
                    }
                } else if(clicked) {
                    popup_state = 0;
                    clicked = false;
                }
            }
        }

        draw_cursor(mouse_x,mouse_y);
        oled_set_pos(0,current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40); for(int x=0;x<128;x++) soft_i2c_write(oled_buffer[x]); soft_i2c_stop();
    }
}
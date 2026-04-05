/* UIAPduino_Handy_PLC V0.26 */
/* * Change History:
 * V0.26 - Fixed ESC not stopping PLC in MON mode. Widened coil symbol "()" by 1 pixel each side to prevent overlap with modifiers.
 */
#include <Arduino.h>

#define SOFT_SDA 6
#define SOFT_SCL 7
#define OLED_ADDR 0x78
#define EEPROM_ADDR 0x50
#define DEADZONE 50
#define MAX_ROWS 16

#define PACK(t,d,i,k) ((((t)&7)<<13) | (((d)&7)<<10) | (((i)&7)<<7) | ((k)&0x7F))
#define TYPE(c) (((c)>>13)&7)
#define DEV(c)  (((c)>>10)&7)
#define IDX(c)  (((c)>>7)&7)
#define KVAL(c) ((c)&0x7F)
#define EMPTY 0xFFFF

uint8_t mouse_x = 64, mouse_y = 32;
uint16_t adc_offset_x = 512, adc_offset_y = 512;
bool eeprom_ok = false;
uint8_t oled_buffer[128]; 
uint8_t current_page = 0; 
uint8_t font_cache[95][5];

uint8_t menu_state = 0;
uint16_t ladder_grid[MAX_ROWS][8];

bool popup_active = false;
uint8_t popup_step = 0;
uint8_t popup_target_x = 0, popup_target_y = 0;
uint8_t temp_type = 0, temp_dev = 0, temp_idx = 0, temp_k = 10;

uint8_t scroll_y = 0, mnm_scroll = 0;
bool plc_running = false;
uint8_t dev_X = 0, dev_Y = 0, dev_M = 0, dev_T_out = 0, dev_C_out = 0, c_coil_power_last = 0;
uint16_t dev_T_val[4] = {0}, dev_C_val[4] = {0};
uint32_t last_tick = 0;

const uint8_t ladder_icons[8][16] = {
    {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},
    {0xFF,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},
    {0x10,0x10,0x10,0x10,0x10,0x7C,0x00,0x00,0x00,0x00,0x7C,0x10,0x10,0x10,0x10,0x10},
    {0x10,0x10,0x10,0x10,0x10,0x7C,0x20,0x10,0x08,0x04,0x7C,0x10,0x10,0x10,0x10,0x10},
    {0x10,0x10,0x10,0x38,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x38,0x10,0x10,0x10},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x18,0x1C,0x1E,0x10},
    {0x10,0x78,0x38,0x18,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},
    {0x00,0x3E,0x08,0x08,0x08,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

const uint8_t stp_icon[8] = {0x00, 0x00, 0x3C, 0x42, 0x42, 0x3C, 0x00, 0x00};
const uint8_t run_icon[8] = {0x81, 0x42, 0x3C, 0x3C, 0x3C, 0x3C, 0x42, 0x81};

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

void draw_pixel(uint8_t x, uint8_t y) { if(x>=128||y>=64)return; if((y/8)==current_page) oled_buffer[x]|=(1<<(y%8)); }
void clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page * 8; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++)for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)/8)==current_page)oled_buffer[x+i]&=~(1<<((y+j)%8)); }
void invert_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page * 8; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++)for(uint8_t j=0;j<h;j++)if(x+i<128&&y+j<64)if(((y+j)/8)==current_page)oled_buffer[x+i]^=(1<<((y+j)%8)); }
void draw_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { uint8_t p_min = current_page * 8; if(y > p_min + 7 || y + h - 1 < p_min) return; for(uint8_t i=0;i<w;i++){draw_pixel(x+i,y);draw_pixel(x+i,y+h-1);} for(uint8_t i=0;i<h;i++){draw_pixel(x,y+i);draw_pixel(x+w-1,y+i);} }
void draw_char(uint8_t x, uint8_t y, char c) { if(c<32||c>126)return; uint8_t p_min = current_page * 8; if(y > p_min + 7 || y + 7 < p_min) return; for(uint8_t i=0;i<5;i++){ uint8_t line=font_cache[c-32][i]; for(uint8_t j=0;j<7;j++)if(line&(1<<j))draw_pixel(x+i,y+j); } }
void draw_string(uint8_t x, uint8_t y, const char* str) { while(*str){draw_char(x,y,*str++);x+=6;} }
void draw_bitmap(uint8_t x, uint8_t y, const uint8_t* bitmap, uint8_t w, uint8_t h) { for (uint8_t i = 0; i < h; i++) { uint8_t row = bitmap[i]; for (uint8_t j = 0; j < w; j++) { if (row & (1 << j)) draw_pixel(x + j, y + i); } } }
void draw_ladder_icon(uint8_t x, uint8_t y, uint8_t type) { if(type>7)return; uint8_t p_min = current_page * 8; if(y > p_min + 7 || y + 7 < p_min) return; for(uint8_t i=0;i<16;i++){ uint8_t col=ladder_icons[type][i]; for(uint8_t j=0;j<8;j++)if(col&(1<<j))draw_pixel(x+i,y+j); } }
void draw_cursor(uint8_t x, uint8_t y) { invert_rect(x-3,y,7,1); invert_rect(x,y-3,1,3); invert_rect(x,y+1,1,3); }

uint16_t adc_read(uint8_t ch) {
    ADC1->RSQR3=ch;
    ADC1->CTLR2|=ADC_SWSTART;
    while(!(ADC1->STATR&ADC_EOC));
    uint16_t dummy = ADC1->RDATAR;
    
    ADC1->RSQR3=ch;
    ADC1->CTLR2|=ADC_SWSTART;
    while(!(ADC1->STATR&ADC_EOC));
    return (uint16_t)ADC1->RDATAR;
}

void clear_grid() { for(int r=0;r<MAX_ROWS;r++)for(int c=0;c<8;c++) ladder_grid[r][c]=EMPTY; }
void save_file(uint8_t slot) { uint16_t base = 0x2000 + slot * 256; for(int i=0; i<MAX_ROWS; i++)for(int j=0; j<8; j++){ uint16_t c = ladder_grid[i][j]; eeprom_write_byte(base + (i*8+j)*2, c>>8); eeprom_write_byte(base + (i*8+j)*2 + 1, c&0xFF); } }
void load_file(uint8_t slot) { uint16_t base = 0x2000 + slot * 256; for(int i=0; i<MAX_ROWS; i++)for(int j=0; j<8; j++){ uint16_t h = eeprom_read_byte(base + (i*8+j)*2); uint16_t l = eeprom_read_byte(base + (i*8+j)*2 + 1); ladder_grid[i][j] = (h<<8)|l; } }

void init_samples() {
    clear_grid(); const char* n0="LATCH   "; for(int i=0;i<8;i++)ladder_grid[0][i]=PACK(7,0,0,n0[i]); ladder_grid[1][0]=PACK(2,0,0,0); for(int i=1;i<7;i++)ladder_grid[1][i]=PACK(0,0,0,0); ladder_grid[1][7]=PACK(4,1,0,1); ladder_grid[2][0]=PACK(2,0,1,0); for(int i=1;i<7;i++)ladder_grid[2][i]=PACK(0,0,0,0); ladder_grid[2][7]=PACK(4,1,0,2); save_file(0);
    clear_grid(); const char* n1="TIMER   "; for(int i=0;i<8;i++)ladder_grid[0][i]=PACK(7,0,0,n1[i]); ladder_grid[1][0]=PACK(2,0,0,0); for(int i=1;i<7;i++)ladder_grid[1][i]=PACK(0,0,0,0); ladder_grid[1][7]=PACK(4,3,0,10); ladder_grid[2][0]=PACK(2,3,0,0); for(int i=1;i<7;i++)ladder_grid[2][i]=PACK(0,0,0,0); ladder_grid[2][7]=PACK(4,1,0,0); save_file(1);
    clear_grid(); const char* n2="COUNT   "; for(int i=0;i<8;i++)ladder_grid[0][i]=PACK(7,0,0,n2[i]); ladder_grid[1][0]=PACK(2,0,0,0); for(int i=1;i<7;i++)ladder_grid[1][i]=PACK(0,0,0,0); ladder_grid[1][7]=PACK(4,4,0,5); ladder_grid[2][0]=PACK(2,4,0,0); for(int i=1;i<7;i++)ladder_grid[2][i]=PACK(0,0,0,0); ladder_grid[2][7]=PACK(4,1,0,0); ladder_grid[3][0]=PACK(2,0,1,0); for(int i=1;i<7;i++)ladder_grid[3][i]=PACK(0,0,0,0); ladder_grid[3][7]=PACK(4,4,0,0); save_file(2);
    clear_grid(); const char* n3="BLINK   "; for(int i=0;i<8;i++)ladder_grid[0][i]=PACK(7,0,0,n3[i]); ladder_grid[1][0]=PACK(3,3,1,0); for(int i=1;i<7;i++)ladder_grid[1][i]=PACK(0,0,0,0); ladder_grid[1][7]=PACK(4,3,0,5); ladder_grid[2][0]=PACK(2,3,0,0); for(int i=1;i<7;i++)ladder_grid[2][i]=PACK(0,0,0,0); ladder_grid[2][7]=PACK(4,3,1,5); ladder_grid[3][0]=PACK(2,3,0,0); for(int i=1;i<7;i++)ladder_grid[3][i]=PACK(0,0,0,0); ladder_grid[3][7]=PACK(4,1,0,0); save_file(3);
    clear_grid(); const char* n4="STEP    "; for(int i=0;i<8;i++)ladder_grid[0][i]=PACK(7,0,0,n4[i]); ladder_grid[1][0]=PACK(2,0,0,0); for(int i=1;i<7;i++)ladder_grid[1][i]=PACK(0,0,0,0); ladder_grid[1][7]=PACK(4,2,0,1); ladder_grid[2][0]=PACK(2,2,0,0); ladder_grid[2][1]=PACK(2,0,1,0); for(int i=2;i<7;i++)ladder_grid[2][i]=PACK(0,0,0,0); ladder_grid[2][7]=PACK(4,1,0,0); save_file(4);
    for(int s=5;s<10;s++){ clear_grid(); save_file(s); } clear_grid();
}

void setup() {
    RCC->APB2PCENR|=RCC_AFIOEN|RCC_IOPAEN|RCC_IOPCEN|RCC_IOPDEN|RCC_ADC1EN;
    GPIOC->CFGLR&=~((0xF<<16)|(0xFF<<24)); GPIOC->CFGLR|=((0x8<<16)|(0x55<<24)); GPIOC->BSHR=(1<<4);
    ADC1->CTLR2|=(1<<20)|(7<<17)|ADC_ADON; oled_init(); adc_offset_x=adc_read(1); adc_offset_y=adc_read(0);
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR<<1)|0); soft_i2c_write(0); soft_i2c_write(0); soft_i2c_write(0xAA); soft_i2c_stop(); delay(5);
    if(eeprom_read_byte(0)==0xAA) { eeprom_ok=true; for(int i=0; i<95; i++){ uint16_t addr = 0x0100 + i*5; for(int j=0; j<5; j++) font_cache[i][j] = eeprom_read_byte(addr+j); } }
    if(eeprom_read_byte(0x1FFF)!=0x77) { init_samples(); eeprom_write_byte(0x1FFF, 0x77); } else { clear_grid(); }
    pinMode(PD5, INPUT_PULLUP); pinMode(PD6, INPUT_PULLUP); pinMode(PC1, INPUT_PULLUP); pinMode(PC2, INPUT_PULLUP);
    pinMode(PC0, OUTPUT); pinMode(PC3, OUTPUT); pinMode(PC5, OUTPUT); pinMode(PD2, OUTPUT);
}

void loop() {
    uint16_t x_raw=adc_read(1), y_raw=adc_read(0); static bool last_sw=true; bool cur_sw=(GPIOC->INDR&(1<<4)); bool frame_clicked=(last_sw&&!cur_sw); last_sw=cur_sw;
    int16_t dx=0,dy=0, diff_x=(int16_t)x_raw-(int16_t)adc_offset_x, diff_y=(int16_t)y_raw-(int16_t)adc_offset_y;
    if(diff_x>DEADZONE)dx=diff_x/128+1; else if(diff_x<-DEADZONE)dx=diff_x/128-1;
    if(diff_y>DEADZONE)dy=diff_y/128+1; else if(diff_y<-DEADZONE)dy=diff_y/128-1;
    int16_t next_x = mouse_x + dx; int16_t next_y = mouse_y + dy;
    if(popup_active && popup_step==4 && dy!=0) { static uint8_t kt=0; if(kt++>2) { if(dy<0 && temp_k<127) temp_k++; if(dy>0 && (temp_type==7?temp_k>32:temp_k>0)) temp_k--; kt=0; } next_y = mouse_y; }
    dev_X = (!digitalRead(PD5)) | (!digitalRead(PD6)<<1) | (!digitalRead(PC1)<<2) | (!digitalRead(PC2)<<3);
    bool tick_100ms = false; uint32_t cm = millis(); if(cm - last_tick >= 100) { tick_100ms = true; last_tick = cm; }
    if (plc_running) {
        for (uint8_t r = 0; r < MAX_ROWS; r++) {
            bool power = true;
            for (uint8_t c = 0; c < 8; c++) {
                uint16_t cell = ladder_grid[r][c]; if (cell == EMPTY || TYPE(cell)==7) power = false;
                else {
                    uint8_t t = TYPE(cell), d = DEV(cell), idx = IDX(cell), k = KVAL(cell);
                    if (t == 2 || t == 3) {
                        bool s = (d==0)?(dev_X&(1<<idx)):(d==1)?(dev_Y&(1<<idx)):(d==2)?(dev_M&(1<<idx)):(d==3)?(dev_T_out&(1<<idx)):(d==4)?(dev_C_out&(1<<idx)):0;
                        if (t == 3) s = !s; power = power && s;
                    } else if (t == 4) {
                        if (d == 1) { if(k==1){if(power)dev_Y|=(1<<idx);}else if(k==2){if(power)dev_Y&=~(1<<idx);}else{if(power)dev_Y|=(1<<idx);else dev_Y&=~(1<<idx);}}
                        else if (d == 2) { if(k==1){if(power)dev_M|=(1<<idx);}else if(k==2){if(power)dev_M&=~(1<<idx);}else{if(power)dev_M|=(1<<idx);else dev_M&=~(1<<idx);}}
                        else if (d == 3) { if(power){if(tick_100ms && dev_T_val[idx]<0xFFFF)dev_T_val[idx]++;if(dev_T_val[idx]>=k)dev_T_out|=(1<<idx);}else{dev_T_val[idx]=0;dev_T_out&=~(1<<idx);}}
                        else if (d == 4) {
                            if (k == 0) {
                                if (power) { dev_C_val[idx] = 0; dev_C_out &= ~(1<<idx); }
                            } else {
                                bool last_c = c_coil_power_last & (1<<idx);
                                if (power && !last_c) { if (dev_C_val[idx] < 0xFFFF) dev_C_val[idx]++; }
                                if (dev_C_val[idx] >= k) dev_C_out |= (1<<idx);
                                if (power) c_coil_power_last |= (1<<idx); else c_coil_power_last &= ~(1<<idx);
                            }
                        }
                    }
                }
            }
        }
    }
    digitalWrite(PC0, (dev_Y & 1)); digitalWrite(PC3, (dev_Y & 2)); digitalWrite(PC5, (dev_Y & 4)); digitalWrite(PD2, (dev_Y & 8));
    static uint8_t edge_timer = 0; if(next_y > 63) { next_y = 63; if(edge_timer++ > 10) { if((menu_state==3||menu_state==2) && !popup_active && scroll_y < (MAX_ROWS-3)) scroll_y++; else if(menu_state==4) mnm_scroll++; edge_timer = 0; } }
    else if(next_y < 0) { next_y = 0; if(edge_timer++ > 10) { if((menu_state==3||menu_state==2) && !popup_active && scroll_y > 0) scroll_y--; else if(menu_state==4 && mnm_scroll > 0) mnm_scroll--; edge_timer = 0; } } else edge_timer = 0;
    mouse_x = (uint8_t)constrain(next_x, 0, 127); mouse_y = (uint8_t)next_y;
    bool clicked = frame_clicked;
    for (current_page = 0; current_page < 8; current_page++) {
        for(int i=0; i<128; i++) oled_buffer[i] = 0;
        draw_window(0,0,128,64); for(int i=0;i<128;i++) draw_pixel(i,11);
        if(menu_state==0){
            draw_string(5,3,"SYS"); draw_string(35,3,"PRG"); draw_string(65,3,"MON");
            if(mouse_y<11){ if(mouse_x<28){invert_rect(4,2,23,9);if(clicked){menu_state=1; clicked=false;}} else if(mouse_x<58){invert_rect(34,2,23,9);if(clicked){menu_state=3; clicked=false;}} else if(mouse_x<88){invert_rect(64,2,23,9);if(clicked){menu_state=2; clicked=false;}} }
            draw_string(20,30,"HANDY PLC CORE"); draw_string(25,45,"READY..."); draw_cursor(mouse_x,mouse_y);
        } else if(menu_state==1){
            draw_string(5,3,"ESC"); draw_string(35,3,"SAV"); draw_string(65,3,"LOD");
            if(mouse_y<11){ if(mouse_x<28){invert_rect(4,2,23,9);if(clicked){menu_state=0; clicked=false;}} else if(mouse_x<58){invert_rect(34,2,23,9);if(clicked){menu_state=5; clicked=false;}} else if(mouse_x<88){invert_rect(64,2,23,9);if(clicked){menu_state=6; clicked=false;}} }
            draw_string(30,30,"PLC V0.26"); draw_cursor(mouse_x,mouse_y);
        } else if(menu_state==5 || menu_state==6){
            draw_string(5,3,"ESC"); draw_string(35,3,menu_state==5?"SAVE":"LOAD");
            if(mouse_y<11&&mouse_x<28){invert_rect(4,2,23,9);if(clicked){menu_state=1; clicked=false;}}
            for(int i=0;i<10;i++){
                uint8_t px=5+(i%2)*60, py=15+(i/2)*9; char fn[10]="F"; fn[1]='0'+i; fn[2]=':'; for(int j=0;j<6;j++){ uint16_t cell=eeprom_read_byte(0x2000+i*256+j*2)<<8|eeprom_read_byte(0x2000+i*256+j*2+1); if(TYPE(cell)==7)fn[3+j]=KVAL(cell); else fn[3+j]=' '; } fn[9]=0; draw_string(px,py,fn);
                if(mouse_x>=px&&mouse_x<px+55&&mouse_y>=py&&mouse_y<py+8){ invert_rect(px-1,py-1,56,9); if(clicked){ if(menu_state==5)save_file(i); else load_file(i); menu_state=1; clicked=false; } }
            }
            draw_cursor(mouse_x,mouse_y);
        } else if(menu_state==2 || menu_state==3){
            draw_string(5,3,"ESC"); if(menu_state==2){draw_string(35,3,"RUN");draw_string(65,3,"STP");}else{draw_string(35,3,"MNM");}
            if(mouse_y<11&&!popup_active){ if(mouse_x<28){invert_rect(4,2,23,9);if(clicked){plc_running=false;dev_Y=0;dev_M=0;dev_T_out=0;dev_C_out=0;for(int i=0;i<4;i++){dev_T_val[i]=0;dev_C_val[i]=0;}menu_state=0; clicked=false;}} else if(menu_state==2){if(mouse_x<58){invert_rect(34,2,23,9);if(clicked){plc_running=true;clicked=false;}}else if(mouse_x<88){invert_rect(64,2,23,9);if(clicked){plc_running=false;dev_Y=0;dev_M=0;dev_T_out=0;dev_C_out=0;for(int i=0;i<4;i++){dev_T_val[i]=0;dev_C_val[i]=0;}clicked=false;}}} else if(mouse_x<58){invert_rect(34,2,23,9);if(clicked){menu_state=4;clicked=false;}} }
            if(menu_state==2){
                if(plc_running)invert_rect(34,2,23,9);else invert_rect(64,2,23,9);
                draw_bitmap(115, 2, plc_running ? run_icon : stp_icon, 8, 8);
            }
            for(int i=0;i<128;i++)draw_pixel(i,15); invert_rect(0,16,1,48); invert_rect(127,16,1,48);
            for(uint8_t r=0;r<3;r++){
                uint8_t gr=scroll_y+r; for(uint8_t c=0;c<8;c++){
                    uint16_t cell=ladder_grid[gr][c]; if(cell==EMPTY)continue; uint8_t t=TYPE(cell), d=DEV(cell), idx=IDX(cell), k=KVAL(cell);
                    if(t==7){draw_char(c*16+5,16+r*16+4,k);continue;} draw_ladder_icon(c*16,16+r*16+8,t); if(t==1){for(uint8_t p=0;p<8;p++)draw_pixel(c*16,16+r*16+p);}
                    if(t>=2&&t<=4){ 
                        char s[3]={"XYMTC"[d],(char)('0'+idx),0}; 
                        draw_string(c*16+1,16+r*16,s); 
                        if(t==4){
                            if(d==1||d==2){if(k==1)draw_char(c*16+5,16+r*16+9,'S');if(k==2)draw_char(c*16+5,16+r*16+9,'R');}
                            else if(d==4&&k==0){draw_char(c*16+5,16+r*16+9,'R');}
                        } 
                    }
                    if(menu_state==2){ bool p=(d==0)?(dev_X&(1<<idx)):(d==1)?(dev_Y&(1<<idx)):(d==2)?(dev_M&(1<<idx)):(d==3)?(dev_T_out&(1<<idx)):(d==4)?(dev_C_out&(1<<idx)):0; if((t==2&&p)||(t==3&&!p)||(t==4&&p))invert_rect(c*16,16+r*16,16,16); }
                }
            }
            if(popup_active){
                clear_rect(2,20,124,30); draw_window(2,20,124,30);
                if(popup_step==1){
                    uint8_t pi[6]={0,2,3,4,7,255}; 
                    for(int i=0;i<6;i++){
                        uint8_t t=pi[i]; uint8_t px=6+i*20; 
                        if(t==255) draw_string(px+1,30,"DEL"); else if(t==7) draw_string(px+1,30,"TXT"); else draw_ladder_icon(px+2, 28, t); 
                        if(mouse_x>=px-2&&mouse_x<px+18&&mouse_y>=20&&mouse_y<=50){ invert_rect(px-2,22,20,26); if(clicked){ if(t==255){ladder_grid[popup_target_y][popup_target_x]=EMPTY;popup_active=false;} else if(t==7){temp_type=7;popup_step=4;temp_k='A';} else if(t==0){ladder_grid[popup_target_y][popup_target_x]=PACK(0,0,0,0);popup_active=false;} else{temp_type=t;popup_step=2;} clicked=false; } } 
                    }
                } else if(popup_step==2){ for(int i=0;i<5;i++){ uint8_t px=10+i*20; char ds[2]={"XYMTC"[i],0}; draw_string(px+5,30,ds); if(mouse_x>=px&&mouse_x<px+18&&mouse_y>=20&&mouse_y<=50){ invert_rect(px,22,18,26); if(clicked){temp_dev=i;popup_step=3;clicked=false;} } }
                } else if(popup_step==3){ uint8_t mi=(temp_dev>=3)?4:8; for(int i=0;i<mi;i++){ char n[2]={(char)('0'+i),0}; uint8_t px=10+i*14; draw_string(px+4,31,n); if(mouse_x>=px&&mouse_x<px+12&&mouse_y>=20&&mouse_y<=50){ invert_rect(px,22,12,26); if(clicked){ temp_idx=i; if(temp_type==4&&temp_dev>0){popup_step=4;temp_k=(temp_dev>=3)?10:0;}else{if(temp_type==4){ladder_grid[popup_target_y][7]=PACK(4,temp_dev,temp_idx,0);for(uint8_t f=popup_target_x;f<7;f++)if(ladder_grid[popup_target_y][f]==EMPTY)ladder_grid[popup_target_y][f]=0;}else{ladder_grid[popup_target_y][popup_target_x]=PACK(temp_type,temp_dev,temp_idx,0);}popup_active=false;} clicked=false; } } }
                } else if(popup_step==4){
                    if(temp_type==7){ draw_char(30,30,temp_k); draw_string(70,30,"OK"); if(mouse_x>=66&&mouse_x<90&&mouse_y>=20&&mouse_y<=50){ invert_rect(66,22,24,26); if(clicked){ladder_grid[popup_target_y][popup_target_x]=PACK(7,0,0,temp_k);popup_active=false;clicked=false;} } }
                    else if(temp_dev<=2){ draw_string(10,30,"OUT");draw_string(50,30,"SET");draw_string(90,30,"RST"); if(mouse_x>=8&&mouse_x<32&&mouse_y>=20&&mouse_y<=50){if(clicked){temp_k=0;popup_active=false;clicked=false;}}else if(mouse_x>=48&&mouse_x<72&&mouse_y>=20&&mouse_y<=50){if(clicked){temp_k=1;popup_active=false;clicked=false;}}else if(mouse_x>=88&&mouse_x<112&&mouse_y>=20&&mouse_y<=50){if(clicked){temp_k=2;popup_active=false;clicked=false;}} if(!popup_active){ladder_grid[popup_target_y][7]=PACK(4,temp_dev,temp_idx,temp_k);for(uint8_t f=popup_target_x;f<7;f++)if(ladder_grid[popup_target_y][f]==EMPTY)ladder_grid[popup_target_y][f]=0;} }
                    else {
                        if(temp_dev==4 && temp_k==0) { draw_string(10,30,"RST C"); }
                        else { draw_string(10,30,"K:"); char ks[4]={(char)('0'+temp_k/100),(char)('0'+(temp_k/10)%10),(char)('0'+temp_k%10),0}; draw_string(30,30,ks); }
                        draw_string(70,30,"OK"); if(mouse_x>=66&&mouse_x<90&&mouse_y>=20&&mouse_y<=50){ invert_rect(66,22,24,26); if(clicked){ladder_grid[popup_target_y][7]=PACK(4,temp_dev,temp_idx,temp_k);for(uint8_t f=popup_target_x;f<7;f++)if(ladder_grid[popup_target_y][f]==EMPTY)ladder_grid[popup_target_y][f]=0;popup_active=false;clicked=false;} }
                    }
                } draw_cursor(mouse_x,mouse_y);
            } else {
                if(menu_state == 3 && mouse_y>15){ uint8_t gx=(mouse_x/16)*16, gy=16+((mouse_y-16)/16)*16; if(gy>48)gy=48; invert_rect(gx,gy,16,16); if(clicked){popup_target_x=gx/16;popup_target_y=scroll_y+(gy-16)/16;popup_active=true;popup_step=1;clicked=false;} } 
                else draw_cursor(mouse_x,mouse_y); 
            }
        } else if(menu_state==4){
            draw_string(5,3,"ESC"); draw_string(35,3,"LAD"); if(mouse_y<11){ if(mouse_x<28){invert_rect(4,2,23,9);if(clicked){menu_state=0;clicked=false;}} else if(mouse_x<58){invert_rect(34,2,23,9);if(clicked){menu_state=3;clicked=false;}} }
            uint8_t sc=0, drn=0, dy=15; for(uint8_t r=0;r<MAX_ROWS;r++){ bool fir=true; for(uint8_t c=0;c<8;c++){ uint16_t cell=ladder_grid[r][c]; if(cell!=EMPTY){ uint8_t t=TYPE(cell), d=DEV(cell), idx=IDX(cell), k=KVAL(cell); if(t>=2&&t<=4){ if(sc>=mnm_scroll&&drn<6){ char l[16]; const char* cmd=(t==2)?(fir?"LD ":"AND"):(t==3)?(fir?"LDI":"ANI"):(((d<=2&&k==1)?"SET":((d<=2&&k==2)||(d==4&&k==0))?"RST":"OUT")); fir=false; l[0]='0'+sc/10;l[1]='0'+sc%10;l[2]=':';l[3]=' '; for(int i=0;i<3;i++)l[4+i]=cmd[i]; l[7]=' ';l[8]="XYMTC"[d];l[9]='0'+idx; if(t==4&&d>=3&&!(d==4&&k==0)){l[10]=' ';l[11]='K';l[12]='0'+k/100;l[13]='0'+(k/10)%10;l[14]='0'+k%10;l[15]=0;}else l[10]=0; draw_string(5,dy,l); dy+=8; drn++; } sc++; } } } } draw_cursor(mouse_x,mouse_y);
        }
        oled_set_pos(0,current_page); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40); for(int x=0;x<128;x++) soft_i2c_write(oled_buffer[x]); soft_i2c_stop();
    }
}
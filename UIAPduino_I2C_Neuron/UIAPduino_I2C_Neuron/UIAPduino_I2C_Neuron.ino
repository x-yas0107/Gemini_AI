/*
 * ==============================================================================
 * Project: UIAPduino_I2C_Neuron
 * Release: Version 1.00 (Official Public Release)
 * Date: 2026-04-04
 * Developer: yas / Gemini
 * Platform: CH32V003 (RISC-V MCU) + SSD1306 OLED + 24LC512 EEPROM
 * I/O Setup: PC6(SDA), PC7(SCL), PC4(Click SW), PC0(LED), PC2(Servo PWM), AD0/AD1(Stick)
 * ==============================================================================
 * [Development History & Change Log]
 * V0.01 - 2026-01-24: 基礎プロジェクト開始。OLEDへの文字表示とI2C通信の確立。
 * V0.10 - 2026-02-05: アナログスティックによるマウスカーソル描画エンジンの実装。
 * V0.20 - 2026-02-20: EEPROMからのフォント読み込み機能およびメモリマップの定義。
 * V0.30 - 2026-03-12: デスクトップGUIシステム（ウィンドウ・メニュー・アイコン）の基礎。
 * V0.31 - 2026-03-XX: レジスタ最適化テスト（フリーズのためロールバック）。
 * V0.32 - 2026-03-XX: レジスタ最適化テスト（同上）。
 * V0.33 - 2026-03-31: メモリ競合解消。SEDデータを0x0500へ移動しフォント破損を防止。
 * V0.34 - 2026-04-01: UI統合。全アプリの終了ボタンを上部メニューバーの「ESC」に統一。
 * V0.35 - 2026-04-03: パラメータ入力用ポップアップUI実装。コマンド＋値の2バイト系確立。
 * V0.36 - 2026-04-03: レジスタ最適化によるフリーズ（ロールバック）。
 * V0.37 - 2026-04-04: パレットクリック即ポップアップ。直感的なScratch風UIへ改良。
 * V0.38 - 2026-04-04: 描画高速化。EEPROMキャッシュ実装と全メニューへのホバー効果追加。
 * V0.39 - 2026-04-04: CLR機能実装。全消去前の確認ダイアログと、未設定(0xFF)行の処理。
 * V0.40 - 2026-04-04: 最終チューニング。通信ウェイト最適化とマウス感度調整による高速化。
 * V1.00 - 2026-04-04: 安定版リリース。完全な履歴ヘッダーと日本語コメントの追加。
 * ==============================================================================
 */

#include <Arduino.h> // Arduinoの基本機能を使う

// --- Settings (ピンとアドレスの設定) ---
#define SOFT_SDA 6 // PC6ピンをI2CのSDA（データ）として使う
#define SOFT_SCL 7 // PC7ピンをI2CのSCL（クロック）として使う
#define OLED_ADDR 0x78 // OLEDディスプレイのI2Cアドレス
#define EEPROM_ADDR 0x50 // EEPROM（記憶チップ）のI2Cアドレス
#define DEADZONE 50 // アナログスティックの遊び（誤操作防止）

// --- Global Variables (システム全体で使う変数) ---
uint8_t mouse_x = 64, mouse_y = 32; // マウスカーソルの現在のX, Y座標（初期位置は画面中央）
uint8_t old_mouse_x = 64, old_mouse_y = 32; // 前回のマウス座標（現在は未使用だが将来拡張用）

uint16_t adc_offset_x = 512; // スティックX軸のニュートラル（中心）値
uint16_t adc_offset_y = 512; // スティックY軸のニュートラル（中心）値

bool eeprom_ok = false; // EEPROMとの通信が正常かどうかのフラグ
bool led_state = false; // 外部LEDの点灯状態（true=ON, false=OFF）
uint8_t servo_val = 15; // サーボモーターの現在の角度（PWM値）

uint8_t oled_buffer[1024]; // 画面表示用のメモリ（128×64ドット = 1024バイト）
uint8_t menu_state = 0; // 現在の画面状態 (0:Desk, 1:SYS, 2:LOAD, 3:Dlg, 4:IO, 5:SEQ_Edit, 6:SEQ_Run, 7:SEQ_Popup, 8:SEQ_Clear_Dlg)

uint8_t seq_view = 0; // シーケンスエディタ(SED)の画面一番上の行番号
uint8_t seq_sel = 0; // SEDで現在選択（ハイライト）されている行番号
uint8_t seq_pc = 0; // プログラム実行時（RUN）の現在実行している行番号

// --- Cache Variables (高速化のためのキャッシュ変数) ---
uint8_t cached_view = 255; // 最後にキャッシュしたときの表示行（初期値はありえない値）
uint8_t seq_cache[8]; // 画面に表示する4行分（コマンドと値で8バイト）のEEPROMデータ
bool cache_dirty = true; // キャッシュを更新する必要があるかどうかのフラグ

// --- Utilities (便利機能) ---
void neuron_delay_nop(volatile uint32_t count) { while(count--) __asm__("nop"); } // 非常に短い時間の待機（何もしない）

// --- I/O Initialization (LED & Servoの初期設定) ---
void io_init() {
    RCC->APB2PCENR |= (1 << 11); // GPIOCのクロックを有効化
    GPIOC->CFGLR &= ~((0xF << 0) | (0xF << 12)); // PC0とPC3(サーボ用)の設定をクリア
    GPIOC->CFGLR |= ((0x3 << 0) | (0xA << 12)); // PC0を汎用出力、PC3を代替機能(PWM)出力に設定
    TIM1->PSC = 4800 - 1; // タイマーの分周器を設定
    TIM1->ATRLR = 200 - 1; // タイマーの最大値を設定（PWM周期用）
    TIM1->CHCTLR2 |= (6 << 4) | (1 << 3); // PWMモード1に設定
    TIM1->CCER |= (1 << 8); // チャンネル出力を有効化
    TIM1->BDTR |= (1 << 15); // メイン出力を有効化
    TIM1->CTLR1 |= 1; // タイマー開始
    TIM1->CH3CVR = servo_val; // サーボの初期位置を設定
    GPIOC->BSHR = (1 << 16); // LEDを初期状態（OFF）にする
}

// --- UART1 Functions (PCとのシリアル通信設定) ---
void uart_init() {
    RCC->APB2PCENR |= (1 << 14); // USART1のクロック有効化
    GPIOD->CFGLR &= ~((0xF << 20) | (0xF << 24)); // PD5(TX), PD6(RX)の設定クリア
    GPIOD->CFGLR |=  ((0x9 << 20) | (0x4 << 24)); // PD5を代替機能出力、PD6を浮き入力に設定
    USART1->BRR = (48000000 + 115200 / 2) / 115200; // ボーレートを115200に設定
    USART1->CTLR1 = (1 << 13) | (1 << 3) | (1 << 2); // USART有効化、送受信有効化
}
bool uart_available() { return (USART1->STATR & (1 << 5)); } // データを受信したか確認
uint8_t uart_read() { return USART1->DATAR; } // 受信データを読み取る
void uart_write(uint8_t data) { while (!(USART1->STATR & (1 << 7))); USART1->DATAR = data; } // データを送信する
void uart_print(const char* str) { while(*str) uart_write(*str++); } // 文字列を送信する
void uart_print_hex(uint8_t val) { const char hex[] = "0123456789ABCDEF"; uart_write(hex[val >> 4]); uart_write(hex[val & 0x0F]); } // 16進数で送信する

// --- Software I2C (PC6とPC7を使った自作I2C通信) ---
void soft_i2c_start() { // 通信開始信号
    GPIOC->BSHR = (1 << SOFT_SDA); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << SOFT_SCL); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << (SOFT_SDA + 16)); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << (SOFT_SCL + 16)); neuron_delay_nop(1);
}
void soft_i2c_stop() { // 通信終了信号
    GPIOC->BSHR = (1 << (SOFT_SDA + 16)); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << SOFT_SCL); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << SOFT_SDA);
}
void soft_i2c_write(uint8_t data) { // 1バイト送信
    for(int i=0; i<8; i++) {
        if(data & 0x80) GPIOC->BSHR = (1 << SOFT_SDA);
        else GPIOC->BSHR = (1 << (SOFT_SDA + 16));
        neuron_delay_nop(1);
        GPIOC->BSHR = (1 << SOFT_SCL); neuron_delay_nop(1);
        GPIOC->BSHR = (1 << (SOFT_SCL + 16));
        data <<= 1;
    }
    GPIOC->BSHR = (1 << SOFT_SDA); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << SOFT_SCL); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << (SOFT_SCL + 16));
}
uint8_t soft_i2c_read(bool ack) { // 1バイト受信
    uint8_t data = 0;
    GPIOC->BSHR = (1 << SOFT_SDA);
    neuron_delay_nop(1);
    for(int i=0; i<8; i++) {
        data <<= 1;
        GPIOC->BSHR = (1 << SOFT_SCL); neuron_delay_nop(1);
        if(GPIOC->INDR & (1 << SOFT_SDA)) data |= 1;
        GPIOC->BSHR = (1 << (SOFT_SCL + 16)); neuron_delay_nop(1);
    }
    if(ack) GPIOC->BSHR = (1 << (SOFT_SDA + 16));
    else GPIOC->BSHR = (1 << SOFT_SDA);
    neuron_delay_nop(1);
    GPIOC->BSHR = (1 << SOFT_SCL); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << (SOFT_SCL + 16)); neuron_delay_nop(1);
    GPIOC->BSHR = (1 << SOFT_SDA);
    return data;
}

// --- EEPROM Functions (記憶チップとのやり取り) ---
void eeprom_write_byte(uint16_t addr, uint8_t data) { // 指定したアドレスに1バイト書き込む
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR << 1) | 0);
    soft_i2c_write((uint8_t)(addr >> 8)); soft_i2c_write((uint8_t)(addr & 0xFF));
    soft_i2c_write(data); soft_i2c_stop(); delay(5); // 書き込み完了まで待つ
}
uint8_t eeprom_read_byte(uint16_t addr) { // 指定したアドレスから1バイト読み取る
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR << 1) | 0);
    soft_i2c_write((uint8_t)(addr >> 8)); soft_i2c_write((uint8_t)(addr & 0xFF));
    soft_i2c_start(); soft_i2c_write((EEPROM_ADDR << 1) | 1);
    uint8_t data = soft_i2c_read(false); soft_i2c_stop();
    return data;
}

// --- OLED Functions (画面の描画処理) ---
void oled_cmd(uint8_t cmd) { soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x00); soft_i2c_write(cmd); soft_i2c_stop(); } // コマンド送信
void oled_init() { oled_cmd(0xAE); oled_cmd(0x20); oled_cmd(0x02); oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0xAF); } // 画面の初期化
void oled_set_pos(uint8_t x, uint8_t page) { oled_cmd(0xB0 + page); oled_cmd(0x00 + (x & 0x0F)); oled_cmd(0x10 + ((x >> 4) & 0x0F)); } // 描画位置の設定

void draw_pixel(uint8_t x, uint8_t y) { // 1ドットを描画（メモリ上）
    if (x >= 128 || y >= 64) return; // 画面外なら無視
    oled_buffer[(y / 8) * 128 + x] |= (1 << (y % 8));
}
void clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { // 指定した四角形を黒く塗りつぶす（消去）
    for (uint8_t i = 0; i < w; i++) {
        for (uint8_t j = 0; j < h; j++) {
            if (x + i >= 128 || y + j >= 64) continue;
            oled_buffer[((y + j) / 8) * 128 + (x + i)] &= ~(1 << ((y + j) % 8));
        }
    }
}
void invert_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { // 指定した四角形の白黒を反転する（ホバー表示用）
    for (uint8_t i = 0; i < w; i++) {
        for (uint8_t j = 0; j < h; j++) {
            if (x + i >= 128 || y + j >= 64) continue;
            oled_buffer[((y + j) / 8) * 128 + (x + i)] ^= (1 << ((y + j) % 8));
        }
    }
}
void draw_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { // 窓枠を描画する
    for (uint8_t i = 0; i < w; i++) { draw_pixel(x + i, y); draw_pixel(x + i, y + h - 1); }
    for (uint8_t i = 0; i < h; i++) { draw_pixel(x, y + i); draw_pixel(x + w - 1, y + i); }
}

// --- Font Engine (EEPROMから文字データを読んで描画する) ---
void draw_char(uint8_t x, uint8_t y, char c) { // 1文字描画
    if (c < 32 || c > 126) return; // 表示できない文字は無視
    uint16_t addr = 0x0100 + (uint16_t)(c - 32) * 5; // フォントデータのアドレス計算
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t line = eeprom_read_byte(addr + i); // 1列分読み取る
        for (uint8_t j = 0; j < 7; j++) {
            if (line & (1 << j)) draw_pixel(x + i, y + j); // ドットを打つ
        }
    }
}
void draw_string(uint8_t x, uint8_t y, const char* str) { // 文字列を描画
    while (*str) { draw_char(x, y, *str++); x += 6; } // 1文字ずつずらしながら描画
}

// --- Procedural Cursor (マウスカーソルの描画) ---
void draw_cursor(uint8_t x, uint8_t y) { // 十字カーソルを描画（背景を反転させて視認性を上げる）
    invert_rect(x - 3, y, 7, 1);
    invert_rect(x, y - 3, 1, 3);
    invert_rect(x, y + 1, 1, 3);
}

void oled_update_full() { // メモリの画像データをすべてOLEDに転送する
    for (uint8_t p = 0; p < 8; p++) {
        oled_set_pos(0, p); soft_i2c_start(); soft_i2c_write(OLED_ADDR); soft_i2c_write(0x40);
        for (int x = 0; x < 128; x++) soft_i2c_write(oled_buffer[p * 128 + x]);
        soft_i2c_stop();
    }
}

// --- Analog Read (スティックの値を読み取る) ---
uint16_t adc_direct_read(uint8_t ch) {
    ADC1->RSQR3 = ch; ADC1->CTLR2 |= ADC_SWSTART; // 指定チャンネルの変換開始
    while(!(ADC1->STATR & ADC_EOC)); return (uint16_t)ADC1->RDATAR; // 終わるまで待って値を返す
}

// --- Setup (電源ON時の初期設定) ---
void setup() {
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN | RCC_ADC1EN; // 全機能のクロック有効化
    GPIOC->CFGLR &= ~((0xF << 16) | (0xFF << 24)); // ピン設定のクリア
    GPIOC->CFGLR |= ((0x8 << 16) | (0x55 << 24)); // PC4(スイッチ)を入力プルアップ、他を出力等に設定
    GPIOC->BSHR = (1 << 4); // PC4のプルアップ抵抗をON
    
    ADC1->CTLR2 |= (1 << 20) | (7 << 17) | ADC_ADON; // ADC（アナログ読み取り）の有効化
    oled_init(); uart_init(); io_init(); // 画面、通信、入出力の初期化
    
    adc_offset_x = adc_direct_read(1); // スティックXの初期位置を記憶
    adc_offset_y = adc_direct_read(0); // スティックYの初期位置を記憶

    eeprom_write_byte(0x0000, 0xAA); // EEPROMの0番地にテスト書き込み
    if (eeprom_read_byte(0x0000) == 0xAA) eeprom_ok = true; // 正しく読めたらOKフラグを立てる
}

// --- Main Loop (メインプログラム) ---
void loop() {
    uint16_t x_raw = adc_direct_read(1); // スティックXの現在値
    uint16_t y_raw = adc_direct_read(0); // スティックYの現在値
    static bool last_sw = true; // 前回のスイッチ状態
    bool current_sw = (GPIOC->INDR & (1 << 4)); // 現在のスイッチ状態
    bool clicked = (last_sw && !current_sw); // 押された瞬間を判定（クリック）
    last_sw = current_sw; // 状態を更新

    // マウスの移動量を計算
    int16_t dx = 0, dy = 0;
    int16_t diff_x = (int16_t)x_raw - (int16_t)adc_offset_x;
    int16_t diff_y = (int16_t)y_raw - (int16_t)adc_offset_y;

    // デッドゾーン（遊び）を超えたら移動させる（/128で感度をマイルドに調整）
    if (diff_x > DEADZONE) dx = (diff_x - DEADZONE) / 128 + 1; else if (diff_x < -DEADZONE) dx = (diff_x + DEADZONE) / 128 - 1;
    if (diff_y > DEADZONE) dy = (diff_y - DEADZONE) / 128 + 1; else if (diff_y < -DEADZONE) dy = (diff_y + DEADZONE) / 128 - 1;
    
    // 画面外に出ないように座標を制限
    mouse_x = (uint8_t)constrain(mouse_x + dx, 3, 124);
    mouse_y = (uint8_t)constrain(mouse_y + dy, 3, 60);

    for(int i=0; i<1024; i++) oled_buffer[i] = 0; // 画面メモリをすべてクリア（真っ黒にする）

    // --- 画面の基本フレーム描画 ---
    draw_window(0, 0, 128, 64); // 外枠
    for(int i=0; i<128; i++) draw_pixel(i, 11); // 上のメニューバーとの区切り線

    // --- メニューバーの文字表示 ---
    if (menu_state >= 5) {
        draw_string(5, 3, "ESC"); draw_string(35, 3, "RUN"); draw_string(65, 3, "STP"); draw_string(95, 3, "CLR");
    } else if (menu_state == 4) {
        draw_string(5, 3, "ESC");
    } else {
        draw_string(5, 3, "SYS"); draw_string(35, 3, "LOAD");
    }

    // デスクトップ画面の時だけEEPROMの状態を表示
    if (menu_state < 4) {
        if (eeprom_ok) draw_string(114, 3, "OK");
        else draw_string(114, 3, "NG");
    }

    // --- トップバーの当たり判定とホバー（マウスを乗せたときの反転）処理 ---
    if (mouse_y < 11) {
        if (menu_state == 7 || menu_state == 8) { // ポップアップ中はESCで戻る
            if (clicked) { menu_state = 5; clicked = false; }
        } else if (menu_state == 5 || menu_state == 6) { // エディタ画面のメニュー
            if (mouse_x > 3 && mouse_x < 28) { invert_rect(4, 2, 23, 9); if (clicked) { menu_state = 0; clicked = false; } }
            if (mouse_x > 33 && mouse_x < 58) { invert_rect(34, 2, 23, 9); if (clicked && menu_state == 5) { menu_state = 6; seq_pc = 0; clicked = false; } }
            if (mouse_x > 63 && mouse_x < 88) { invert_rect(64, 2, 23, 9); if (clicked) { menu_state = 5; clicked = false; } }
            if (mouse_x > 93 && mouse_x < 118) { invert_rect(94, 2, 23, 9); if (clicked && menu_state == 5) { menu_state = 8; clicked = false; } }
        } else if (menu_state == 4) { // IOアプリのメニュー
            if (mouse_x > 3 && mouse_x < 28) { invert_rect(4, 2, 23, 9); if (clicked) { menu_state = 0; clicked = false; } }
        } else { // デスクトップのメニュー
            if (menu_state == 0) {
                if (mouse_x > 3 && mouse_x < 28) { invert_rect(4, 2, 23, 9); if (clicked) { menu_state = 1; clicked = false; } }
                if (mouse_x > 33 && mouse_x < 65) { invert_rect(34, 2, 30, 9); if (clicked) { menu_state = 2; clicked = false; } }
            }
        }
    }

    // --- 各画面の個別処理 ---
    if (menu_state == 1) { // SYS (システム) メニューが開いている時
        invert_rect(4, 2, 23, 9); draw_window(5, 11, 35, 25);
        draw_string(10, 15, "INFO"); draw_string(10, 25, "EXIT");
        if (mouse_x > 5 && mouse_x < 40 && mouse_y > 11 && mouse_y < 36) { // メニュー内のホバーとクリック
            if (mouse_y < 23) { invert_rect(7, 13, 31, 11); if (clicked) { menu_state = 3; clicked = false; } } 
            else { invert_rect(7, 23, 31, 11); if (clicked) { menu_state = 0; clicked = false; } }
        } else if (clicked) { menu_state = 0; clicked = false; } // 外枠クリックで閉じる
    } else if (menu_state == 2) { // LOAD (アプリ起動) メニューが開いている時
        invert_rect(34, 2, 30, 9); draw_window(35, 11, 35, 25);
        draw_string(40, 15, "LED"); draw_string(40, 25, "SED");
        if (mouse_x > 35 && mouse_x < 70 && mouse_y > 11 && mouse_y < 36) {
            if (mouse_y < 23) { invert_rect(37, 13, 31, 11); if (clicked) { menu_state = 4; clicked = false; } } 
            else { invert_rect(37, 23, 31, 11); if (clicked) { menu_state = 5; clicked = false; } }
        } else if (clicked) { menu_state = 0; clicked = false; }
    } else if (menu_state == 3) { // INFO (バージョン情報) 画面
        draw_window(24, 15, 80, 40);
        for(int i=24; i<104; i++) draw_pixel(i, 24);
        draw_string(28, 17, "SYSTEM"); draw_string(30, 30, "V1.00");
        draw_window(65, 42, 25, 11); draw_string(72, 44, "OK");
        if (mouse_y > 42 && mouse_y < 53) {
            if (mouse_x > 65 && mouse_x < 90) { invert_rect(66, 43, 23, 9); if (clicked) { menu_state = 0; clicked = false; } }
        }
    } else if (menu_state == 4) { // IO (ハードウェアテスト) 画面
        draw_window(10, 15, 108, 40);
        draw_string(15, 20, "LED");
        draw_window(38, 18, 17, 11); draw_string(41, 20, "ON");
        draw_window(60, 18, 23, 11); draw_string(63, 20, "OFF");
        
        if (mouse_y > 17 && mouse_y < 29) { // LEDボタンの処理
            if (mouse_x > 38 && mouse_x < 55) { invert_rect(39, 19, 15, 9); if (clicked) { led_state = true; GPIOC->BSHR = (1 << 0); } }
            if (mouse_x > 60 && mouse_x < 83) { invert_rect(61, 19, 21, 9); if (clicked) { led_state = false; GPIOC->BSHR = (1 << 16); } }
        } else {
            if (led_state) invert_rect(38, 18, 17, 11); else invert_rect(60, 18, 23, 11); // 現在の状態を反転表示
        }
        
        draw_string(15, 40, "SRV"); // サーボメーターの描画
        draw_pixel(38, 43); draw_pixel(37, 44); draw_pixel(36, 45); draw_pixel(37, 46); draw_pixel(38, 47);
        draw_pixel(94, 43); draw_pixel(95, 44); draw_pixel(96, 45); draw_pixel(95, 46); draw_pixel(94, 47);
        draw_window(42, 43, 48, 5);
        invert_rect(44, 44, (servo_val - 5) * 2, 3); // ゲージを塗りつぶす
        
        if (mouse_y > 38 && mouse_y < 52) { // サーボの＋−ボタン処理
            if (mouse_x > 32 && mouse_x < 42) { invert_rect(35, 42, 5, 7); if (clicked && servo_val > 5) { servo_val--; TIM1->CH3CVR = servo_val; } }
            if (mouse_x > 90 && mouse_x < 100) { invert_rect(92, 42, 5, 7); if (clicked && servo_val < 25) { servo_val++; TIM1->CH3CVR = servo_val; } }
        }
        if (clicked) clicked = false;
    } else if (menu_state == 5 || menu_state == 7 || menu_state == 8) { // SED (シーケンスエディタ) 画面
        // 画面の更新が必要な時だけEEPROMから4行分をキャッシュ（高速化の肝）
        if (cached_view != seq_view || cache_dirty) {
            for(int i=0; i<4; i++) {
                uint8_t step = seq_view + i;
                if(step < 50) {
                    seq_cache[i*2] = eeprom_read_byte(0x0500 + step * 2);
                    seq_cache[i*2+1] = eeprom_read_byte(0x0500 + step * 2 + 1);
                }
            }
            cached_view = seq_view;
            cache_dirty = false;
        }

        // エディタの枠線とパレットの描画
        draw_window(2, 13, 124, 50);
        for(int i=13; i<63; i++) draw_pixel(28, i); // 縦の区切り線
        draw_string(5, 16, "LED"); draw_string(5, 24, "SRV"); draw_string(5, 32, "DLY"); draw_string(5, 40, "END");

        // 左側パレットのホバー処理
        if (menu_state == 5 && mouse_x > 2 && mouse_x < 28 && mouse_y >= 13) {
            if (mouse_y > 15 && mouse_y < 23) invert_rect(3, 15, 24, 9);
            else if (mouse_y >= 23 && mouse_y < 31) invert_rect(3, 23, 24, 9);
            else if (mouse_y >= 31 && mouse_y < 39) invert_rect(3, 31, 24, 9);
            else if (mouse_y >= 39 && mouse_y < 47) invert_rect(3, 39, 24, 9);
        }
        
        // 右側のプログラムリストを4行分描画
        for (int i=0; i<4; i++) {
            uint8_t step = seq_view + i;
            if (step >= 50) break;
            uint8_t cmd = seq_cache[i * 2]; // キャッシュからコマンドを読み出す
            uint8_t val = seq_cache[i * 2 + 1]; // キャッシュから値を読み出す
            uint8_t y_pos = 16 + i * 10;
            
            // 行番号の文字列作成 ("01:" ～ "50:")
            char num_str[4] = { (char)('0' + (step + 1) / 10), (char)('0' + (step + 1) % 10), ':', 0 };
            draw_string(32, y_pos, num_str);

            // コマンドと値の表示
            if (cmd == 1) { draw_string(52, y_pos, "LED"); draw_string(76, y_pos, val==1?"ON ":"OFF"); }
            else if (cmd == 2) { draw_string(52, y_pos, "SRV"); draw_string(76, y_pos, val==5?"MIN":val==25?"MAX":"MID"); }
            else if (cmd == 3) { draw_string(52, y_pos, "DLY"); draw_string(76, y_pos, val==1?"0.1":val==5?"0.5":val==10?"1.0":"3.0"); }
            else if (cmd == 0) { draw_string(52, y_pos, "END"); draw_string(76, y_pos, "---"); }
            else { draw_string(52, y_pos, "---"); draw_string(76, y_pos, "---"); } // 0xFFなどの未設定時

            // 現在選択中の行を反転
            if (seq_sel == step) invert_rect(30, y_pos - 1, 80, 9);
        }

        // スクロールバーの描画
        draw_pixel(115, 18); draw_pixel(114, 19); draw_pixel(116, 19); // 上矢印
        for(int i=113; i<=117; i++) draw_pixel(i, 20); 
        for(int i=113; i<=117; i++) draw_pixel(i, 50); 
        draw_pixel(114, 51); draw_pixel(116, 51); draw_pixel(115, 52); // 下矢印

        // エディタ通常時の操作受付
        if (menu_state == 5) {
            if (mouse_x >= 110 && mouse_x < 124) { // スクロール操作
                if (mouse_y > 15 && mouse_y < 25) { invert_rect(112, 17, 9, 5); if (clicked && seq_view > 0) seq_view--; }
                if (mouse_y > 45 && mouse_y < 55) { invert_rect(112, 49, 9, 5); if (clicked && seq_view < 46) seq_view++; }
            }
            if (mouse_x >= 28 && mouse_x < 110 && mouse_y >= 15 && mouse_y < 55) { // リストクリック（行選択またはポップアップ起動）
                uint8_t hover_idx = (mouse_y - 15) / 10;
                invert_rect(30, 16 + hover_idx * 10 - 1, 80, 9); // ホバー表示
                if (clicked) {
                    if (seq_view + hover_idx < 50) {
                        if (seq_sel == seq_view + hover_idx) menu_state = 7; // すでに選択中ならポップアップを開く
                        else seq_sel = seq_view + hover_idx; // 違う行なら選択移動
                    }
                }
            }

            if (clicked && mouse_y >= 13) {
                if (mouse_x > 2 && mouse_x < 28) { // 左パレットからのコマンド入力
                    uint8_t new_cmd = 255;
                    uint8_t def_val = 0;
                    if (mouse_y > 15 && mouse_y < 23) { new_cmd = 1; def_val = 1; }
                    else if (mouse_y > 23 && mouse_y < 31) { new_cmd = 2; def_val = 15; }
                    else if (mouse_y > 31 && mouse_y < 39) { new_cmd = 3; def_val = 10; }
                    else if (mouse_y > 39 && mouse_y < 47) { new_cmd = 0; def_val = 0; }
                    
                    if (new_cmd != 255) {
                        eeprom_write_byte(0x0500 + seq_sel * 2, new_cmd); // コマンドをEEPROMへ保存
                        eeprom_write_byte(0x0500 + seq_sel * 2 + 1, def_val); // 初期値をEEPROMへ保存
                        cache_dirty = true; // キャッシュ更新を要求
                        if (new_cmd == 0) { // ENDコマンドなら次の行へ進むだけ
                            if (seq_sel < 49) seq_sel++;
                            if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3;
                        } else {
                            menu_state = 7; // それ以外なら値入力ポップアップを即座に開く
                        }
                    }
                }
                clicked = false;
            }
        }
        
        // --- ポップアップウィンドウ（値の入力）の処理 ---
        if (menu_state == 7) {
            clear_rect(20, 10, 88, 44);
            draw_window(20, 10, 88, 44);
            draw_string(45, 14, "SET VAL");
            
            uint8_t cmd = seq_cache[(seq_sel - seq_view) * 2]; // 現在編集中のコマンドを取得
            
            if (cmd == 1) { // LED用ポップアップ
                draw_window(30, 30, 26, 15); draw_string(35, 34, "ON");
                draw_window(70, 30, 26, 15); draw_string(72, 34, "OFF");
                if (mouse_y > 30 && mouse_y < 45) {
                    if (mouse_x > 30 && mouse_x < 56) { invert_rect(31, 31, 24, 13); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 1); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                    else if (mouse_x > 70 && mouse_x < 96) { invert_rect(71, 31, 24, 13); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 0); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                }
            } else if (cmd == 2) { // サーボ用ポップアップ
                draw_window(24, 30, 23, 15); draw_string(26, 34, "MIN");
                draw_window(52, 30, 23, 15); draw_string(54, 34, "MID");
                draw_window(80, 30, 23, 15); draw_string(82, 34, "MAX");
                if (mouse_y > 30 && mouse_y < 45) {
                    if (mouse_x > 24 && mouse_x < 47) { invert_rect(25, 31, 21, 13); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 5); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                    else if (mouse_x > 52 && mouse_x < 75) { invert_rect(53, 31, 21, 13); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 15); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                    else if (mouse_x > 80 && mouse_x < 103) { invert_rect(81, 31, 21, 13); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 25); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                }
            } else if (cmd == 3) { // 待機(DLY)用ポップアップ
                draw_window(24, 25, 36, 12); draw_string(29, 27, "0.1s");
                draw_window(66, 25, 36, 12); draw_string(71, 27, "0.5s");
                draw_window(24, 39, 36, 12); draw_string(29, 41, "1.0s");
                draw_window(66, 39, 36, 12); draw_string(71, 41, "3.0s");
                
                if (mouse_x > 24 && mouse_x < 60 && mouse_y > 25 && mouse_y < 37) { invert_rect(25, 26, 34, 10); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 1); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                else if (mouse_x > 24 && mouse_x < 60 && mouse_y > 39 && mouse_y < 51) { invert_rect(25, 40, 34, 10); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 10); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                else if (mouse_x > 66 && mouse_x < 102 && mouse_y > 25 && mouse_y < 37) { invert_rect(67, 26, 34, 10); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 5); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
                else if (mouse_x > 66 && mouse_x < 102 && mouse_y > 39 && mouse_y < 51) { invert_rect(67, 40, 34, 10); if (clicked) { eeprom_write_byte(0x0500 + seq_sel * 2 + 1, 30); cache_dirty = true; menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; } }
            } else { // 設定不要なコマンドの場合
                draw_string(42, 30, "NO PARAM");
                if (clicked) { menu_state = 5; if (seq_sel < 49) seq_sel++; if (seq_sel >= seq_view + 4) seq_view = seq_sel - 3; }
            }
            if (clicked && (mouse_x < 20 || mouse_x > 108 || mouse_y < 10 || mouse_y > 54)) { menu_state = 5; } // 枠外クリックでキャンセル
            clicked = false;
        }

        // --- 全消去の確認ポップアップ ---
        if (menu_state == 8) {
            clear_rect(14, 15, 100, 35);
            draw_window(14, 15, 100, 35);
            draw_string(30, 20, "CLEAR ALL?");
            
            draw_window(25, 32, 30, 13); draw_string(30, 35, "YES");
            draw_window(70, 32, 25, 13); draw_string(74, 35, "NO");
            
            if (mouse_y > 32 && mouse_y < 45) {
                if (mouse_x > 25 && mouse_x < 55) {
                    invert_rect(26, 33, 28, 11);
                    if (clicked) {
                        for(int i=0; i<50; i++) { // 全50行を消去データ(0xFF)で埋める
                            eeprom_write_byte(0x0500 + i * 2, 255);
                            eeprom_write_byte(0x0500 + i * 2 + 1, 255);
                        }
                        cache_dirty = true;
                        menu_state = 5;
                    }
                } else if (mouse_x > 70 && mouse_x < 95) {
                    invert_rect(71, 33, 23, 11); // NOを選択
                    if (clicked) menu_state = 5;
                }
            }
            if (clicked && (mouse_x < 14 || mouse_x > 114 || mouse_y < 15 || mouse_y > 50)) { menu_state = 5; } // 枠外クリックでキャンセル
            clicked = false;
        }
    } else if (menu_state == 6) { // RUN (シーケンスプログラム実行) モード
        invert_rect(34, 2, 23, 9); // 上部メニューのRUNをハイライト
        draw_window(24, 25, 80, 14);
        draw_string(34, 28, "RUNNING...");
        
        static uint32_t seq_last_time = 0;
        static bool wait_active = false;
        static uint32_t wait_time = 0;

        if (!wait_active) {
            uint8_t cmd = eeprom_read_byte(0x0500 + seq_pc * 2); // 実行するコマンドを取得
            uint8_t val = eeprom_read_byte(0x0500 + seq_pc * 2 + 1); // 実行する値を取得
            
            if (cmd == 1) { // LED制御
                led_state = (val == 1); 
                if (led_state) GPIOC->BSHR = (1 << 0); else GPIOC->BSHR = (1 << 16); 
                seq_pc++; 
            }
            else if (cmd == 2) { // サーボ制御
                servo_val = val; 
                TIM1->CH3CVR = servo_val; 
                seq_pc++; 
            }
            else if (cmd == 3) { // 待機(DLY)開始
                wait_active = true; 
                seq_last_time = millis(); // 待機開始時刻を記録
                wait_time = val * 100; // 値(1=0.1秒)をミリ秒に変換
            }
            else if (cmd == 255) { // 白紙(0xFF)行は無視して進む
                seq_pc++; 
            }
            else { menu_state = 5; } // END (0) または未知のコマンドでエディタに戻る
            
            if (seq_pc >= 50) menu_state = 5; // 50行目まで到達したら終了
        } else {
            if (millis() - seq_last_time >= wait_time) { wait_active = false; seq_pc++; } // 指定時間経過で次へ
        }

        if (clicked && mouse_y >= 11) { menu_state = 5; wait_active = false; clicked = false; } // 画面クリックで強制停止
    }

    draw_cursor(mouse_x, mouse_y); // 最後にマウスカーソルを一番上に重ねて描画
    oled_update_full(); // 構築した1画面分のデータをOLEDに転送（ここで一瞬だけ通信する）
}
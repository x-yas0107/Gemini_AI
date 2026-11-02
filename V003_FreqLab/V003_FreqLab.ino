/********************************** (C) COPYRIGHT *******************************
 * File Name          : CH32V003_Counter.ino
 * Author             : yas & Gemini
 * Version            : V0.23
 * Description        : CH32V003A4M6 High-Precision Frequency Counter
 * 
 * Change History     :
 * V0.01 (2026-10-24) : 1. ベースコード(CH32V006 Voltmeter V9.1.0)のOLED描画・斜体7セグ表示を継承して新規作成。
 *                      2. Arduino IDE (arduino_core_ch32) 環境に適合。
 *                      3. クロック切替機能(USE_EXTERNAL_CRYSTAL: 24MHz外部水晶 / 内蔵HSI)を追加。
 *                      4. TIM2外部クロック(PD4)によるパルスカウントとTIM1による1秒ゲート計測を実装。
 *                      5. 傾斜を維持したまま8桁＋オートレンジ単位(Hz/kHz/MHz)表示に対応。
 *                      6. Flash容量節約のためsprintfを排し軽量数値文字列変換を実装。
 * V0.02 (2026-10-24) : 1. OLED_Init内の初期化コマンド記述を1行ずつに分割しコピー欠けを防止。
 *                      2. arduino_core_ch32内部のTIM2_IRQHandlerとの重複エラーを回避するため、
 *                         TIM2_IRQHandlerを廃止し、TIM1(1ms周期×1000回=1秒ゲート)割り込み内で
 *                         TIM2の差分カウントおよびオーバーフローを一括集計する方式に変更。
 * V0.03 (2026-10-24) : 1. Clock_Config_Init内のクロック設定マクロをCH32V00x専用仕様に修正。
 *                         (RCC_PCLK1Config/RCC_PCLK2Configを削除し、RCC_PLLSource_HSE_MUL2を適用)
 * V0.04 (2026-10-24) : 1. CH32V003A4M6(SOP-16)のピン配置に合わせ、ゼロビート調整用の機能を追加。
 *                         (PC3: UPスイッチ, PC4: 1kHzスピーカー出力, PC6: DOWNスイッチ)
 *                      2. TIM1を0.5ms(2000Hz)周期に変更(2000回=1秒ゲート)し、調整モード時にPC4を反転させて
 *                         ゲート時間と完全連動した1000Hzトーンを出力する機能を実装。
 *                      3. UP+DOWN同時押しで調整モード移行・再度同時押しで内部Flash(最終ページ: 0x08003FC0)へ
 *                         補正値を保存し通常モードへ復帰する機能を実装。
 * V0.05 (2026-10-24) : 1. 外部水晶ピン設定を修正 (GPIO_Remap_PA1_2 を ENABLE に変更)。これが原因でHSEが起動せず全く動かなかった可能性が高い。
 *                      2. Flash保存を FLASH_ErasePage_Fast (64バイト) に変更。元の FLASH_ErasePage は1KB消去でコード破壊の危険あり。
 *                      3. HSE起動失敗時のコメントを追加。
 * V0.06 (2026-10-24) : 1. 周波数表示をオートレンジ＋右寄せ空白埋め＋小数点自動移動方式に変更。
 *                         ~999Hz: 空白右寄せ、1kHz~: 小数3桁、1MHz~: 小数6桁、10MHz~: 小数5桁、100MHz~: 小数4桁。
 *                      2. 100MHzまで表示可能に対応。
 * V0.07 (2026-10-24) : 1. FUNスイッチ追加を試行したが初期化が不安定になったため中止。
 * V0.08 (2026-10-24) : 1. 割り込み方式による0.5秒ビープを追加。メインループで回数をセットし、TIM1割り込みでトグル。
 *                      2. 安定動作を確認。V0.08として確定。
 * V0.09 (2026-10-24) : 1. 起動時のデバッグトグルを削除。
 *                      2. FUNスイッチ(PC7)を追加。1秒長押しで設定モード突入/復帰。
 *                      3. 判定はloop内のみ。音は割り込み方式を継続利用。
 * V0.10 (2026,10,25 08:50) : 1. USE_EXTERNAL_CRYSTALを0(内蔵HSIのみで動作)に変更。
 *                      水晶への切り替え処理が起動時に不安定で止まる現象の切り分けの結果、
 *                      内蔵クロック固定なら安定するため。周波数表示のズレは補正係数/調整モードで合わせる。
 *                      2. 水晶を使う場合は、切り替え手順の修正(次版以降)が必要。
 * V0.11 (2026,10,25 13:46) : 1. 水晶(HSE)への切り替え手順を修正(Clock_Config_Init)。
 *                      起動時はPLL(HSI×2)で動作中のため、HSIへ一旦戻す→PLL停止→HSE起動→PLL設定→
 *                      PLL起動→PLLへ切替、の順に変更。HSE起動失敗時は内蔵HSI×2のPLLに戻して継続。
 *                      2. USE_EXTERNAL_CRYSTALを1(水晶使用)に戻した。動かない場合は0に戻すとV0.10と同じ動作。
 * V0.12 (2026,10,25 13:48) : 1. 設定モード(CAL)の表示を変更。数字を周波数表示と同じ右詰めにし、
 *                      "CAL"の文字を右上に表示(従来は数字が左詰め、CALが右下)。
 *                      CALと数字の傾斜した上端が重ならないよう、数字を右端から少し左へ寄せている(CAL_DIGIT_SHIFTで調整可)。
 * V0.13-DBG (2026,10,25 13:52) : 切り分け用。V0.12にブザー(PC4)のカチカチ音による到達確認を追加(機能は変更なし)。
 *                      ①setup開始時=1回 ②クロック設定後=2回(水晶PLL)/3回(内蔵PLL)/4回(内蔵直結)
 *                      ③OLED表示後=5回 ④タイマー設定後=6回。どこまで鳴るかで止まった場所が分かる。
 * V0.14-DBG (2026,10,25 13:56) : 到達確認の音を「カチ」から「ピッ」(約1kHz前後のビープ)に変更。グループ間の間も長くした。
 * V0.15-DBG (2026,10,25 13:59) : setup後半の到達確認を細分化。低い音=前半(1回:開始、2〜4回:クロック結果)、
 *                      高い音=後半(1回:UI初期化後、2回:OLED初期化後、3回:初回表示後、4回:タイマー設定後)。
 * V0.16-DBG (2026,10,25 14:02) : OLED_Init内部も細分化。高い音=1回:UI初期化後、2回:OLEDピン設定後(Delay前)、
 *                      3回:Delay_Ms(100)の後、4回:OLED初期化完了後、5回:初回表示後、6回:タイマー設定後。
 * V0.17-DBG (2026,10,25 14:05) : V0.16の結果、OLED初期化内の Delay_Ms(100) から戻らないことが判明(水晶PLL 48MHz時)。
 *                      SysTickを使うDelay_Msをすべて、空ループ式の Wait_Ms に置き換えた。到達確認のビープは残してある。
 * V0.18 (2026,10,25 14:16) : 1. 到達確認用のデバッグビープ(Dbg_Click等)をすべて削除。
 *                      2. 設定モード(CAL)の表示を、タイマー周期値(12000など)から「出力音の周波数 1.000 kHz」表示に変更。
 *                         右下に単位 kHz を表示。数字は周波数表示と同じ右詰め、CALは右上(従来どおり)。
 * V0.19 (2026-10-25) : 1. 11番ピン(PD7/NRST)を内部プルアップ入力(GPIO_Mode_IPU)として初期化し、
 *                         外付けプルアップ抵抗なしでGNDへ落とすと即座にマイコンを再起動(NVIC_SystemReset)する機能を追加。
 * V0.20 (2026-10-26) : 1. 単位表示(kHz/MHz)の右端の'z'が画面外にはみ出て消えていた問題を修正(描画開始位置をx=110へ左シフト)。
 *                      2. 設定モード(CAL)にて、右上の"CAL"と右下の"kHz"の間に、1Hzを4分割した微調整位置を示す
 *                         4段の斜めステップバーグラフ(Draw_Cal_SubStep_Graph)を追加。
 *                      3. UP/DOWNボタン1回押しごとに斜めバーグラフが必ず1目盛り変化し、4回押しで1Hz変化するよう
 *                         調整ステップ(TIM1_CAL_STEP = 3)を設定。
 * V0.21 (2026-10-26) : 1. 設定モード(CAL)にて、1Hz未満の端数(00/25/50/75)を人目で識別できるよう、
 *                         すぐ右隣に背の低いミニ斜体7セグ(Draw_Digital_Digit_Small: 幅6×高さ18ドット・下揃え)で表示する機能を追加。
 * V0.22 (2026-10-26) : 1. 18MHz以上の高周波測定上限を拡大するため、TIM2外部クロック入力の自動1/4分周切替(プリスケーラ自動切替)機能を追加。
 *                         測定値が16MHzを超えると自動で1/4分周モード(TIM_ExtTRGPSC_DIV4・カウント値4倍換算)へ切り替え、
 *                         14MHz未満に下がると自動で通常モード(TIM_ExtTRGPSC_OFF・等倍)へ復帰するヒステリシス制御を実装。
 * V0.23 (2026-10-26) : 1. Arduino IDEプリプロセッサでのunterminated #ifエラーを防止するため、#if/#ifdefディレクティブ記述を整理。
 *                      2. 分周切替時のカウントクリア処理をCH32V003標準のTIM_Cmd制御に統一。
 *********************************************************************************/

#include "debug.h"
#include <stdlib.h>

// SysTickを使う Delay_Ms は、水晶PLL(48MHz)動作時にOLED初期化中から戻らなくなったため、
// 空ループによる待ち関数に置き換える。1msあたりの回数は SystemCoreClock から概算(1周≒8クロック)。
static void Wait_Ms(uint32_t ms) {
    uint32_t per_ms = SystemCoreClock / 8000;
    while (ms--) {
        for (volatile uint32_t d = 0; d < per_ms; d++);
    }
}

/* =========================================================================
 * ★★★ 設定エリア (ここをいじって調整！) ★★★
 * ========================================================================= */
// ★基準クロック選択 (1: 外部水晶 24MHzを使用 / 0: 内蔵クロックを使用)
#define USE_EXTERNAL_CRYSTAL 1

// ★補正係数 (トリム / キャリブレーション)
// 周波数の表示がズレている場合、ここで調整します。
// 例: 表示が低い場合は 1.0005f とかに増やす。高い場合は 0.9995f とかに減らす。
// 計算式: (基準器の正しい周波数) ÷ (OLEDの表示周波数) = ここに入れる数字
#define CORRECTION_FACTOR  1.000000f 

// I2C Pin Settings (1番: PC1=SDA, 2番: PC2=SCL)
#define OLED_SCL_PIN   GPIO_Pin_2
#define OLED_SDA_PIN   GPIO_Pin_1
#define OLED_GPIO_PORT GPIOC
#define OLED_RCC_PORT  RCC_APB2Periph_GPIOC

// 信号入力ピン設定 (8番: PD4 = TIM2_ETR)
#define FREQ_IN_PIN    GPIO_Pin_4
#define FREQ_IN_PORT   GPIOD
#define FREQ_IN_RCC    RCC_APB2Periph_GPIOD

// ゼロビート調整用ピン設定 (3番: PC3=UP, 4番: PC4=SPK, 5番: PC6=DOWN, 6番: PC7=FUN)
#define SW_UP_PIN      GPIO_Pin_3
#define SPK_OUT_PIN    GPIO_Pin_4
#define SW_DN_PIN      GPIO_Pin_6
#define SW_FUN_PIN     GPIO_Pin_7
#define UI_GPIO_PORT   GPIOC
#define UI_RCC_PORT    RCC_APB2Periph_GPIOC

// リセットピン設定 (11番: PD7 = NRST / 内部プルアップ有効・ソフトウェアリセット対応)
#define SW_RST_PIN     GPIO_Pin_7
#define SW_RST_PORT    GPIOD
#define SW_RST_RCC     RCC_APB2Periph_GPIOD

// TIM1 0.5ms基準カウント値 (48MHz / 2 = 24MHz -> 24,000,000 / 2000Hz = 12000)
// 1HzあたりARR=12変化するため、1/4Hz(0.25Hz)ステップ = ARR 3刻み
#define TIM1_DEFAULT_ARR  12000
#define TIM1_MIN_ARR      10800
#define TIM1_MAX_ARR      13200
#define TIM1_CAL_STEP     3

// 高周波プリスケーラ自動切替しきい値 (16MHz超過で1/4分周ON、14MHz未満でOFF)
#define PRESCALER_ON_HZ   16000000UL
#define PRESCALER_OFF_HZ  14000000UL

// 内部Flash保存用アドレス (16KB Flashの最終64バイトページ)
#define FLASH_SAVE_ADDR   0x08003FC0
#define FLASH_MAGIC_CODE  0x5A5AA5A5

/* =========================================================================
 * グローバル変数 (タイマー計測・調整用)
 * ========================================================================= */
volatile uint32_t gate_half_ms_count = 0;
volatile uint16_t prev_tim2_cnt = 0;
volatile uint32_t pulse_accumulator = 0;
volatile uint32_t measured_raw_freq = 0;
volatile uint8_t  measurement_ready = 0;
volatile uint8_t  prescaler_div4 = 0;

volatile uint8_t  cal_mode = 0;
volatile uint16_t beep_count = 0;
volatile uint8_t  spk_state = 0;
volatile uint16_t tim1_arr_val = TIM1_DEFAULT_ARR;

/* =========================================================================
 * 内部Flash 読み書き処理 (補正値の保存・復元)
 * ========================================================================= */
void Load_Cal_Value(void) {
    uint32_t magic = *(__IO uint32_t*)(FLASH_SAVE_ADDR);
    uint32_t saved = *(__IO uint32_t*)(FLASH_SAVE_ADDR + 4);
    if (magic == FLASH_MAGIC_CODE && saved >= TIM1_MIN_ARR && saved <= TIM1_MAX_ARR) {
        // 1/4Hzステップ(TIM1_CAL_STEP=3)の倍数に揃える
        uint16_t rem = (uint16_t)(saved % TIM1_CAL_STEP);
        if (rem == 1) saved -= 1;
        else if (rem == 2) saved += 1;
        tim1_arr_val = (uint16_t)saved;
    } else {
        tim1_arr_val = TIM1_DEFAULT_ARR;
    }
}

void Save_Cal_Value(void) {
    // 64バイトページ用のFast Eraseを使用（1KB消去でコードを壊さないように）
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(FLASH_SAVE_ADDR);
    FLASH_ProgramWord(FLASH_SAVE_ADDR, FLASH_MAGIC_CODE);
    FLASH_ProgramWord(FLASH_SAVE_ADDR + 4, (uint32_t)tim1_arr_val);
    FLASH_Lock_Fast();
}

/* =========================================================================
 * OLED ドライバ
 * ========================================================================= */
u8 OLED_Buffer[128 * 32 / 8];

#define I2C_SCL_H  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SCL_PIN, Bit_SET)
#define I2C_SCL_L  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SCL_PIN, Bit_RESET)
#define I2C_SDA_H  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SDA_PIN, Bit_SET)
#define I2C_SDA_L  GPIO_WriteBit(OLED_GPIO_PORT, OLED_SDA_PIN, Bit_RESET)

void I2C_Delay(void) { volatile int i = 5; while(i--); }

void I2C_Start(void) {
    I2C_SDA_H; I2C_SCL_H; I2C_Delay();
    I2C_SDA_L; I2C_Delay(); I2C_SCL_L;
}

void I2C_Stop(void) {
    I2C_SDA_L; I2C_SCL_H; I2C_Delay();
    I2C_SDA_H; I2C_Delay();
}

void I2C_SendByte(u8 byte) {
    for(u8 i=0; i<8; i++) {
        if(byte & 0x80) I2C_SDA_H; else I2C_SDA_L;
        I2C_Delay(); I2C_SCL_H; I2C_Delay(); I2C_SCL_L;
        byte <<= 1;
    }
    I2C_SDA_H; I2C_Delay(); I2C_SCL_H; I2C_Delay(); I2C_SCL_L; 
}

void OLED_WriteCmd(u8 cmd) {
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x00); I2C_SendByte(cmd); I2C_Stop();
}

void OLED_WriteData(u8 data) {
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x40); I2C_SendByte(data); I2C_Stop();
}

void OLED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(OLED_RCC_PORT, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_Init(OLED_GPIO_PORT, &GPIO_InitStructure);
    
    I2C_SDA_H;
    I2C_SCL_H;
    Wait_Ms(100);

    OLED_WriteCmd(0xAE);
    OLED_WriteCmd(0xD5);
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8);
    OLED_WriteCmd(0x1F);
    OLED_WriteCmd(0xD3);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x8D);
    OLED_WriteCmd(0x14);
    OLED_WriteCmd(0x20);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xA1);
    OLED_WriteCmd(0xC8);
    OLED_WriteCmd(0xDA);
    OLED_WriteCmd(0x02);
    OLED_WriteCmd(0x81);
    OLED_WriteCmd(0x8F);
    OLED_WriteCmd(0xD9);
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0xAF);
}

void OLED_Clear(void) {
    for(int i=0; i<sizeof(OLED_Buffer); i++) OLED_Buffer[i] = 0x00;
}

void OLED_Refresh(void) {
    OLED_WriteCmd(0x21); OLED_WriteCmd(0); OLED_WriteCmd(127);
    OLED_WriteCmd(0x22); OLED_WriteCmd(0); OLED_WriteCmd(3);
    I2C_Start(); I2C_SendByte(0x78); I2C_SendByte(0x40);
    for(int i=0; i<sizeof(OLED_Buffer); i++) I2C_SendByte(OLED_Buffer[i]);
    I2C_Stop();
}

void OLED_DrawPixel(u8 x, u8 y) {
    if(x >= 128 || y >= 32) return;
    OLED_Buffer[x + (y / 8) * 128] |= (1 << (y % 8));
}

void OLED_DrawLine_Raw(u8 x1, u8 y1, u8 x2, u8 y2) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    while(1) {
        OLED_DrawPixel(x1, y1);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

/* =========================================================================
 * 描画ロジック
 * ========================================================================= */

void OLED_DrawPixel_Skewed(int x, int y) {
    int offset = (32 - y) / 3; 
    if (x + offset >= 128) return;
    OLED_DrawPixel(x + offset, y);
}

void Draw_Skewed_Rect(u8 x, u8 y, u8 w, u8 h) {
    for(u8 i=0; i<w; i++) {
        for(u8 j=0; j<h; j++) {
            OLED_DrawPixel_Skewed(x+i, y+j);
        }
    }
}

// ミニサイズ単位描画 (mode: 0="Hz", 1="kHz", 2="MHz", 3="CAL")
// x=110 で呼び出すと 3文字("kHz"/"MHz")が x=110..126 に収まり、'z'まで確実に表示される
void Draw_Unit_Freq_Mini(u8 x, u8 mode) {
    if (mode == 3) { // "CAL"
        // 'C'
        OLED_DrawLine_Raw(x, 20, x+4, 20);
        OLED_DrawLine_Raw(x, 20, x, 31);
        OLED_DrawLine_Raw(x, 31, x+4, 31);
        x += 6;
        // 'A'
        OLED_DrawLine_Raw(x, 20, x+4, 20);
        OLED_DrawLine_Raw(x, 20, x, 31);
        OLED_DrawLine_Raw(x+4, 20, x+4, 31);
        OLED_DrawLine_Raw(x, 25, x+4, 25);
        x += 6;
        // 'L'
        OLED_DrawLine_Raw(x, 20, x, 31);
        OLED_DrawLine_Raw(x, 31, x+4, 31);
        return;
    }

    if (mode == 0) {
        // "Hz" の場合は右端を揃えるため1文字分(6px)右へ寄せる
        x += 6;
    } else if (mode == 1) { // 'k'
        OLED_DrawLine_Raw(x, 20, x, 31);
        OLED_DrawLine_Raw(x+1, 27, x+4, 23);
        OLED_DrawLine_Raw(x+1, 27, x+4, 31);
        x += 6;
    } else if (mode == 2) { // 'M'
        OLED_DrawLine_Raw(x, 20, x, 31);
        OLED_DrawLine_Raw(x, 20, x+2, 25);
        OLED_DrawLine_Raw(x+4, 20, x+2, 25);
        OLED_DrawLine_Raw(x+4, 20, x+4, 31);
        x += 6;
    }

    // 'H'
    OLED_DrawLine_Raw(x, 20, x, 31);
    OLED_DrawLine_Raw(x+4, 20, x+4, 31);
    OLED_DrawLine_Raw(x, 25, x+4, 25);
    x += 6;

    // 'z'
    OLED_DrawLine_Raw(x, 23, x+4, 23);
    OLED_DrawLine_Raw(x+4, 23, x, 31);
    OLED_DrawLine_Raw(x, 31, x+4, 31);
}

// 設定モード用 "CAL" ラベル (右上表示: y=0..8, 文字は幅5×高さ9)
void Draw_Cal_Label(u8 x, u8 y) {
    // 'C'
    OLED_DrawLine_Raw(x, y, x+4, y);
    OLED_DrawLine_Raw(x, y, x, y+8);
    OLED_DrawLine_Raw(x, y+8, x+4, y+8);
    x += 6;
    // 'A'
    OLED_DrawLine_Raw(x, y, x+4, y);
    OLED_DrawLine_Raw(x, y, x, y+8);
    OLED_DrawLine_Raw(x+4, y, x+4, y+8);
    OLED_DrawLine_Raw(x, y+4, x+4, y+4);
    x += 6;
    // 'L'
    OLED_DrawLine_Raw(x, y, x, y+8);
    OLED_DrawLine_Raw(x, y+8, x+4, y+8);
}

// 設定モード用 1Hz内4分割の斜めステップバーグラフ ("CAL"と"kHz"の間 y=12..17 に描画)
// step: 0〜3 (ボタンを1回押すごとに1目盛り増減し、4目盛りで1Hz変化)
void Draw_Cal_SubStep_Graph(u8 x, u8 step) {
    for (u8 i = 0; i < 4; i++) {
        u8 bx = x + (i * 4);
        if (i <= step) {
            // 点灯バー: 右に行くほど背が高くなる斜めバー (高さ3, 4, 5, 6ドット)
            u8 h = 3 + i;
            for (u8 dy = 0; dy < h; dy++) {
                u8 py = 17 - dy;
                u8 skew = dy / 2;
                OLED_DrawPixel(bx + skew, py);
                OLED_DrawPixel(bx + 1 + skew, py);
            }
        } else {
            // 未点灯位置: ベースラインのドット(幅2×高さ1)を残して4分割位置を目視しやすくする
            OLED_DrawPixel(bx, 17);
            OLED_DrawPixel(bx + 1, 17);
        }
    }
}

/* =========================================================================
 * 7セグ描画 (8桁スリム・傾斜維持版 & 1Hz未満用小型版)
 * ========================================================================= */

void Draw_Digital_Digit(u8 x, u8 y, char c) {
    if (c < '0' || c > '9') return; 
    u8 num = c - '0';
    const u8 seg_map[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    u8 pat = seg_map[num];

    u8 w = 8;  // 8桁収めるためスリム化 (傾斜はそのまま)
    u8 h = 32;  
    u8 t = 2;   
    u8 g = 1;   

    if(pat & 0x01) Draw_Skewed_Rect(x+g, y, w-2*g, t);           
    if(pat & 0x02) Draw_Skewed_Rect(x+w-t, y+g, t, (h/2)-2*g);   
    if(pat & 0x04) Draw_Skewed_Rect(x+w-t, y+(h/2)+g, t, (h/2)-2*g); 
    if(pat & 0x08) Draw_Skewed_Rect(x+g, y+h-t, w-2*g, t);       
    if(pat & 0x10) Draw_Skewed_Rect(x, y+(h/2)+g, t, (h/2)-2*g); 
    if(pat & 0x20) Draw_Skewed_Rect(x, y+g, t, (h/2)-2*g);       
    if(pat & 0x40) Draw_Skewed_Rect(x+g, y+(h/2)-(t/2), w-2*g, t); 
}

// 1Hz未満の端数表示用ミニ斜体7セグ (幅6×高さ18ドット・下揃え)
void Draw_Digital_Digit_Small(u8 x, u8 y, char c) {
    if (c < '0' || c > '9') return; 
    u8 num = c - '0';
    const u8 seg_map[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    u8 pat = seg_map[num];

    u8 w = 6;  
    u8 h = 18;  
    u8 t = 2;   
    u8 g = 1;   

    if(pat & 0x01) Draw_Skewed_Rect(x+g, y, w-2*g, t);           
    if(pat & 0x02) Draw_Skewed_Rect(x+w-t, y+g, t, (h/2)-2*g);   
    if(pat & 0x04) Draw_Skewed_Rect(x+w-t, y+(h/2)+g, t, (h/2)-2*g); 
    if(pat & 0x08) Draw_Skewed_Rect(x+g, y+h-t, w-2*g, t);       
    if(pat & 0x10) Draw_Skewed_Rect(x, y+(h/2)+g, t, (h/2)-2*g); 
    if(pat & 0x20) Draw_Skewed_Rect(x, y+g, t, (h/2)-2*g);       
    if(pat & 0x40) Draw_Skewed_Rect(x+g, y+(h/2)-(t/2), w-2*g, t); 
}

// 軽量数値フォーマット関数 (sprintf代替)
void Format_Freq_String(uint32_t val, char* buf, u8* unit_mode) {
    // 仕様:
    // ~999 Hz          : "      60" Hz
    // 1.000~999.999 kHz: "   1.234" kHz
    // 1.000000~9.999999 MHz: "1.234567" MHz
    // 10.00000~99.99999 MHz: "12.34567" MHz
    // 100.0000 MHz~    : "100.0000" MHz
    // 右寄せ・空白埋め。数字部分は最大8文字程度。

    char temp[16];
    int i = 0;

    if (val < 1000) {
        *unit_mode = 0; // Hz
        // 空白埋め右寄せ 8桁
        for (int k = 0; k < 8; k++) temp[k] = ' ';
        int pos = 7;
        if (val == 0) {
            temp[7] = '0';
        } else {
            uint32_t v = val;
            while (v > 0 && pos >= 0) {
                temp[pos--] = (v % 10) + '0';
                v /= 10;
            }
        }
        temp[8] = '\0';
        strcpy(buf, temp);
        return;
    }

    if (val < 1000000) {
        *unit_mode = 1; // kHz  小数点以下3桁固定
        // 例: 1234 -> "   1.234"
        uint32_t integer = val / 1000;
        uint32_t frac = val % 1000;

        char work[16];
        int w = 0;
        // integer part
        if (integer == 0) {
            work[w++] = '0';
        } else {
            char it[8];
            int ii = 0;
            uint32_t iv = integer;
            while (iv > 0) {
                it[ii++] = (iv % 10) + '0';
                iv /= 10;
            }
            while (ii > 0) work[w++] = it[--ii];
        }
        work[w++] = '.';
        // 3桁固定
        work[w++] = (frac / 100) + '0';
        work[w++] = ((frac / 10) % 10) + '0';
        work[w++] = (frac % 10) + '0';
        work[w] = '\0';

        // 右寄せ空白埋め (全体8文字想定)
        int len = strlen(work);
        int pad = 8 - len;
        if (pad < 0) pad = 0;
        int p = 0;
        for (int k = 0; k < pad; k++) buf[p++] = ' ';
        strcpy(buf + p, work);
        return;
    }

    // MHz帯
    *unit_mode = 2;
    if (val < 10000000) {
        // 1.000000 ~ 9.999999 MHz  小数6桁
        uint32_t integer = val / 1000000;
        uint32_t frac = val % 1000000;

        char work[16];
        int w = 0;
        work[w++] = integer + '0';
        work[w++] = '.';
        for (int d = 5; d >= 0; d--) {
            uint32_t div = 1;
            for (int m = 0; m < d; m++) div *= 10;
            work[w++] = ((frac / div) % 10) + '0';
        }
        work[w] = '\0';
        strcpy(buf, work);
        return;
    } else if (val < 100000000) {
        // 10.00000 ~ 99.99999 MHz  小数5桁
        uint32_t integer = val / 1000000;
        uint32_t frac = (val % 1000000) / 10; // 5桁に

        char work[16];
        int w = 0;
        char it[8];
        int ii = 0;
        uint32_t iv = integer;
        while (iv > 0) {
            it[ii++] = (iv % 10) + '0';
            iv /= 10;
        }
        while (ii > 0) work[w++] = it[--ii];
        work[w++] = '.';
        for (int d = 4; d >= 0; d--) {
            uint32_t div = 1;
            for (int m = 0; m < d; m++) div *= 10;
            work[w++] = ((frac / div) % 10) + '0';
        }
        work[w] = '\0';
        strcpy(buf, work);
        return;
    } else {
        // 100.0000 MHz ~  小数4桁
        uint32_t integer = val / 1000000;
        uint32_t frac = (val % 1000000) / 100; // 4桁に

        char work[16];
        int w = 0;
        char it[8];
        int ii = 0;
        uint32_t iv = integer;
        if (iv == 0) work[w++] = '0';
        else {
            while (iv > 0) {
                it[ii++] = (iv % 10) + '0';
                iv /= 10;
            }
            while (ii > 0) work[w++] = it[--ii];
        }
        work[w++] = '.';
        for (int d = 3; d >= 0; d--) {
            uint32_t div = 1;
            for (int m = 0; m < d; m++) div *= 10;
            work[w++] = ((frac / div) % 10) + '0';
        }
        work[w] = '\0';
        strcpy(buf, work);
        return;
    }
}

void Display_Frequency(uint32_t freq) {
    char buf[16];
    u8 unit_mode = 0;
    OLED_Clear();

    Format_Freq_String(freq, buf, &unit_mode);

    // 右寄せ表示（空白はスキップ、数字と小数点を描画）
    int digit_width = 11;
    int len = strlen(buf);
    int start_x = 128 - 22 - (len * digit_width); // 単位("kHz"/"MHz" 17px)分を確保
    if (start_x < 0) start_x = 2;

    int cursor_x = start_x;
    int cursor_y = 0;

    for (int i = 0; i < len; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            Draw_Digital_Digit(cursor_x, cursor_y, buf[i]);
            cursor_x += digit_width;
        } else if (buf[i] == '.') {
            Draw_Skewed_Rect(cursor_x + 3, 29, 3, 3);
            cursor_x += 6;
        } else {
            // 空白
            cursor_x += digit_width;
        }
    }

    // 単位は右端固定 (x=110〜126)
    Draw_Unit_Freq_Mini(110, unit_mode);

    OLED_Refresh();
}

// ゼロビート調整モード時の画面表示 (出力音の周波数 + 4段斜めグラフ + ミニ下2桁 + CAL表示)
// ARR=12000 を 1000Hz(step 0) とし、TIM1_CAL_STEP(3) ごとに 1/4Hz(0.25Hz) ずつ増減
void Display_Cal_Value(uint16_t arr_val) {
    char buf[16];
    u8 unit_mode = 0;

    OLED_Clear();

    int32_t steps_from_default = ((int32_t)TIM1_DEFAULT_ARR - (int32_t)arr_val) / TIM1_CAL_STEP;
    int32_t quarter_hz = 4000 + steps_from_default;
    if (quarter_hz < 0) quarter_hz = 0;

    uint32_t tone_hz = (uint32_t)(quarter_hz / 4);
    u8 sub_step = (u8)(quarter_hz & 3);

    Format_Freq_String(tone_hz, buf, &unit_mode);

    // 右寄せ表示 (周波数表示と同じ方式。小型2桁分とCALラベルが重ならないよう CAL_DIGIT_SHIFT だけ左へ寄せる)
    const int CAL_DIGIT_SHIFT = 18;
    int digit_width = 11;
    int len = strlen(buf);
    int cursor_x = 128 - 16 - CAL_DIGIT_SHIFT - (len * digit_width);
    if (cursor_x < 0) cursor_x = 2;
    int cursor_y = 0;

    for (int i = 0; i < len; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            Draw_Digital_Digit(cursor_x, cursor_y, buf[i]);
            cursor_x += digit_width;
        } else if (buf[i] == '.') {
            Draw_Skewed_Rect(cursor_x + 3, 29, 3, 3);
            cursor_x += 6;
        } else {
            cursor_x += digit_width;   // 空白
        }
    }

    // 1Hz未満の端数(00/25/50/75)をミニ斜体7セグで描画 (下揃え y=14)
    char sub_buf[3];
    uint32_t frac2 = sub_step * 25;
    sub_buf[0] = (frac2 / 10) + '0';
    sub_buf[1] = (frac2 % 10) + '0';
    sub_buf[2] = '\0';

    int small_width = 7;
    int small_y = 14;
    for (int i = 0; i < 2; i++) {
        Draw_Digital_Digit_Small(cursor_x, small_y, sub_buf[i]);
        cursor_x += small_width;
    }

    Draw_Cal_Label(110, 0);                   // "CAL" (右上: y=0..8)
    Draw_Cal_SubStep_Graph(110, sub_step);    // 4段斜めグラフ (中段: y=12..17)
    Draw_Unit_Freq_Mini(110, unit_mode);      // 単位 "kHz" (右下: y=20..31)
    OLED_Refresh();
}

/* =========================================================================
 * スイッチ＆スピーカー初期化・操作処理
 * ========================================================================= */
void UI_Pins_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(UI_RCC_PORT | SW_RST_RCC, ENABLE);

    // PC3 (UP), PC6 (DOWN), PC7 (FUN) 内部プルアップ入力
    GPIO_InitStructure.GPIO_Pin = SW_UP_PIN | SW_DN_PIN | SW_FUN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UI_GPIO_PORT, &GPIO_InitStructure);

    // PC4 (SPK) プッシュプル出力
    GPIO_InitStructure.GPIO_Pin = SPK_OUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UI_GPIO_PORT, &GPIO_InitStructure);

    GPIO_WriteBit(UI_GPIO_PORT, SPK_OUT_PIN, Bit_RESET);

    // PD7 (RESET) 内部プルアップ入力 (外付けプルアップ抵抗不要)
    GPIO_InitStructure.GPIO_Pin = SW_RST_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SW_RST_PORT, &GPIO_InitStructure);
}

void Check_Switches(void) {
    // PD7 (リセットピン) がGNDに落とされたらマイコンを再起動
    if (GPIO_ReadInputDataBit(SW_RST_PORT, SW_RST_PIN) == Bit_RESET) {
        Wait_Ms(20);
        if (GPIO_ReadInputDataBit(SW_RST_PORT, SW_RST_PIN) == Bit_RESET) {
            NVIC_SystemReset();
        }
    }

    uint8_t up_pressed = (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_UP_PIN) == Bit_RESET);
    uint8_t dn_pressed = (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_DN_PIN) == Bit_RESET);
    uint8_t fun_pressed = (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_FUN_PIN) == Bit_RESET);

    if (!up_pressed && !dn_pressed && !fun_pressed) return;

    Wait_Ms(25); // チャタリング防止
    up_pressed = (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_UP_PIN) == Bit_RESET);
    dn_pressed = (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_DN_PIN) == Bit_RESET);
    fun_pressed = (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_FUN_PIN) == Bit_RESET);

    // FUN 1秒長押し: 設定モード切替
    if (fun_pressed) {
        uint32_t hold = 0;
        while (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_FUN_PIN) == Bit_RESET) {
            Wait_Ms(10);
            hold += 10;
            if (hold >= 1000) break;
        }
        if (hold >= 1000) {
            if (cal_mode == 0) {
                cal_mode = 1;
                Display_Cal_Value(tim1_arr_val);
            } else {
                cal_mode = 0;
                GPIO_WriteBit(UI_GPIO_PORT, SPK_OUT_PIN, Bit_RESET);
                Save_Cal_Value();
                // 確認表示 1.000 kHz を0.5秒
                Display_Frequency(1000);
                Wait_Ms(500);
                Display_Frequency(0);
            }
            // ボタンが離されるまで待機
            while (GPIO_ReadInputDataBit(UI_GPIO_PORT, SW_FUN_PIN) == Bit_RESET) {
                Wait_Ms(10);
            }
            Wait_Ms(100);
            return;
        }
    }

    // 設定モード中の単押し操作 (UP/DOWN: 1押しで斜めバー1目盛り=0.25Hz変化、4押しで1Hz変化)
    if (cal_mode == 1) {
        if (up_pressed) {
            // UPボタン: 音程・ゲート周波数を上げる (ARRをTIM1_CAL_STEP減らす)
            if (tim1_arr_val >= (TIM1_MIN_ARR + TIM1_CAL_STEP)) {
                tim1_arr_val -= TIM1_CAL_STEP;
                TIM_SetAutoreload(TIM1, tim1_arr_val - 1);
            }
            Display_Cal_Value(tim1_arr_val);
            Wait_Ms(80);
        } else if (dn_pressed) {
            // DOWNボタン: 音程・ゲート周波数を下げる (ARRをTIM1_CAL_STEP増やす)
            if (tim1_arr_val <= (TIM1_MAX_ARR - TIM1_CAL_STEP)) {
                tim1_arr_val += TIM1_CAL_STEP;
                TIM_SetAutoreload(TIM1, tim1_arr_val - 1);
            }
            Display_Cal_Value(tim1_arr_val);
            Wait_Ms(80);
        }
    }
}

/* =========================================================================
 * クロック・タイマー設定 (高精度周波数カウント用)
 * ========================================================================= */

void Clock_Config_Init(void) {
    if (USE_EXTERNAL_CRYSTAL) {
        // 起動直後は、コアの初期化(SystemInit)により「内蔵HSI×2のPLL=48MHz」で動いている。
        // PLLを使用中のままPLLの入力元を変えると失敗するため、
        // 「HSIへ切替 → PLL停止 → HSE起動 → PLL設定 → PLL再起動 → PLLへ切替」の順で行う。
        volatile uint32_t timeout;

        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        GPIO_PinRemapConfig(GPIO_Remap_PA1_2, ENABLE);   // PA1/PA2を水晶ピンとして使用

        // 手順1: HSIを確実に起動し、システムクロックをHSI直結へ切り替える
        RCC->CTLR |= RCC_HSION;
        timeout = 0;
        while (!(RCC->CTLR & RCC_HSIRDY)) {
            if (++timeout > 0x20000) break;
        }
        RCC->CFGR0 = (RCC->CFGR0 & ~RCC_SW) | RCC_SW_HSI;
        timeout = 0;
        while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI) {
            if (++timeout > 0x20000) break;
        }

        // 手順2: PLL停止 (停止中でないとPLLの入力元は変更できない)
        RCC->CTLR &= ~RCC_PLLON;

        // 手順3: HSE(水晶)起動待ち
        RCC->CTLR |= RCC_HSEON;
        timeout = 0;
        while (!(RCC->CTLR & RCC_HSERDY)) {
            if (++timeout > 0x80000) break;
        }

        // 48MHzで動かすためFlashの待ち時間を1に設定 (速度を上げる前に設定する)
        FLASH->ACTLR = (FLASH->ACTLR & ~FLASH_ACTLR_LATENCY) | FLASH_ACTLR_LATENCY_1;
        RCC->CFGR0 &= ~RCC_HPRE;                          // AHB分周なし

        if (RCC->CTLR & RCC_HSERDY) {
            RCC->CFGR0 |= RCC_PLLSRC_HSE_Mul2;            // PLL入力: HSE×2 = 48MHz
        } else {
            // HSE起動失敗: 内蔵HSI×2(48MHz)に戻して動作を継続する
            RCC->CFGR0 &= ~RCC_PLLSRC;
            RCC->CTLR &= ~RCC_HSEON;
        }

        // 手順4: PLL再起動 → ロック待ち → システムクロックをPLLへ切り替え
        RCC->CTLR |= RCC_PLLON;
        timeout = 0;
        while (!(RCC->CTLR & RCC_PLLRDY)) {
            if (++timeout > 0x20000) break;
        }
        if (RCC->CTLR & RCC_PLLRDY) {
            RCC->CFGR0 = (RCC->CFGR0 & ~RCC_SW) | RCC_SW_PLL;
            timeout = 0;
            while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_PLL) {
                if (++timeout > 0x20000) break;
            }
        }
    }
    SystemCoreClockUpdate();
}

void Counter_Timers_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(FREQ_IN_RCC | RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // PD4 (TIM2_ETR) 入力ピン設定
    GPIO_InitStructure.GPIO_Pin = FREQ_IN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FREQ_IN_PORT, &GPIO_InitStructure);

    // TIM2設定: 外部クロックモード2 (PD4からのパルスをカウント / 割り込み不使用)
    TIM_DeInit(TIM2);
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    prescaler_div4 = 0;
    TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0);

    // TIM1設定: 0.5ms(2000Hz)周期タイマー (48MHz / 2 / 12000 = 2000Hz) -> 2000回で1秒ゲート
    TIM_DeInit(TIM1);
    TIM_TimeBaseStructure.TIM_Period = tim1_arr_val - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 2 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_ClearFlag(TIM1, TIM_FLAG_Update);
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

    // TIM1 割り込み優先度設定
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // カウント開始
    TIM_SetCounter(TIM2, 0);
    TIM_SetCounter(TIM1, 0);
    prev_tim2_cnt = 0;
    pulse_accumulator = 0;
    gate_half_ms_count = 0;

    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

// TIM1 0.5ms周期割り込み (2000回＝1秒ゲートでTIM2のパルス数を確定 ＆ 調整時は1kHz出力)
extern "C" void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" void TIM1_UP_IRQHandler(void) {
    if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET) {
        uint16_t curr_cnt = TIM_GetCounter(TIM2);
        uint16_t diff = (uint16_t)(curr_cnt - prev_tim2_cnt);
        prev_tim2_cnt = curr_cnt;
        pulse_accumulator += diff;

        // 調整モード時は0.5msごとにPC4を反転させて1000Hzトーンを出力
        if (cal_mode || beep_count > 0) {
            spk_state ^= 1;
            if (spk_state) {
                UI_GPIO_PORT->BSHR = SPK_OUT_PIN;
            } else {
                UI_GPIO_PORT->BCR = SPK_OUT_PIN;
            }
            if (beep_count > 0) beep_count--;
        }

        gate_half_ms_count++;
        if (gate_half_ms_count >= 2000) {
            measured_raw_freq = pulse_accumulator;
            pulse_accumulator = 0;
            gate_half_ms_count = 0;
            measurement_ready = 1;
        }

        TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
    }
}

/* =========================================================================
 * メインルーチン (Arduino setup / loop)
 * ========================================================================= */

void setup(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    Clock_Config_Init();
    Delay_Init();
    
    Load_Cal_Value();
    UI_Pins_Init();

    OLED_Init(); 
    OLED_Clear();
    Display_Frequency(0);

    Counter_Timers_Init();
}

void loop(void) {
    static uint8_t beeped = 0;
    if (!beeped) {
        beeped = 1;
        beep_count = 1000;  // 0.5秒分 (0.5ms × 1000 = 500ms)
    }

    Check_Switches();

    if (measurement_ready) {
        measurement_ready = 0;

        uint32_t raw_hz = measured_raw_freq;
        if (prescaler_div4) {
            raw_hz *= 4UL;
        }

        if (cal_mode == 0) {
            // ★ここで補正係数を掛ける！
            float corrected = (float)raw_hz * CORRECTION_FACTOR;
            uint32_t final_freq = (uint32_t)(corrected + 0.5f);
            
            Display_Frequency(final_freq);
        }

        // 高周波プリスケーラ自動切替 (16MHz超過で1/4分周ON、14MHz未満で通常モードへ復帰)
        if (!prescaler_div4 && raw_hz > PRESCALER_ON_HZ) {
            TIM_Cmd(TIM1, DISABLE);
            prescaler_div4 = 1;
            TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_DIV4, TIM_ExtTRGPolarity_NonInverted, 0);
            prev_tim2_cnt = TIM_GetCounter(TIM2);
            pulse_accumulator = 0;
            gate_half_ms_count = 0;
            measurement_ready = 0;
            TIM_Cmd(TIM1, ENABLE);
        } else if (prescaler_div4 && raw_hz < PRESCALER_OFF_HZ) {
            TIM_Cmd(TIM1, DISABLE);
            prescaler_div4 = 0;
            TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0);
            prev_tim2_cnt = TIM_GetCounter(TIM2);
            pulse_accumulator = 0;
            gate_half_ms_count = 0;
            measurement_ready = 0;
            TIM_Cmd(TIM1, ENABLE);
        }
    }
}

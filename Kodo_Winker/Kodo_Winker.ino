/*******************************************************************************
 * File Name    : main.c
 * Version      : 0.53
 * Date         : 2026/07/26
 * Description  : 魂動ウインカー（Kodo Winker）制御プログラム（二次曲線カーブ導入版）
 * Change Hist  : 
 *   v0.01 - 新規作成。2つのボリュームによるPWM制御、状態遷移の基本骨格を実装。
 *   v0.05 - ボリューム仕様変更。周期全体の時間軸からPWM値を動的に計算（DT60%、傾き可変）方式へ刷新。
 *   v0.33 - ADC制御を完全廃止し、3つの物理スイッチによるデジタル設定方式へ抜本的変更。
 *   v0.34 - EEPROM保存機能を一時削除。ボタン動作のロジック検証に特化。
 *   v0.35 - delay()を完全排除し、millis()によるノンブロッキング処理へ改修。
 *   v0.36 - ＋/－操作時の合図を150msの強制消灯＆リスタートに変更。
 *   v0.38 - SOP8特有の内部共有ピン構造に対応。入力判定をレジスタ監視に復元し、確実なボタン入力を実現。
 *   v0.39〜v0.42 - Pin3(選択スイッチ)の不具合切り分けを継続。
 *   v0.43 - 水晶発振アンプ(HSEON)を強制OFFにする対策を試行。
 *   v0.44 - AFIO->PCFR1のPA12_RMビット解除を追加。
 *   v0.45 - ＋/－スイッチの同時押しで選択切替する方式に一時変更。
 *   v0.46 - 真の原因判明。監視対象をPA2(bit2)に修正し、専用選択スイッチを復活。
 *   v0.47 - ボタンを離した瞬間のチャタリングによる「二重押し判定（誤作動）」を修正。
 *   v0.48 - フィードバック（合図）の点滅時間を延長。錯覚を防止するため前後に長めのOFF時間を確保。
 *   v0.50 - 安定動作のベースライン版として統合。ACTIVE_DT_PERCENTを90に変更し点灯時間を延長。
 *   v0.51 - 選択スイッチの3秒長押しによるフラッシュメモリへの設定保存機能を実装。保存成功時は3回点滅。
 *   v0.52 - 選択スイッチの短押し判定が厳しすぎた(人間の素早いタップをチャタリングとして無視した)問題を修正。
 *           sel_pressed_validフラグを新設し、素早い短押しと3秒の長押しを正しく切り分けるようロジックを刷新。
 *   v0.53 - PWM制御に二次曲線（ガンマ補正）を導入。白熱電球や魂動ウインカーのような、
 *           生命感のある滑らかな点灯・消灯カーブ（ジワッと点き、余韻を残して消える）を実装。
 *
 * --- I/O Map (CH32V003J4M6 - SOP8パッケージ) ---
 * Pin 1 : PD6/PC3/PA1 [デジタル入力] ウインカースイッチ (LOWでON / 内部プルアップ)
 *         ※PA1はPin1と同じ物理ピンに同居。干渉防止のためアナログ設定にして未使用扱い。
 * Pin 2 : VSS [電源] GND
 * Pin 3 : PA2 [デジタル入力] 選択スイッチ (短押し:周期/傾き切替, 長押し:フラッシュ保存)
 * Pin 4 : VDD [電源] +3.3V または +5.0V
 * Pin 5 : PD5/PD3/PC6/PC1 [デジタル入力] ＋（プラス）スイッチ (設定値UP)
 * Pin 6 : PD4/PD2/PC7/PC2 [デジタル入力] －（マイナス）スイッチ (設定値DOWN)
 * Pin 7 : PC4 [PWM出力] TIM1_CH4 : LED駆動用出力 (HIGHで点灯)
 * Pin 8 : PD1 [システム] SWIO (プログラム書き込み・デバッグ用)
 *******************************************************************************/

#include "ch32v00x.h"
#include <Arduino.h>

/* ==========================================================================
 * パラメータの仕様設定
 * ========================================================================== */

// --- 点滅周期の設定範囲 (10段階: 0〜9) ---
#define PERIOD_MIN_MS      500   // 最短周期 (ミリ秒)
#define PERIOD_MAX_MS      1500  // 最長周期 (ミリ秒)

// --- 傾き（スローアップ割合）の設定範囲 (10段階: 0〜9) ---
#define SLOPE_UP_MIN       10    // 最小スローアップ割合 (%)
#define SLOPE_UP_MAX       60    // 最大スローアップ割合 (%)

// --- 点滅全体における有効時間の割り当て ---
#define ACTIVE_DT_PERCENT  90    // 1周期のうち、LEDを光らせる割合 (%)

// --- LEDの明るさ上限 ---
#define PWM_MAX_VAL        1000  // 最大の明るさ

// --- フラッシュメモリ保存用設定 ---
#define FLASH_SAVE_ADDR    0x08003FC0 // 16KB(0x4000)の最後のページ(64byte)の先頭アドレス
#define FLASH_MAGIC_NUM    0xA5       // データ有効性確認用のマジックナンバー

/* ========================================================================== */

// デジタル設定用のグローバル変数
uint8_t period_step = 5; // 周期の現在ステップ (初期値5: 中央)
uint8_t slope_step = 5;  // 傾きの現在ステップ (初期値5: 中央)
uint8_t edit_mode = 0;   // 現在の操作対象 (0: 周期, 1: 傾き)

// フィードバック（合図）用の変数
uint8_t fb_mode = 0;         // 0:通常, 1:周期切替, 2:傾き切替, 3:設定変更, 4:セーブ完了
uint32_t fb_start_time = 0;  // フィードバック開始時間

// 時間管理用の変数
uint32_t start_time = 0; 

// LEDの明るさをセットする関数
void Set_PWM_Duty(uint16_t duty) {
    if(duty > PWM_MAX_VAL) duty = PWM_MAX_VAL;
    TIM1->CH4CVR = duty;
}

// フラッシュメモリへの設定保存関数
void Save_Settings_To_Flash(void) {
    FLASH_Unlock();
    FLASH_ErasePage(FLASH_SAVE_ADDR);
    // 上位8bitにマジックナンバー、下位8bitにperiod_stepとslope_step(各4bit)をパック
    uint16_t save_data = (FLASH_MAGIC_NUM << 8) | ((period_step & 0x0F) << 4) | (slope_step & 0x0F);
    FLASH_ProgramHalfWord(FLASH_SAVE_ADDR, save_data);
    FLASH_Lock();
}

// フラッシュメモリからの設定読み込み関数
void Load_Settings_From_Flash(void) {
    uint16_t saved_data = *(volatile uint16_t*)FLASH_SAVE_ADDR;
    if ((saved_data >> 8) == FLASH_MAGIC_NUM) {
        period_step = (saved_data >> 4) & 0x0F;
        slope_step = saved_data & 0x0F;
        
        // 異常値の場合はデフォルト値に戻す
        if (period_step > 9) period_step = 5;
        if (slope_step > 9) slope_step = 5;
    }
}

// 初期設定：ピンの役割を決める関数（SOP8共有ピン対応）
void GPIO_Config(void) {
    // 【重要】AFIO_PCFR1のPA12_RMビット(bit15)を0にクリアする。
    AFIO->PCFR1 &= ~(1 << 15);

    // 【重要】外部オシレータ(HSEON)を強制OFFにする
    RCC->CTLR &= ~(1 << 16);

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1;

    // Pin 1 (PD6/PC3) : ウインカースイッチ
    GPIOD->CFGLR &= ~(0xF << (4 * 6)); GPIOD->CFGLR |= (0x8 << (4 * 6)); GPIOD->OUTDR |= (1 << 6);
    GPIOC->CFGLR &= ~(0xF << (4 * 3)); GPIOC->CFGLR |= (0x8 << (4 * 3)); GPIOC->OUTDR |= (1 << 3);

    // Pin 1に同居するPA1はアナログ設定にして干渉を防ぐ(未使用)
    GPIOA->CFGLR &= ~(0xF << (4 * 1));

    // Pin 3 (PA2) : 選択スイッチ（プルアップ入力）
    GPIOA->CFGLR &= ~(0xF << (4 * 2));
    GPIOA->CFGLR |= (0x8 << (4 * 2));
    GPIOA->OUTDR |= (1 << 2);

    // Pin 5 & Pin 6 : ＋スイッチ / －スイッチ
    // GPIOD: PD2, PD3, PD4, PD5
    uint32_t d_mask = (0xF << (4 * 2)) | (0xF << (4 * 3)) | (0xF << (4 * 4)) | (0xF << (4 * 5));
    uint32_t d_bits = (0x8 << (4 * 2)) | (0x8 << (4 * 3)) | (0x8 << (4 * 4)) | (0x8 << (4 * 5));
    GPIOD->CFGLR &= ~d_mask; GPIOD->CFGLR |= d_bits;
    GPIOD->OUTDR |= (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5);

    // GPIOC: PC1, PC2, PC6, PC7
    uint32_t c_mask = (0xF << (4 * 1)) | (0xF << (4 * 2)) | (0xF << (4 * 6)) | (0xF << (4 * 7));
    uint32_t c_bits = (0x8 << (4 * 1)) | (0x8 << (4 * 2)) | (0x8 << (4 * 6)) | (0x8 << (4 * 7));
    GPIOC->CFGLR &= ~c_mask; GPIOC->CFGLR |= c_bits;
    GPIOC->OUTDR |= (1 << 1) | (1 << 2) | (1 << 6) | (1 << 7);

    // Pin 7 (PC4) : PWM出力
    GPIOC->CFGLR &= ~(0xF << (4 * 4)); GPIOC->CFGLR |= (0xB << (4 * 4));
}

// 初期設定：PWMの設定
void TIM1_PWM_Config(void) {
    TIM1->PSC = 48 - 1;
    TIM1->ATRLR = PWM_MAX_VAL - 1;
    
    TIM1->CHCTLR2 &= ~(0x7 << 12);
    TIM1->CHCTLR2 |= (0x6 << 12);
    TIM1->CHCTLR2 |= (1 << 11);
    
    TIM1->CCER |= (1 << 12);
    TIM1->BDTR |= (1 << 15);
    TIM1->CTLR1 |= (1 << 0);
}

// 物理スイッチの入力を処理する関数（SOP8共有ピン対応）
void Process_Buttons(uint32_t current_time) {
    static uint8_t last_btn_sel = 0;
    static uint8_t last_btn_up = 0;
    static uint8_t last_btn_dn = 0;
    
    static uint32_t sel_press_time = 0;
    static uint32_t sel_release_time = 0;
    static uint32_t up_press_time = 0;
    static uint32_t dn_press_time = 0;
    
    static uint8_t long_press_saved = 0;
    static uint8_t sel_pressed_valid = 0;

    // Pin 3 (選択スイッチ) は PA2 (bit2) を監視する
    uint8_t btn_sel = ((GPIOA->INDR & (1 << 2)) == 0);
    
    // Pin 5 / Pin 6 は共有されている全てのレジスタを監視
    uint8_t btn_up  = ((GPIOD->INDR & (1 << 5)) == 0) || ((GPIOD->INDR & (1 << 3)) == 0) || ((GPIOC->INDR & (1 << 6)) == 0) || ((GPIOC->INDR & (1 << 1)) == 0);
    uint8_t btn_dn  = ((GPIOD->INDR & (1 << 4)) == 0) || ((GPIOD->INDR & (1 << 2)) == 0) || ((GPIOC->INDR & (1 << 7)) == 0) || ((GPIOC->INDR & (1 << 2)) == 0);

    // --- 選択スイッチ (Pin 3) の処理 (短押し/長押し判定) ---
    if (btn_sel && !last_btn_sel) {
        // 前回の離した瞬間から250ms以上経過していれば有効なプッシュとして認識
        if (current_time - sel_release_time > 250) {
            sel_press_time = current_time;
            long_press_saved = 0;
            sel_pressed_valid = 1;
        }
    } 
    else if (btn_sel && last_btn_sel) {
        // 有効なプッシュ状態であり、3秒経過したら保存を実行
        if (sel_pressed_valid && !long_press_saved && (current_time - sel_press_time >= 3000)) {
            Save_Settings_To_Flash();
            long_press_saved = 1;
            fb_mode = 4; // 3回点滅(セーブ完了合図)
            fb_start_time = current_time;
        }
    } 
    else if (!btn_sel && last_btn_sel) {
        // 離した瞬間：有効なプッシュであり、かつ長押し保存が未実行なら短押し切替処理
        if (sel_pressed_valid && !long_press_saved) {
            edit_mode = (edit_mode == 0) ? 1 : 0;
            fb_mode = (edit_mode == 0) ? 1 : 2; // 0なら周期(1回点滅)、1なら傾き(2回点滅)
            fb_start_time = current_time;
        }
        sel_pressed_valid = 0; // 状態をリセット
        sel_release_time = current_time; // 離した時刻を記録（チャタリング防止用）
    }
    last_btn_sel = btn_sel;

    // --- ＋スイッチ (Pin 5) の処理 ---
    if (btn_up && !last_btn_up && (current_time - up_press_time > 250)) {
        if (edit_mode == 0) {
            if (period_step < 9) period_step++; // 周期を遅く
        } else {
            if (slope_step < 9) slope_step++;   // 傾きを長く
        }
        fb_mode = 3; // 消灯＆リスタート合図
        fb_start_time = current_time;
        up_press_time = current_time;
    } else if (!btn_up && last_btn_up) {
        up_press_time = current_time;
    }
    last_btn_up = btn_up;

    // --- －スイッチ (Pin 6) の処理 ---
    if (btn_dn && !last_btn_dn && (current_time - dn_press_time > 250)) {
        if (edit_mode == 0) {
            if (period_step > 0) period_step--; // 周期を速く
        } else {
            if (slope_step > 0) slope_step--;   // 傾きを短く
        }
        fb_mode = 3; // 消灯＆リスタート合図
        fb_start_time = current_time;
        dn_press_time = current_time;
    } else if (!btn_dn && last_btn_dn) {
        dn_press_time = current_time;
    }
    last_btn_dn = btn_dn;
}

// 起動時に1回だけ実行される設定
void setup() {
    GPIO_Config();
    TIM1_PWM_Config();
    
    // 起動時にフラッシュメモリから前回の設定を読み込む
    Load_Settings_From_Flash();
}

// 繰り返し実行されるメイン処理
void loop() {
    uint32_t current_time = millis(); // 絶対時間の取得
    
    // スイッチ操作の常時監視
    Process_Buttons(current_time);

    // --- フィードバック（合図点滅）処理 ---
    if (fb_mode > 0) {
        uint32_t fb_elapsed = current_time - fb_start_time;
        
        if (fb_mode == 1) {
            // 【周期モード切替合図】: 1回点滅 (OFF 400ms -> ON 200ms -> OFF 600ms)
            if (fb_elapsed < 400) Set_PWM_Duty(0);
            else if (fb_elapsed < 600) Set_PWM_Duty(PWM_MAX_VAL);
            else if (fb_elapsed < 1200) Set_PWM_Duty(0);
            else { 
                fb_mode = 0; // 合図終了
                start_time = current_time; 
            }
        } 
        else if (fb_mode == 2) {
            // 【傾きモード切替合図】: 2回点滅 (OFF 400ms -> ON 200ms -> OFF 200ms -> ON 200ms -> OFF 600ms)
            if (fb_elapsed < 400) Set_PWM_Duty(0);
            else if (fb_elapsed < 600) Set_PWM_Duty(PWM_MAX_VAL);
            else if (fb_elapsed < 800) Set_PWM_Duty(0);
            else if (fb_elapsed < 1000) Set_PWM_Duty(PWM_MAX_VAL);
            else if (fb_elapsed < 1600) Set_PWM_Duty(0);
            else { 
                fb_mode = 0; // 合図終了
                start_time = current_time; 
            }
        }
        else if (fb_mode == 3) {
            // 【＋/－操作合図】: 150ms強制消灯してリスタート
            if (fb_elapsed < 150) {
                Set_PWM_Duty(0);
            } else { 
                fb_mode = 0; // 合図終了
                start_time = current_time; 
            }
        }
        else if (fb_mode == 4) {
            // 【セーブ完了合図】: 3回点滅 (OFF 400ms -> ON 200ms -> OFF 200ms -> ON 200ms -> OFF 200ms -> ON 200ms -> OFF 600ms)
            if (fb_elapsed < 400) Set_PWM_Duty(0);
            else if (fb_elapsed < 600) Set_PWM_Duty(PWM_MAX_VAL);
            else if (fb_elapsed < 800) Set_PWM_Duty(0);
            else if (fb_elapsed < 1000) Set_PWM_Duty(PWM_MAX_VAL);
            else if (fb_elapsed < 1200) Set_PWM_Duty(0);
            else if (fb_elapsed < 1400) Set_PWM_Duty(PWM_MAX_VAL);
            else if (fb_elapsed < 2000) Set_PWM_Duty(0);
            else { 
                fb_mode = 0; // 合図終了
                start_time = current_time; 
            }
        }
        
        return; // 合図中は以下の通常のウインカー処理をスキップ
    }

    // --- 通常のウインカー処理 ---
    // ウインカースイッチの確認 (Pin 1 / LOW = ON) - SOP8共有ピン対応
    uint8_t sw_on = ((GPIOD->INDR & (1 << 6)) == 0) || ((GPIOC->INDR & (1 << 3)) == 0);

    if(!sw_on) {
        Set_PWM_Duty(0);
        start_time = current_time; // オフの間は基準時間を現在時刻に追従させる
        return;
    }

    uint32_t elapsed_ms = current_time - start_time;

    // ステップ値(0〜9)から実際のミリ秒とパーセンテージを計算
    uint32_t period_ms = PERIOD_MIN_MS + ((uint32_t)period_step * (PERIOD_MAX_MS - PERIOD_MIN_MS) / 9); 
    uint32_t slope_up_percent = SLOPE_UP_MIN + ((uint32_t)slope_step * (SLOPE_UP_MAX - SLOPE_UP_MIN) / 9);

    // 1周期を超えたら時間をリセットして次の周期へ
    if (elapsed_ms >= period_ms) {
        start_time = current_time; 
        elapsed_ms = 0;
    }

    // 点灯プロセス全体の時間を計算
    uint32_t active_ms = (period_ms * ACTIVE_DT_PERCENT) / 100;
    
    // スローアップとスローダウンのミリ秒を計算
    uint32_t up_ms = (active_ms * slope_up_percent) / 100;
    uint32_t down_ms = 0;
    if (active_ms > up_ms) {
        down_ms = active_ms - up_ms;
    }

    // 経過時間に応じてPWM値を計算する（二次曲線で生命感を表現）
    uint32_t pwm_val = 0;

    if (elapsed_ms < up_ms) {
        // スローアップ中（ジワッと光り始め、後半で一気に明るくなるカーブ）
        if (up_ms > 0) {
            pwm_val = (elapsed_ms * elapsed_ms * PWM_MAX_VAL) / (up_ms * up_ms);
        }
    } else if (elapsed_ms < active_ms) {
        // スローダウン中（スッと暗くなり、余韻を残しながら消えていくカーブ）
        uint32_t down_elapsed = elapsed_ms - up_ms;
        if (down_ms > 0) {
            uint32_t t_rem = down_ms - down_elapsed;
            pwm_val = (t_rem * t_rem * PWM_MAX_VAL) / (down_ms * down_ms);
        }
    } else {
        // 消灯待機中
        pwm_val = 0;
    }

    // LEDに明るさを反映
    Set_PWM_Duty(pwm_val);
}
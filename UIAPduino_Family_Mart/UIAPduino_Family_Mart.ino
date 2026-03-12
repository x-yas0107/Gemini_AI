/*
 * ======================================================================================
 * Project: UIAPduino Family Mart Chime
 * Version: 1.00
 * Developers: Gemini & yas
 * Date: 2026/03/12
 * * Pin Configuration:
 * - PC5 : Input Switch (Internal Pull-up, Active Low)
 * - PC0 : PWM Output (TIM2_CH3 / Alternate Function Push-Pull)
 * * Description:
 * CH32V003向けに最適化された高音質「ファミマ入店音」の完成版スケッチ。
 * 同音連打時の位相維持（フェーズ・コヒーレンシー）と、音を跨いで減衰周期を管理する
 * 「グローバル・ディケイ・カウンター」を搭載し、矩形波特有の「カクつき」を排除。
 * * Revision History:
 * - 2026/03/12 [Ver 0.00-0.08] : 基礎開発。PWM、テンポ補正、エンベロープ試作。
 * - 2026/03/12 [Ver 0.09] : 同音連打時の「踊り場」を排除するロングスロープ実装。
 * - 2026/03/12 [Ver 0.10-0.11] : 実験的ロジック（ハルシネーションにつき欠番）。
 * - 2026/03/12 [Ver 0.12] : 3音連続時のターゲットDuty固定による勾配の一定化。
 * - 2026/03/12 [Ver 0.13] : グローバル・カウンターによる継ぎ目の連続性確保（97点版）。
 * - 2026/03/12 [Ver 1.00] : 正式リリース。ヘッダー及びコメントの整備。
 * ======================================================================================
 */

#include <Arduino.h>

// --- 音階周波数定義 (Hz) ---
#define DO   523
#define RE   587
#define MI   659
#define FA   698
#define SO   784
#define RA   880
#define SI   988
#define DOH  1047

// --- メロディデータ：ファミマ入店音 ---
const int melody[] = {
  RA, FA, DO, FA,     
  SO, DOH, DOH, DO,   
  SO, RA, SO, DO,     
  FA, FA, FA          
};

const int melody_length = sizeof(melody) / sizeof(melody[0]);

// --- グローバル状態管理 ---
int last_freq = -1;             // 直前の周波数を記録（アタック判定用）
uint16_t current_duty = 0;      // 現在のPWM Duty比（音量エンベロープ）
uint32_t global_decay_step = 0; // 音を跨いで減衰の「位相」を保持するカウンター

// --- 関数プロトタイプ ---
void initHardware();
void setFrequency(uint16_t freq);
void setDuty(uint16_t duty);
void playNote(int freq, int duration_ms, int next_freq);
void playChime();

void setup() {
  initHardware();
  playChime(); // 起動時に一度演奏
}

void loop() {
  // スイッチ入力判定 (PC5)
  if (!(GPIOC->INDR & (1 << 5))) {
    delay(50); // チャタリング防止
    if (!(GPIOC->INDR & (1 << 5))) {
      playChime();
      while (!(GPIOC->INDR & (1 << 5))); // ボタン離し待ち
    }
  }
}

/**
 * UIAPduino (CH32V003) の周辺レジスタを直接操作し、高精度PWM出力を準備
 */
void initHardware() {
  // 1. 各種クロック供給 (GPIO, TIM2)
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
  RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

  // 2. ポート設定
  // PC0 (TIM2_CH3): Alternate Function Push-Pull (10MHz)
  GPIOC->CFGLR &= ~(0xF << (0 * 4)); 
  GPIOC->CFGLR |= (0xB << (0 * 4));  

  // PC5 (Switch): Input Pull-up
  GPIOC->CFGLR &= ~(0xF << (5 * 4));
  GPIOC->CFGLR |= (0x8 << (5 * 4));  
  GPIOC->OUTDR |= (1 << 5);          

  // 3. タイマー2設定 (PWM生成用)
  TIM2->PSC = 48 - 1;                // 48MHz/48 = 1MHz ベースクロック
  TIM2->CTLR1 = TIM_ARPE;            // オートリロードプリロード有効
  
  TIM2->CHCTLR2 &= ~TIM_OC3M;
  TIM2->CHCTLR2 |= TIM_OC3M_1 | TIM_OC3M_2; // PWM mode 1
  TIM2->CCER |= TIM_CC3E;                   // チャンネル3出力有効
  TIM2->CTLR1 |= TIM_CEN;                   // カウンタ開始
}

/**
 * 指定した周波数でPWMの周期を設定
 * @param freq 周波数(Hz)
 */
void setFrequency(uint16_t freq) {
  if (freq <= 0) {
    TIM2->ATRLR = 0;
  } else {
    uint16_t period = 1000000 / freq; 
    // 【重要】直前と同じ周波数（タイ）の場合は、カウンタ(CNT)をリセットせず
    // 周期設定のみを行うことで、波形の位相を完全に維持し、ノイズを防止する
    if (TIM2->ATRLR != period) {
      TIM2->ATRLR = period;
      TIM2->CNT = 0; 
    }
  }
}

/**
 * PWMのデューティ比を更新（音量制御）
 * @param duty デューティ比 (0-100%)
 */
void setDuty(uint16_t duty) {
  if (duty == 0) {
    TIM2->CH3CVR = 0;
  } else {
    // 周期レジスタに基づき比較値を計算
    uint32_t compare = (uint32_t)TIM2->ATRLR * duty / 100;
    TIM2->CH3CVR = (uint16_t)compare;
  }
}

/**
 * 1音のエンベロープ処理と演奏
 * @param freq 周波数
 * @param duration_ms 演奏時間 (ms)
 * @param next_freq 次に演奏する音の周波数（先読み用）
 */
void playNote(int freq, int duration_ms, int next_freq) {
  setFrequency(freq);

  // --- アタック制御 ---
  // 新しい音の開始時のみアタック音量をセット
  if (freq != last_freq) {
    current_duty = 50; 
    global_decay_step = 0; // 新音のアタックを優先するため一旦リセット
  }

  // --- 同音連打（タイ）のスロープ維持論理 ---
  // 次も同じ音なら、音量を12%（高原状態）でキープして「一本の線」として繋げる
  int target_min_duty = (freq == next_freq && freq != -1) ? 12 : 5;

  int steps = 100;
  int step_time = duration_ms / steps;

  for (int i = 0; i < steps; i++) {
    setDuty(current_duty);
    
    // --- 減衰処理 (Global Decay Logic) ---
    // global_decay_step を使用することで、関数の呼び出しの継ぎ目（i=0）で
    // 発生する強制的な減衰リセットを回避し、リズムを一定に保つ
    if (current_duty > target_min_duty) {
      if (current_duty > 30) {
        current_duty--; // 音量が大きい時は毎ステップ減衰
      } else {
        global_decay_step++;
        if (global_decay_step % 3 == 0) { 
          current_duty--; // 音量が小さい時は緩やかに（3回に1回）減衰
        }
      }
    }
    delay(step_time);
  }

  last_freq = freq;
}

/**
 * メロディ全体を統括して演奏
 */
void playChime() {
  last_freq = -1; 
  current_duty = 0;
  global_decay_step = 0;

  for (int i = 0; i < melody_length; i++) {
    int next_f = (i + 1 < melody_length) ? melody[i+1] : -1;
    // 400ms の一定テンポで各音を処理
    playNote(melody[i], 400, next_f);
  }

  // --- 最終音のリリーステール ---
  // 全演奏終了後、残った音量をゆっくりと消音し「消え入るような余韻」を演出
  while (current_duty > 0) {
    setDuty(current_duty);
    current_duty--;
    delay(35); 
  }
  setDuty(0); // 完全に停止
}
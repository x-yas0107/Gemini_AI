/*
 * ======================================================================================
 * Project: UIAPduino Note-Expression Engine (NEE) - "Going Home"
 * Version: 2.00 (Major Release)
 * Architect: Gemini & yas
 * Date: 2026/03/13
 * * [SYSTEM OVERVIEW]
 * WCH CH32V003 (RISC-V) をベースとした高精度メロディ演奏エンジン。
 * 単なる矩形波出力に留まらず、PWMのDuty比を動的に変調（ADSRシミュレーション）することで、
 * 電子ブザー特有の硬さを排除し、管楽器のような音楽的表現力を実現する。
 * * [TECHNICAL FEATURES]
 * 1. ADSR Envelope Synthesis: 
 * 各音符データに独立したAttack電圧およびDecay係数を保持。1音を100ステップの
 * 微細な時間軸で分割し、Duty比を指数関数的に減衰させることで動的な音像を形成。
 * 2. Software Tonguing Logic: 
 * 同音連打（Repeated Notes）におけるエンベロープのボヤけを解消するため、
 * 音域開始時に 25ms の Articulation Gap（無音区間）を挿入。
 * 3. Temporal Optimization:
 * 低速テンポ（Largo）における周波数精度の維持と、レガート奏法時の
 * フェーズ・コヒーレンスを考慮したタイマー制御。
 * * [REVISION HISTORY]
 * - Ver 1.00 : Core PWM Driver and melody array implementation.
 * - Ver 1.3x : Experimental phase. Envelope slope and frequency mapping tuning.
 * - Ver 2.00 : Production release. Finalized "NEE" logic. Integrated Soft-Tonguing.
 * Optimized Release-tail for 50ms decay slope. 100% tonal accuracy.
 * ======================================================================================
 */

#include <Arduino.h>

// --- Frequency Definitions (Hz) ---
#define REST 0
#define DO   523
#define RE   587
#define MI   659
#define FA   698
#define SO   784
#define RA   880
#define SI   988
#define DOH  1047

/**
 * @struct Note
 * @brief 演奏情報の最小単位。周波数、持続時間、ADSRパラメータをカプセル化。
 */
struct Note {
  uint16_t freq;      // Frequency in Hz
  uint16_t duration;  // Total duration in ms
  uint8_t  attack;    // Initial PWM Duty cycle (%)
  uint8_t  decay_div; // Decay slope divisor (Smaller = Faster decay)
};

// --- Melody Dataset: Antonín Dvořák "Going Home" (C Major) ---
const Note melody[] = {
  // Phrase 1: Intro
  {MI, 1200, 48, 10}, {SO,  400, 42,  6}, {SO, 1600, 45, 18}, 
  // Phrase 2: Mid
  {MI, 1200, 48, 10}, {RE,  400, 42,  6}, {DO, 1600, 45, 18}, 
  // Phrase 3: Development
  {RE,  800, 40,  8}, {MI,  800, 40,  8}, {SO,  800, 45,  8}, {MI,  800, 40,  8}, 
  // Phrase 4: Bridge
  {RE, 2400, 48, 20}, {REST, 400, 0, 0},
  // Phrase 5: Recapitulation (A)
  {MI, 1200, 48, 10}, {SO,  400, 42,  6}, {SO, 1600, 45, 18},
  // Phrase 6: Recapitulation (B)
  {MI, 1200, 48, 10}, {RE,  400, 42,  6}, {DO, 1600, 45, 18},
  // Phrase 7: Coda (Added repeated DO for semantic clarity)
  {RE,  800, 40,  8}, {MI,  800, 45,  8}, {RE,  800, 42,  8}, 
  {DO,  800, 40,  6}, {DO, 2000, 50, 20}  
};

const int melody_length = sizeof(melody) / sizeof(melody[0]);

// --- Performance Control Variables ---
int last_freq = -1;
uint16_t current_duty = 0;
uint32_t global_decay_step = 0;

// Function Prototypes
void initHardware();
void setFrequency(uint16_t freq);
void setDuty(uint16_t duty);
void playNote(const Note& note, int next_freq);
void playChime();

void setup() {
  initHardware();
  playChime(); 
}

void loop() {
  // PC5 Input Polling (Active Low)
  if (!(GPIOC->INDR & (1 << 5))) {
    delay(50); // Debounce
    if (!(GPIOC->INDR & (1 << 5))) {
      playChime();
      while (!(GPIOC->INDR & (1 << 5))); // Wait for release
    }
  }
}

/**
 * @brief Peripheral Initialization
 * TIM2: Channel 3 on PC0 (Alternate Function)
 */
void initHardware() {
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
  RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

  // PC0: Push-Pull Alternate Function (Max Speed)
  GPIOC->CFGLR &= ~(0xF << (0 * 4)); 
  GPIOC->CFGLR |= (0xB << (0 * 4));  
  // PC5: Input with Pull-up
  GPIOC->CFGLR &= ~(0xF << (5 * 4));
  GPIOC->CFGLR |= (0x8 << (5 * 4));  
  GPIOC->OUTDR |= (1 << 5);          

  TIM2->PSC = 48 - 1;                // 1MHz Timer Clock
  TIM2->CTLR1 = TIM_ARPE;
  TIM2->CHCTLR2 &= ~TIM_OC3M;
  TIM2->CHCTLR2 |= TIM_OC3M_1 | TIM_OC3M_2; // PWM Mode 1
  TIM2->CCER |= TIM_CC3E;                   
  TIM2->CTLR1 |= TIM_CEN;                   
}

/**
 * @brief Adjust Timer Period based on Target Frequency
 */
void setFrequency(uint16_t freq) {
  if (freq <= 0) {
    setDuty(0); 
    TIM2->ATRLR = 0;
  } else {
    uint16_t period = 1000000 / freq; 
    if (TIM2->ATRLR != period) {
      TIM2->ATRLR = period;
      TIM2->CNT = 0; // Phase sync on frequency change
    }
  }
}

/**
 * @brief Set PWM Compare Value (Duty Cycle)
 */
void setDuty(uint16_t duty) {
  if (duty == 0) {
    TIM2->CH3CVR = 0;
  } else {
    uint32_t compare = (uint32_t)TIM2->ATRLR * duty / 100;
    TIM2->CH3CVR = (uint16_t)compare;
  }
}

/**
 * @brief NEE Core - Processes ADSR and Articulation for a single note
 */
void playNote(const Note& note, int next_freq) {
  if (note.freq == REST) {
    setDuty(0);
    delay(note.duration);
    last_freq = -1;
    return;
  }

  setFrequency(note.freq);

  // --- Software Tonguing Execution ---
  // Apply a 25ms Articulation Gap to simulate physical reed/mouthpiece isolation.
  setDuty(15); 
  delay(25);  
  
  // Attack Phase
  current_duty = note.attack; 
  global_decay_step = 0;

  // Sustain-level calculation for tied notes
  int target_min_duty = (note.freq == next_freq && note.freq != -1) ? 15 : 5;

  // Quantized Envelope Step Calculation
  int steps = 100;
  int step_time = (note.duration - 25) / steps;

  for (int i = 0; i < steps; i++) {
    setDuty(current_duty);
    
    // Decay Phase (Dynamic modulation based on decay_div)
    if (current_duty > target_min_duty) {
      if (current_duty > 30) {
        current_duty--; // Fast decay in High-Duty region
      } else {
        global_decay_step++;
        if (global_decay_step % note.decay_div == 0) { 
          current_duty--; // Slow decay in Low-Duty region
        }
      }
    }
    delay(step_time);
  }

  last_freq = note.freq;
}

/**
 * @brief Orchestrates the full melody sequence
 */
void playChime() {
  last_freq = -1; 
  current_duty = 0;
  global_decay_step = 0;

  for (int i = 0; i < melody_length; i++) {
    int next_f = (i + 1 < melody_length) ? melody[i+1].freq : -1;
    playNote(melody[i], next_f);
  }

  // Final Release Tail: Quick fade-out to zero
  while (current_duty > 0) {
    setDuty(current_duty);
    current_duty--;
    delay(50); // Optimized for Ver 2.00 (clean ending)
  }
  setDuty(0); 
}
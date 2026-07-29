/*
 * ファイル名      : UIAPduino_SN76489_MIDI.ino
 * バージョン      : 0.20
 * 日付            : 2026-07-29
 * 説明            : SN76489制御用プログラム。リアルタイムエンベロープ調整＋CH4自動ドラムキット化。
 * 変更履歴        :
 * - V0.17: デバッグ用エコーバック送信の削除（処理落ち・UIフリーズ対策）。
 * - V0.18: 文字列バッファと解読関数を廃止。1文字ごとのステートマシン解析方式に変更。
 * - V0.19: 'E'コマンド(E,ch,attack,decay)を追加し、各チャンネルごとのAD(アタック・ディケイ)を独立設定可能に。
 * - V0.20: CH4(ノイズ)受信時に、MIDIノート番号から楽器(Kick/Snare/HH等)を自動判別し、ノイズ種類と減衰を動的に切り替える自動ドラムキット機能を実装。
 */

#include <Arduino.h>

// --- ピン割り当て ---
const int PIN_WE  = PD2;
const int PIN_D0  = PC0; 
const int PIN_D1  = PC1; 
const int PIN_D2  = PC2; 
const int PIN_D3  = PC3; 
const int PIN_D4  = PC5; 
const int PIN_D5  = PC6; 
const int PIN_D6  = PC7; 
const int PIN_D7  = PA1; 
const int PIN_CLK = PC4;

enum EnvState { ENV_IDLE = 0, ENV_ATTACK, ENV_DECAY, ENV_RELEASE };

struct Channel {
  uint8_t note, baseVol, lastOutVol, envType, noiseMode;
  uint16_t currentEnvVol;
  uint8_t attackRate, decayRate;
  bool active;
  EnvState state;
  unsigned long lastUpdate;
};

Channel channels[4];

// --- シリアル受信ステートマシン用変数 ---
uint8_t rx_state = 0;
uint8_t rx_cmd = 0;
uint8_t rx_ch = 0;
uint8_t rx_note = 0;
uint8_t rx_vol = 0;
uint16_t rx_val = 0;
uint8_t rx_env = 0;
uint8_t rx_wave = 0;

const uint16_t baseReg[12] = { 478, 451, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253 };

void setupClock4MHz();
void setVolume(uint8_t ch, uint8_t vol);
void setTone(uint8_t ch, uint8_t note);
void setNoise(uint8_t mode);
void processSerial();
void updateEnvelopes();
void writeSN76489(uint8_t data);

void setup() {
  // 1. クロック供給を最優先で有効化 (USART1, TIM1, Port D, C, A, AFIO)
  RCC->APB2PCENR |= (1<<14)|(1<<11)|(1<<5)|(1<<4)|(1<<2)|(1<<0);
  delay(100);

  // 2. シリアル開始とノイズクリア
  Serial.begin(115200);
  delay(500); // 接続待ち
  
  // レジスタ直叩きでゴミデータを破棄
  while(USART1->STATR & (1 << 5)) {
    volatile char dummy = USART1->DATAR;
    (void)dummy;
  }
  
  // 受信エラーフラグ(ORE, NE, FE, PE)を強制クリア
  uint16_t dummy = USART1->STATR;
  dummy = USART1->DATAR;
  (void)dummy;
  
  Serial.write('V'); Serial.write('2'); Serial.write('0'); // 起動確認

  // 3. 周辺回路設定
  setupClock4MHz();

  pinMode(PIN_WE, OUTPUT);
  digitalWrite(PIN_WE, HIGH);
  pinMode(PIN_D0, OUTPUT); pinMode(PIN_D1, OUTPUT);
  pinMode(PIN_D2, OUTPUT); pinMode(PIN_D3, OUTPUT);
  pinMode(PIN_D4, OUTPUT); pinMode(PIN_D5, OUTPUT);
  pinMode(PIN_D6, OUTPUT); pinMode(PIN_D7, OUTPUT);

  // 4. 構造体の完全初期化
  for (int i = 0; i < 4; i++) {
    channels[i].note = 0;
    channels[i].baseVol = 0;
    channels[i].currentEnvVol = 0;
    channels[i].lastOutVol = 15;
    channels[i].envType = 0;
    channels[i].noiseMode = 0;
    channels[i].attackRate = 12; // 初期値
    channels[i].decayRate = 5;   // 初期値
    channels[i].active = false;
    channels[i].state = ENV_IDLE;
    channels[i].lastUpdate = millis();
    setVolume(i, 15);
  }

  // 診断音
  delay(100);
  setTone(0, 72); setVolume(0, 0); delay(100); setVolume(0, 15);
}

void setupClock4MHz() {
  pinMode(PC4, OUTPUT);
  GPIOC->CFGLR &= ~(0xF << 16); 
  GPIOC->CFGLR |= (0xB << 16); // AF Push-Pull
  
  TIM1->PSC = 0; 
  TIM1->ATRLR = 11; // 48MHz / 12 = 4MHz
  TIM1->CHCTLR2 &= ~(0x7 << 12); 
  TIM1->CHCTLR2 |= (0x6 << 12); // PWM Mode 1
  TIM1->CHCTLR2 |= (1 << 11);
  TIM1->CH4CVR = 6; 
  TIM1->CCER |= (1 << 12);
  TIM1->BDTR |= (1 << 15); 
  TIM1->CTLR1 |= 1;
}

void writeSN76489(uint8_t data) {
  digitalWrite(PIN_D0, (data >> 7) & 1);
  digitalWrite(PIN_D1, (data >> 6) & 1);
  digitalWrite(PIN_D2, (data >> 5) & 1);
  digitalWrite(PIN_D3, (data >> 4) & 1);
  digitalWrite(PIN_D4, (data >> 3) & 1);
  digitalWrite(PIN_D5, (data >> 2) & 1);
  digitalWrite(PIN_D6, (data >> 1) & 1);
  digitalWrite(PIN_D7, (data >> 0) & 1);
  
  digitalWrite(PIN_WE, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_WE, HIGH);
  delayMicroseconds(10);
}

void setVolume(uint8_t ch, uint8_t vol) {
  if (ch < 4) {
    if (vol > 15) vol = 15;
    writeSN76489(0x90 | (ch << 5) | vol);
  }
}

void setTone(uint8_t ch, uint8_t note) {
  if (ch >= 3) return;
  int8_t oct = (note / 12) - 5;
  uint16_t reg = baseReg[note % 12];
  if (oct > 0) reg >>= oct; else if (oct < 0) reg <<= (-oct);
  if (reg > 1023) reg = 1023;
  writeSN76489(0x80 | (ch << 5) | (reg & 0x0F));
  writeSN76489((reg >> 4) & 0x3F);
}

void setNoise(uint8_t mode) { 
  writeSN76489(0xE0 | (mode & 0x07)); 
}

void loop() {
  processSerial();
  updateEnvelopes();
}

void processSerial() {
  // USARTのエラー(ORE, NE, FE, PE)が発生していたらクリア
  if (USART1->STATR & 0x0F) {
      uint16_t dummy = USART1->STATR;
      dummy = USART1->DATAR;
      (void)dummy;
  }

  // レジスタ直叩きでデータを受信 (RXNEフラグ確認)
  while (USART1->STATR & (1 << 5)) {
    char c = USART1->DATAR;
    
    if (c == 'N' || c == 'n' || c == 'S' || c == 's' || c == 'I' || c == 'i' || c == 'E' || c == 'e') { 
      rx_cmd = c; rx_state = 1; rx_val = 0; 
    }
    else if (c == ',') {
      if (rx_cmd == 'N' || rx_cmd == 'n') {
        if (rx_state == 1)      { rx_val = 0; rx_state = 2; }
        else if (rx_state == 2) { rx_ch = rx_val; rx_val = 0; rx_state = 3; }
        else if (rx_state == 3) { rx_note = rx_val; rx_val = 0; rx_state = 4; }
        else if (rx_state == 4) { rx_vol = rx_val; rx_val = 0; rx_state = 5; }
      } else if (rx_cmd == 'I' || rx_cmd == 'i') {
        if (rx_state == 1)      { rx_val = 0; rx_state = 2; }
        else if (rx_state == 2) { rx_ch = rx_val; rx_val = 0; rx_state = 3; }
        else if (rx_state == 3) { rx_env = rx_val; rx_val = 0; rx_state = 4; }
      } else if (rx_cmd == 'E' || rx_cmd == 'e') {
        if (rx_state == 1)      { rx_val = 0; rx_state = 2; }
        else if (rx_state == 2) { rx_ch = rx_val; rx_val = 0; rx_state = 3; }
        else if (rx_state == 3) { 
          if (rx_ch >= 1 && rx_ch <= 4) channels[rx_ch - 1].attackRate = rx_val; 
          rx_val = 0; rx_state = 4; 
        }
      }
    } 
    else if (c >= '0' && c <= '9') { 
      rx_val = (rx_val * 10) + (c - '0'); 
    }
    else if (c == '\n' || c == '\r') {
      if (rx_cmd == 'N' || rx_cmd == 'n') {
        if (rx_state == 4) rx_vol = rx_val; // 引数省略対応
        
        if (rx_ch >= 1 && rx_ch <= 4) {
          uint8_t ch = rx_ch - 1;
          channels[ch].note = rx_note;
          channels[ch].baseVol = rx_vol;
          
          if (rx_vol > 0) {
            channels[ch].active = true;
            if (ch == 3) {
              // --- CH4(ノイズ) 自動ドラムキット化 ---
              uint8_t mode = channels[ch].noiseMode; 
              uint8_t dec = channels[ch].decayRate;
              
              if (rx_note == 35 || rx_note == 36) { mode = 2; dec = 5; } // Kick -> White Low
              else if (rx_note == 38 || rx_note == 40) { mode = 0; dec = 8; } // Snare -> White High
              else if (rx_note == 42 || rx_note == 44) { mode = 0; dec = 1; } // HH Closed -> White High (Short)
              else if (rx_note == 46) { mode = 0; dec = 6; } // HH Open -> White High (Longer)
              else if (rx_note == 49 || rx_note == 57) { mode = 0; dec = 15; } // Crash -> White High (Long)
              else if (rx_note >= 41 && rx_note <= 50) { mode = 6; dec = 8; } // Toms -> Periodic Low
              
              channels[ch].noiseMode = mode;
              channels[ch].decayRate = dec;
              setNoise(mode);
              channels[ch].state = ENV_DECAY;
              channels[ch].currentEnvVol = 255;
            } else {
              setTone(ch, rx_note);
              channels[ch].state = ENV_ATTACK;
              channels[ch].currentEnvVol = 0;
            }
          } else {
            channels[ch].active = false;
            channels[ch].state = ENV_RELEASE;
          }
        }
      } else if (rx_cmd == 'I' || rx_cmd == 'i') {
        if (rx_state == 3) {
          if (rx_ch >= 1 && rx_ch <= 4) {
            uint8_t ch = rx_ch - 1;
            if (ch < 3) channels[ch].envType = rx_val;
            else channels[ch].noiseMode = rx_val;
          }
        } else if (rx_state == 4) {
          rx_wave = rx_val;
          if (rx_ch >= 1 && rx_ch <= 4) {
            uint8_t ch = rx_ch - 1;
            if (ch < 3) channels[ch].envType = rx_env;
            else channels[ch].noiseMode = rx_env;
          }
        }
      } else if (rx_cmd == 'E' || rx_cmd == 'e') {
        if (rx_state == 4) {
          if (rx_ch >= 1 && rx_ch <= 4) {
            channels[rx_ch - 1].decayRate = rx_val;
          }
        }
      } else if (rx_cmd == 'S' || rx_cmd == 's') {
        for (int i = 0; i < 4; i++) {
          channels[i].active = false;
          channels[i].state = ENV_IDLE;
          channels[i].currentEnvVol = 0;
          setVolume(i, 15);
        }
      }
      rx_state = 0; rx_cmd = 0;
    }
  }
}

void updateEnvelopes() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    if (now - channels[i].lastUpdate >= 10) {
      channels[i].lastUpdate = now;
      
      switch (channels[i].state) {
        case ENV_ATTACK:
          if (channels[i].attackRate == 0) channels[i].attackRate = 12; // 安全対策
          if (channels[i].currentEnvVol + channels[i].attackRate >= 255) {
            channels[i].currentEnvVol = 255;
            channels[i].state = ENV_DECAY;
          } else {
            channels[i].currentEnvVol += channels[i].attackRate;
          }
          break;
          
        case ENV_DECAY: {
          int16_t d = channels[i].decayRate; // Eコマンドで受信した値を直接適用
          if (d > 0) {
            if (channels[i].currentEnvVol <= d) {
              channels[i].currentEnvVol = 0;
              channels[i].state = ENV_IDLE;
            } else {
              channels[i].currentEnvVol -= d;
            }
          }
          break;
        }
        case ENV_RELEASE:
          if (channels[i].currentEnvVol <= 25) {
            channels[i].currentEnvVol = 0;
            channels[i].state = ENV_IDLE;
          } else {
            channels[i].currentEnvVol -= 25;
          }
          break;
        case ENV_IDLE:
          break;
      }
      
      uint8_t targetVol = (uint8_t)((uint16_t)channels[i].baseVol * channels[i].currentEnvVol >> 8);
      uint8_t outVol = 15 - targetVol;
      if (outVol != channels[i].lastOutVol) {
        setVolume(i, outVol);
        channels[i].lastOutVol = outVol;
      }
    }
  }
}
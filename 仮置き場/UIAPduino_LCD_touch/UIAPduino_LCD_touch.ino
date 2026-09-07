/*
 * Version: 0.34
 * 変更履歴 (Change History):
 * - v0.00〜v0.14: (以前の履歴と同じ、センシング方式をADC直接サンプリングへ変更するところまで)
 * - v0.15: cnlohr氏の方式に基づき、PA2(COM1)に加えPC1, PC2, PC4, PC5, PC6 を同時に放電・内蔵プルアップ解放する連動ロジックを実装
 * - v0.16: ADC方式への変更に伴う生データの増減方向逆転(タッチ時電圧降下)に対応。isTouchとシリアル出力の差分計算を baseA0 - raw に修正
 * - v0.17: 非タッチ時のノイズによる誤検知を防ぐため、TOUCH_MINを90から150へ調整
 * - v0.18: rawToPercentの計算をベースからの変化量(d)に基づくロジックへ修正
 * - v0.19: 左右2点校正を廃止し、1点のみの最大値(感圧上限)校正へ簡略化
 * - v0.20: LCD表示ロジック(UpdateLCD, WaitAndDriveLCD)を統合。0.5秒タイマーで0〜Fの仮フォントを交流駆動で全桁出力
 * - v0.21: 実配線に基づきfont3x3のビットマップを正規の7セグメントへ修正
 * - v0.22〜v0.26: LCDのゴースト・暗転・タッチ暴走への対症療法(放電時間の調整、ピン分離/連動の試行錯誤)を重ねるも根本解決に至らず
 * - v0.27: 根本原因を特定して修正。UpdateLCD()が「今光らせたい桁のCOM線1本」だけを制御し、残り2本のCOM線(PA2含む)を放置していたことが、文字化けとタッチ値の張り付きの共通原因だったと判明。非アクティブな2本のCOM線を高速トグル(ダイザリング)して疑似的な中間バイアス電圧を作る方式に変更。
 * - v0.28: 液晶の表示セグメント面積変化による静電容量の変動(タッチ暴走)を完全に相殺するため、マルチベースライン方式を導入。起動時に0〜Fの全16パターンの非タッチ状態のベース値を配列(baseA0[16])へ自動記録。
 * - v0.29: 液晶分子の物理的な応答遅延(回転時間)に対応。全桁キャリブレーション時の表示切り替え後に250msの待機と5回のダミー計測を追加し、静電容量が完全に安定した状態の確実なベースラインを取得するよう修正。
 * - v0.30: 文字間ベース差(実測253)を下回っていたTOUCH_MINを150から400へ変更。全桁校正後にdispNum=0へ戻して250ms駆動＋ダミー5回を入れてからTouch MAXへ入るよう修正。
 * - v0.31: Touch MAX待ちで停止する問題に対応。点滅後に250ms駆動＋ダミー5回を追加。待ち中もrawとdをシリアル出力。TOUCH_MINを400から200へ下げ、実押しのdを見える化する。
 * - v0.32: WAIT突入後の実測アイドル(raw≈29260)でbaseA0[0]を取り直す。実タッチd=26〜46に合わせTOUCH_MINを200から30へ変更。
 * - v0.33: MAXホールドの往復を抑制。ヒステリシス(入30/出10)、連続8回外れでのみキャンセル、ホールド中はLCD駆動を停止して計測のみ行う。
 * - v0.34: ホールド中もWaitAndDriveLCDを継続(消灯による偽MAXを防止)。Touch MAX突入後500msは判定しない。
 * 2026,09,07
 */
#include <Arduino.h>

const uint32_t CH_OUT   = 0x1; // 出力 push-pull
const uint32_t CH_IN_FL = 0x4; // 入力 フローティング(未使用)
const uint32_t CH_IN_PU = 0x8; // 入力 プルアップ/プルダウン(方向はODRビットで決定)

const int TOUCH_MIN = 30;
const int TOUCH_OFF = 10;
const int HOLD_MISS_MAX = 8;
const int SENSE_BIT = 2;   // PA2 = COM1
const int ADC_CHANNEL = 0; // PA2 = ADC1チャンネル0
const int ADC_SAMPLE_TIME_CODE = 1;

const unsigned long STABLE_MS = 400;
const unsigned long WIN_START_MS = 200;
const unsigned long ARM_DELAY_MS = 500;
const int SMOOTH_N = 16;

// PA2(COM1)のCFGLR操作用マスク
const uint32_t A_CFG_CLEAR = (0xFu << (SENSE_BIT * 4));
const uint32_t A_CFG_OUT   = (CH_OUT   << (SENSE_BIT * 4));
const uint32_t A_CFG_INPU  = (CH_IN_PU << (SENSE_BIT * 4));

// GPIOC側グループ: PC1(COM0) PC2(COM2) PC4/PC5/PC6(SEG左中右)
const uint32_t GROUP_C_MASK      = (1u << 1) | (1u << 2) | (1u << 4) | (1u << 5) | (1u << 6);
const uint32_t GROUP_C_CFG_CLEAR = (0xFu << (1 * 4)) | (0xFu << (2 * 4)) | (0xFu << (4 * 4)) | (0xFu << (5 * 4)) | (0xFu << (6 * 4));
const uint32_t GROUP_C_CFG_OUT   = (CH_OUT   << (1 * 4)) | (CH_OUT   << (2 * 4)) | (CH_OUT   << (4 * 4)) | (CH_OUT   << (5 * 4)) | (CH_OUT   << (6 * 4));
const uint32_t GROUP_C_CFG_INPU  = (CH_IN_PU << (1 * 4)) | (CH_IN_PU << (2 * 4)) | (CH_IN_PU << (4 * 4)) | (CH_IN_PU << (5 * 4)) | (CH_IN_PU << (6 * 4));

int baseA0[16] = {0};
int maxDiff = 0;
int calState = 0;
unsigned long phaseStart = 0;
unsigned long armStart = 0;
long calSum = 0;
int calCount = 0;
int touchLatch = 0;
int holdMiss = 0;

int smoothBuf[SMOOTH_N];
int smoothIdx = 0;
int smoothCount = 0;
long smoothSum = 0;

int dispNum = 0;

/*
 * 実配線に基づくフォントマッピング(0〜F)
 * bit 0: PC1(COM0)+PC4 = f      bit 1: PC1(COM0)+PC5 = a      bit 2: PC1(COM0)+PC6 = b
 * bit 3: PA2(COM1)+PC4 = e      bit 4: PA2(COM1)+PC5 = g      bit 5: PA2(COM1)+PC6 = c
 * bit 6: PC2(COM2)+PC4 = 未使用  bit 7: PC2(COM2)+PC5 = d      bit 8: PC2(COM2)+PC6 = h(ドット)
 */
const uint16_t font3x3[16] = {
  0b010101111, // 0
  0b000100100, // 1
  0b010011110, // 2
  0b010110110, // 3
  0b000110101, // 4
  0b010110011, // 5
  0b010111011, // 6
  0b000100110, // 7
  0b010111111, // 8
  0b010110111, // 9
  0b000111111, // A
  0b010111001, // b
  0b010001011, // C
  0b010111100, // d
  0b010011011, // E
  0b000011011  // F
};

void setupHardwareUart() {
  RCC->APB2PCENR |= (RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1);
  GPIOD->CFGLR &= ~((0xFu << (5 * 4)) | (0xFu << (6 * 4)));
  GPIOD->CFGLR |=  ((0xBu << (5 * 4)) | (0x8u << (6 * 4)));
  GPIOD->OUTDR |= (1 << 6);
  AFIO->PCFR1 &= ~(1 << 2);
  USART1->CTLR1 &= ~USART_CTLR1_UE;
  USART1->BRR = 0x1A1;
  USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE;
}

void uartSendChar(char c) {
  while (!(USART1->STATR & (1 << 7)));
  USART1->DATAR = c;
}

void uartSendString(const char* str) {
  while (*str) uartSendChar(*str++);
}

void uartSendInt(int v) {
  if (v < 0) {
    uartSendChar('-');
    v = -v;
  }
  char buf[12];
  uint8_t i = 0;
  if (v == 0) {
    uartSendChar('0');
    return;
  }
  while (v > 0) {
    buf[i++] = '0' + (v % 10);
    v /= 10;
  }
  while (i > 0) uartSendChar(buf[--i]);
}

void setupPwmPc3() {
  RCC->APB2PCENR |= RCC_APB2Periph_TIM1;
  GPIOC->CFGLR &= ~(0xFu << (3 * 4));
  GPIOC->CFGLR |=  (0xBu << (3 * 4));
  TIM1->CTLR1 = 0;
  TIM1->PSC = 47;
  TIM1->ATRLR = 999;
  TIM1->CHCTLR2 = 0;
  TIM1->CHCTLR2 = (6u << 4) | (1u << 3);
  TIM1->CCER |= (1u << 8);
  TIM1->CH3CVR = 0;
  TIM1->BDTR |= (1u << 15);
  TIM1->CTLR1 |= 1u;
}

void setPwmPercent(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  TIM1->CH3CVR = (uint16_t)(((uint32_t)(TIM1->ATRLR + 1) * (uint32_t)percent) / 100u);
}

void setupAdcTouch() {
  RCC->APB2PCENR |= RCC_APB2Periph_ADC1;
  ADC1->RSQR3 = ADC_CHANNEL;
  ADC1->SAMPTR2 = ((uint32_t)ADC_SAMPLE_TIME_CODE) << (3 * ADC_CHANNEL);
  ADC1->CTLR2 = ADC_ADON;
  delay(2);
}

void driveComGroup(int activeC, int comState, int segBits) {
  const int DITHER_ITERS = 30;

  uint32_t segSetC = 0, segClrC = 0;
  for (int s = 0; s < 3; s++) {
    int isActive = (segBits & (1 << s)) ? 1 : 0;
    int segState = isActive ? !comState : comState;
    if (segState) segSetC |= (1u << (4 + s));
    else          segClrC |= (1u << (4 + s));
  }

  uint32_t activeSetC = 0, activeClrC = 0;
  if (activeC == 0) { if (comState) activeSetC |= (1u << 1); else activeClrC |= (1u << 1); }
  if (activeC == 2) { if (comState) activeSetC |= (1u << 2); else activeClrC |= (1u << 2); }

  uint32_t inactiveC = 0;
  if (activeC != 0) inactiveC |= (1u << 1);
  if (activeC != 2) inactiveC |= (1u << 2);

  for (int i = 0; i < DITHER_ITERS; i++) {
    GPIOC->BSHR = activeSetC | segSetC | inactiveC;
    GPIOC->BCR  = activeClrC | segClrC;
    if (activeC == 1) { if (comState) GPIOA->BSHR = (1 << SENSE_BIT); else GPIOA->BCR = (1 << SENSE_BIT); }
    else GPIOA->BSHR = (1 << SENSE_BIT);

    GPIOC->BSHR = activeSetC | segSetC;
    GPIOC->BCR  = activeClrC | segClrC | inactiveC;
    if (activeC == 1) { if (comState) GPIOA->BSHR = (1 << SENSE_BIT); else GPIOA->BCR = (1 << SENSE_BIT); }
    else GPIOA->BCR = (1 << SENSE_BIT);
  }
}

void UpdateLCD(int num) {
  uint16_t pat = font3x3[num & 0x0F];

  GPIOA->CFGLR = (GPIOA->CFGLR & ~A_CFG_CLEAR) | A_CFG_OUT;
  GPIOC->CFGLR = (GPIOC->CFGLR & ~GROUP_C_CFG_CLEAR) | GROUP_C_CFG_OUT;

  for (int phase = 0; phase < 2; phase++) {
    for (int c = 0; c < 3; c++) {
      int segBits = (pat >> (c * 3)) & 0x07;
      driveComGroup(c, phase, segBits);
    }
  }

  GPIOA->BCR = (1 << SENSE_BIT);
  GPIOC->BCR = GROUP_C_MASK;
}

void WaitAndDriveLCD(int ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    UpdateLCD(dispNum);
  }
}

void blinkLed(int times) {
  for (int i = 0; i < times; i++) {
    GPIOC->BSHR = 1;
    WaitAndDriveLCD(120);
    GPIOC->BCR = 1;
    WaitAndDriveLCD(120);
  }
}

int adcSampleTouch(int bit) {
  uint32_t sh = bit * 4;

  GPIOA->CFGLR &= ~(0xFu << sh);
  GPIOA->CFGLR |=  (CH_OUT << sh);
  GPIOA->BCR = (1 << bit);

  GPIOC->CFGLR &= ~GROUP_C_CFG_CLEAR;
  GPIOC->CFGLR |=  GROUP_C_CFG_OUT;
  GPIOC->BCR = GROUP_C_MASK;

  delayMicroseconds(80);

  GPIOA->CFGLR &= ~(0xFu << sh);
  GPIOA->CFGLR |=  (CH_IN_PU << sh);
  GPIOA->OUTDR |= (1 << bit);

  GPIOC->CFGLR &= ~GROUP_C_CFG_CLEAR;
  GPIOC->CFGLR |=  GROUP_C_CFG_INPU;
  GPIOC->OUTDR |= GROUP_C_MASK;

  ADC1->CTLR2 = ADC_SWSTART | ADC_ADON | ADC_EXTSEL;
  while (!(ADC1->STATR & ADC_EOC));
  return ADC1->RDATAR;
}

int readCap(int bit) {
  long s = 0;
  for (int i = 0; i < 32; i++) s += adcSampleTouch(bit);
  return (int)s;
}

int evalTouch(int raw) {
  int d = baseA0[dispNum] - raw;
  if (d < 0) d = 0;
  if (touchLatch) {
    if (d < TOUCH_OFF) touchLatch = 0;
  } else {
    if (d >= TOUCH_MIN) touchLatch = 1;
  }
  return touchLatch;
}

int rawToPercent(int raw) {
  int d_raw = baseA0[dispNum] - raw;

  if (maxDiff <= 0) return 100;

  long p = ((long)d_raw * 100L) / (long)maxDiff;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)p;
}

void smoothReset() {
  smoothIdx = 0;
  smoothCount = 0;
  smoothSum = 0;
}

int smoothPush(int raw) {
  if (smoothCount < SMOOTH_N) {
    smoothBuf[smoothIdx] = raw;
    smoothSum += raw;
    smoothCount++;
  } else {
    smoothSum -= smoothBuf[smoothIdx];
    smoothSum += raw;
    smoothBuf[smoothIdx] = raw;
  }
  smoothIdx = (smoothIdx + 1) % SMOOTH_N;
  return (int)(smoothSum / smoothCount);
}

void printRawD(int raw, const char* tag) {
  int d = baseA0[dispNum] - raw;
  uartSendString(tag);
  uartSendString(" raw=");
  uartSendInt(raw);
  uartSendString(" d=");
  uartSendInt(d);
  uartSendString("\r\n");
}

void setup() {
  setupHardwareUart();
  RCC->APB2PCENR |= (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC);
  GPIOC->CFGLR &= ~0xFu;
  GPIOC->CFGLR |= CH_OUT;
  setupPwmPc3();
  setPwmPercent(0);
  setupAdcTouch();

  uartSendString("=== LCD slider Ver0.34 (hold with LCD) ===\r\n");
  uartSendString("Calibrating ALL digits...\r\n");
  
  for (int d = 0; d < 16; d++) {
    dispNum = d;
    WaitAndDriveLCD(250);
    
    for (int i = 0; i < 5; i++) {
      readCap(SENSE_BIT);
    }
    
    baseA0[d] = readCap(SENSE_BIT);
    
    uartSendString("Base [");
    uartSendInt(d);
    uartSendString("]=");
    uartSendInt(baseA0[d]);
    uartSendString("\r\n");
  }
  
  dispNum = 0;
  WaitAndDriveLCD(250);
  for (int i = 0; i < 5; i++) {
    readCap(SENSE_BIT);
  }

  blinkLed(1);
  WaitAndDriveLCD(250);
  for (int i = 0; i < 5; i++) {
    readCap(SENSE_BIT);
  }
  baseA0[0] = readCap(SENSE_BIT);
  uartSendString("Base0 wait=");
  uartSendInt(baseA0[0]);
  uartSendString("\r\n");

  calState = 1;
  touchLatch = 0;
  armStart = millis();
  uartSendString("Touch MAX\r\n");
}

void loop() {
  int raw = readCap(SENSE_BIT);
  unsigned long now = millis();
  int touched = evalTouch(raw);

  static int printTimer = 0;
  printTimer++;

  if (calState == 1) {
    if (printTimer >= 20) {
      printRawD(raw, "WAIT");
      printTimer = 0;
    }
    if (touched && (now - armStart) >= ARM_DELAY_MS) {
      phaseStart = now;
      calSum = 0;
      calCount = 0;
      holdMiss = 0;
      calState = 2;
      uartSendString("MAX hold\r\n");
    }
    WaitAndDriveLCD(20);
    return;
  }

  if (calState == 2) {
    if (printTimer >= 20) {
      printRawD(raw, "HOLD");
      printTimer = 0;
    }
    if (!touched) {
      holdMiss++;
      if (holdMiss >= HOLD_MISS_MAX) {
        calState = 1;
        touchLatch = 0;
        armStart = millis();
        uartSendString("Touch MAX\r\n");
        WaitAndDriveLCD(20);
        return;
      }
    } else {
      holdMiss = 0;
    }
    unsigned long elapsed = now - phaseStart;
    if (elapsed >= WIN_START_MS && elapsed <= STABLE_MS) {
      calSum += raw;
      calCount++;
    }
    if (elapsed < STABLE_MS) {
      WaitAndDriveLCD(20);
      return;
    }
    
    int maxRaw = (calCount > 0) ? (int)(calSum / calCount) : raw;
    maxDiff = baseA0[dispNum] - maxRaw;
    if (maxDiff < TOUCH_MIN) maxDiff = TOUCH_MIN;
    
    uartSendString("MAX_DIFF=");
    uartSendInt(maxDiff);
    uartSendString("\r\n");
    
    blinkLed(2);
    phaseStart = millis();
    calState = 3;
    uartSendString("Release wait\r\n");
    WaitAndDriveLCD(20);
    return;
  }

  if (calState == 3) {
    if (printTimer >= 20) {
      printRawD(raw, "REL ");
      printTimer = 0;
    }
    if (touched) {
      phaseStart = now;
    } else if ((now - phaseStart) >= STABLE_MS) {
      calState = 4;
      smoothReset();
      printTimer = 0;
      uartSendString("RUN\r\n");
    }
    WaitAndDriveLCD(20);
    return;
  }

  int currentState = 0;
  const char* pos = "NONE ";
  int sliderPos = 0;

  if (!touched) {
    currentState = 0;
    pos = "NONE ";
    smoothReset();
    GPIOC->BCR = 1;
    setPwmPercent(0);
  } else {
    currentState = 1;
    pos = "TOUCH";
    int avgRaw = smoothPush(raw);
    sliderPos = rawToPercent(avgRaw);
    GPIOC->BSHR = 1;
    setPwmPercent(sliderPos);
  }

  static int lastCurrentState = 0;
  if (currentState == 1 && lastCurrentState == 0) {
    dispNum = (dispNum + 1) % 16;
  }
  lastCurrentState = currentState;

  if (printTimer >= 50) {
    uartSendString("raw=");
    uartSendInt(raw);
    uartSendString(" d=");
    uartSendInt(baseA0[dispNum] - raw);
    uartSendChar(' ');
    uartSendString(pos);

    if (currentState != 0) {
      uartSendString(" POS=");
      uartSendInt(sliderPos);
      uartSendString("%");
    }

    uartSendString("\r\n");
    printTimer = 0;
  }

  WaitAndDriveLCD(20);
}

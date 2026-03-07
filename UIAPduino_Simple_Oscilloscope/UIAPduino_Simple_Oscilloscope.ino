/*
 * UIAPduino Simple Oscilloscope (Direct I2C Mode)
 * Version: 1.07
 * Date: 2026-03-07
 * Developers: Gemini & yas
 * * Pin Assignments (UIAPduino / CH32V003F4P6):
 * Pin 1  (PD4) : Unused
 * Pin 2  (PD5) : Unused
 * Pin 3  (PD6) : Unused
 * Pin 4  (PD7) : RST (Reset)
 * Pin 5  (PA1) : Unused
 * Pin 6  (PA2) : Unused
 * Pin 7  (GND) : Ground
 * Pin 8  (VDD) : Power Supply (3.3V/5V)
 * Pin 9  (PC1) : I2C SDA (OLED) / SWDIO (Programming)
 * Pin 10 (PC2) : Analog Input A2 (TL431 2.49V Reference) / I2C SCL (If remapped, check board spec)
 * Pin 11 (PC3) : Unused
 * Pin 12 (PC4) : Analog Input A0 (Oscilloscope Input, 100k/10k Divider)
 * Pin 13 (PC5) : SW1 (FUN / Auto Zero Long Press)
 * Pin 14 (PC6) : SW2 (UP / +)
 * Pin 15 (PC7) : SW3 (DOWN / -)
 * Pin 16 (PD1) : Unused / I2C SCL (Alternative)
 * * History:
 * 0.xx - 1.45 (Beta phase): 
 * - Optimized flash memory usage to fit within the strict 16KB limit (97% used).
 * - Implemented direct I2C OLED driving, high-speed ADC sampling (24MHz), and waveform interpolation.
 * - Added trigger edge selection (Rise/Fall), adjustable trigger levels (5-step), and auto-zero calibration.
 * - Resolved UI rendering bugs, finalized the 101px Golden Ratio layout, and implemented XOR/OR logic for UI elements.
 * 1.00 (2026-03-07): Initial Release. Finalized as a dedicated, battery-driven simple oscilloscope with absolute visibility trigger marker.
 * 1.01 (2026-03-07): Fixed a critical bug in DC mode where current_min was incorrectly offset. Now correctly sets the bottom of the screen to 0V.
 * 1.02 (2026-03-07): (Build Failed - ROM Overflow) Attempted to implement pre-sampling dynamic centering for AC mode.
 * 1.03 (2026-03-07): Resolved 72-byte flash overflow from V1.02. Implemented zero-overhead feedback centering (calculates true center during main sampling for the next frame without extra arrays or loops). Inlined and batched I2C commands to eliminate function call overhead, significantly saving ROM and increasing FPS.
 * 1.04 (2026-03-07): Fixed missing trigger marker in DC mode. The trigger level is now properly anchored to the center of the selected voltage scale (range >> 1) instead of the AC wave center.
 * 1.05 (2026-03-07): Added trigger auto-return. Changing the voltage (scale_mode) or time (time_mode) scale now automatically resets trig_offset to 0, bringing the trigger marker back to the center (near the input waveform) to prevent it from getting lost off-screen.
 * 1.06 (2026-03-07): Public Release. Added comprehensive beginner-friendly comments explaining the extreme optimization techniques.
 * 1.07 (2026-03-07): Updated calibration instructions in comments. Added a crucial note that A0 must be open and shorted to A2 (2.49V VREF) during the SW1 long-press auto-calibration for accurate centering.
 */

#include <Wire.h>

#define OLED_ADDR 0x3C
#define SW1_PIN PC5
#define SW2_PIN PC6
#define SW3_PIN PC7

// フォントデータ: 5x7ピクセルの極小フォント。ROM節約のため必要な文字だけを厳選。
// (PROGMEMはAVR用ですが、CH32Vのコンパイラでもフラッシュに配置されます)
const uint8_t font[20][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
  {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
  {0x00, 0x60, 0x60, 0x00, 0x00}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
  {0x7F, 0x09, 0x09, 0x01, 0x01}, {0x01, 0x01, 0x7F, 0x01, 0x01},
  {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x3E, 0x41, 0x41, 0x41, 0x22},
  {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x04, 0x02, 0x7F, 0x02, 0x04},
  {0x20, 0x40, 0x7F, 0x40, 0x20}, {0x00, 0x00, 0x00, 0x00, 0x00}
};

// オシロのスケール設定群
const int offset[4] = {279, 112, 56, 28}; // 電圧スケールの係数
const unsigned int t_wait[4] = {0, 5, 20, 50}; // サンプリング時のウェイト(us)

// UIの文字マッピングデータ
const uint8_t vStrs[4][4] = {{11,5,10,0},{11,2,10,0},{11,1,10,0},{11,0,10,5}};
const uint8_t tStrs[4][4] = {{12,19,1,0},{12,19,2,0},{12,19,5,0},{12,1,0,0}};
const uint8_t trigStr[2][4] = {{13,19,19,18},{13,19,19,17}};
const uint8_t acdcStr[2][4] = {{19,14,15,19},{19,16,15,19}};

uint8_t scale_mode = 0, time_mode = 0, ui_mode = 0;
bool is_dc_mode = false, trig_edge = true, is_long_press = false;
int trig_offset = 0, center_val = 510, stored_ref_val = 510, wave_center = 510;
uint8_t prev_sw1 = HIGH, prev_sw2 = HIGH, prev_sw3 = HIGH;
unsigned long sw_press_time = 0;

void setup() {
  Wire.begin();
  Wire.setClock(400000); // I2Cを400kHz(Fast Mode)に設定して描画を高速化
  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);

  // 【最重要チューニング: ADCのオーバークロック】
  // CH32V003のADCクロック分周を DIV2 に設定 (システムクロック48MHz / 2 = 24MHz)
  // これにより analogRead() が極めて高速になり、オシロスコープとしての時間分解能が向上します。
  RCC_ADCCLKConfig(RCC_PCLK2_Div2);

  // 【I2Cコマンドの一筆書き送信】
  // SSD1306(OLED)への初期化コマンドを、通信のオーバーヘッドを無くすために連続送信します。
  // 関数呼び出しの回数を減らすことで、フラッシュメモリ(ROM)を大幅に節約しています。
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(0xAE); Wire.write(0x20); Wire.write(0x00); // ディスプレイOFF, メモリモード設定
  Wire.write(0x8D); Wire.write(0x14); Wire.write(0xAF); // チャージポンプ有効化, ディスプレイON
  Wire.endTransmission();
}

void loop() {
  // 【TL431によるVDDノイズキャンセラー】
  // USB電源等の電圧(VDD)が変動するとADCの値もブレてしまいます。
  // そこで、A2ピンに繋いだ高精度シャントレギュレータTL431(2.49V)を基準(モノサシ)として読み込み、
  // ソフトウェアの計算でVDDのブレを逆算して相殺しています。
  int ref_val = analogRead(A2);
  if (ref_val < 100) ref_val = 510; // TL431未接続時のフェイルセーフ
  int current_center = (center_val * (long)ref_val) / stored_ref_val;

  // DCモード時は画面の基準を2.5Vに固定、ACモード時は入力波形の中心に動的に追従させます。
  if (is_dc_mode) wave_center = current_center;

  // UI入力処理 (チャタリング防止と長押し判定)
  uint8_t cur_sw1 = digitalRead(SW1_PIN);
  uint8_t cur_sw2 = digitalRead(SW2_PIN);
  uint8_t cur_sw3 = digitalRead(SW3_PIN);
  
  if (cur_sw1 == LOW && prev_sw1 == HIGH) { sw_press_time = millis(); is_long_press = false; delay(20); }
  else if (cur_sw1 == LOW && prev_sw1 == LOW && !is_long_press && (millis() - sw_press_time >= 1000)) {
      // SW1長押し: オート・キャリブレーション (センター位置の校正)
      // 【重要】入力(A0)をオープンにし、A0ピンとA2ピン(TL431基準電圧)をジャンパー等でショートさせた状態で
      // 長押ししてください。これにより正確な2.49Vをオシロのセンター(中心)として記憶します。
      center_val = analogRead(A0); stored_ref_val = ref_val; trig_offset = 0; is_long_press = true;
  } else if (cur_sw1 == HIGH && prev_sw1 == LOW) { if (!is_long_press) ui_mode = (ui_mode + 1) % 5; delay(20); }
  prev_sw1 = cur_sw1;
  
  // スケール変更時（ui_mode 0 または 1）に、迷子防止のためトリガー位置(trig_offset)を0にリセットします
  if (cur_sw2 == LOW && prev_sw2 == HIGH) {
    if (ui_mode == 0) { scale_mode = (scale_mode + 1) % 4; trig_offset = 0; } else if (ui_mode == 1) { time_mode = (time_mode + 1) % 4; trig_offset = 0; }
    else if (ui_mode == 2) trig_offset += 5; else if (ui_mode == 3) trig_edge = !trig_edge; else if (ui_mode == 4) is_dc_mode = !is_dc_mode;
    delay(20);
  }
  prev_sw2 = cur_sw2;
  
  if (cur_sw3 == LOW && prev_sw3 == HIGH) {
    if (ui_mode == 0) { scale_mode = (scale_mode + 3) % 4; trig_offset = 0; } else if (ui_mode == 1) { time_mode = (time_mode + 3) % 4; trig_offset = 0; }
    else if (ui_mode == 2) trig_offset -= 5; else if (ui_mode == 3) trig_edge = !trig_edge; else if (ui_mode == 4) is_dc_mode = !is_dc_mode;
    delay(20);
  }
  prev_sw3 = cur_sw3;

  long range = (offset[scale_mode] * 2 * (long)ref_val) / 510;
  // DCモードは画面の一番下を0V(GND)に、ACモードは波形の中心を画面中央に設定
  long current_min = is_dc_mode ? 0 : wave_center - (range >> 1);

  // 【トリガー処理】
  // DCモード時はスケールの中心を、ACモード時は波形の中心をトリガーの基準点(アンカー)にします
  int trig_level = (is_dc_mode ? (range >> 1) : wave_center) + trig_offset;
  int arm_level = trig_level - (trig_edge ? 10 : -10); // ノイズによる誤動作を防ぐヒステリシス(遊び)
  uint16_t timeout = 1500; // タイムアウトを設定してフリーズを防止
  
  // トリガーの準備(Arm): 指定したエッジの反対側に波形が来るまで待機
  while(--timeout && (((analogRead(A0) >= arm_level) ^ !trig_edge)));
  
  timeout = 1500;
  int prev_v = analogRead(A0);
  // トリガー発動待ち: 波形が指定した電圧(trig_level)を指定した方向(立ち上がり/立ち下がり)にクロスする瞬間を捕獲
  while(--timeout) {
    int cur_v = analogRead(A0);
    if (trig_edge ? (prev_v < trig_level && cur_v >= trig_level) : (prev_v > trig_level && cur_v <= trig_level)) break;
    prev_v = cur_v;
  }

  // 【サンプリング ＆ AC動的センタリングのオンザフライ計算】
  uint8_t wave_buf[101];
  unsigned int tw = t_wait[time_mode];
  int max_val = 0, min_val = 1023;
  
  for (uint8_t i = 0; i < 101; i++) {
    int raw = analogRead(A0);
    // ACモード時のみ、サンプリングと同時に波形の最大・最小値を記録(ROMとRAMの節約)
    if (!is_dc_mode) {
      if (raw > max_val) max_val = raw;
      if (raw < min_val) min_val = raw;
    }
    // 取得した電圧を、OLEDの縦幅(Y座標: 2〜61)にスケーリング
    int y = 61 - ((raw - current_min) * 59L) / range;
    wave_buf[i] = (y < 2) ? 2 : (y > 61 ? 61 : y);
    if(tw) delayMicroseconds(tw);
  }
  
  // 次回のフレームのために波形の中心を更新（フィードバック・センタリング）
  // これにより、事前サンプリングの遅延なしで波形が常に画面中央へ吸い込まれます。
  if (!is_dc_mode) {
    wave_center = (max_val + min_val) / 2;
  }

  int trig_y = 61 - ((trig_level - current_min) * 59L) / range;
  trig_y = (trig_y < 2) ? 2 : (trig_y > 61 ? 61 : trig_y);

  // 【描画処理（I2C一筆書き最適化）】
  // 描画範囲をOLED全体にリセットします
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(0x21); Wire.write(0); Wire.write(127);
  Wire.write(0x22); Wire.write(0); Wire.write(7);
  Wire.endTransmission();

  for (uint8_t page = 0; page < 8; page++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40); // 以降は画面描画用データであることを宣言
    for (uint8_t col = 0; col < 128; col++) {
      uint8_t outByte = 0x00;
      
      if (col < 24) { 
        // 【UIテキストエリアの描画】
        if (!(page & 1)) { 
          uint8_t c = col / 6, b = col % 6;
          if (b < 5) {
            uint8_t f_idx = 19; // 空白
            if (page == 0) f_idx = vStrs[scale_mode][c];
            else if (page == 2) f_idx = tStrs[time_mode][c];
            else if (page == 4) f_idx = trigStr[trig_edge][c];
            else if (page == 6) f_idx = acdcStr[is_dc_mode][c];
            outByte |= font[f_idx][b];
          }
          // 選択中のUI項目をビット反転(XOR)してハイライト表示
          if ((ui_mode == 0 && page == 0) || (ui_mode == 1 && page == 2) || (ui_mode == 4 && page == 6)) outByte ^= 0xFF;
          else if (page == 4) { if ((ui_mode == 2 && c <= 1) || (ui_mode == 3 && c >= 2)) outByte ^= 0xFF; }
        }
      } else if (col <= 126) { 
        // 【波形・グリッドエリアの描画】
        if (page == 0) outByte |= 0x03; // 上枠
        if (page == 7) outByte |= 0xC0; // 下枠
        if (col == 24 || col == 126) outByte |= 0xFF; // 左右枠
        else {
          if (col == 75) outByte |= 0x55; // センターライン(点線)
          else if ((col - 75) % 10 == 0) { // 10ピクセルごとのグリッド(点線)
            if (page == 1 || page == 6) outByte |= 0x08;
            if (page == 2) outByte |= 0x20; if (page == 5) outByte |= 0x02;
          }
          if (page == 3 && (col & 1)) outByte |= 0x80;

          // 【波形の線形補間描画】
          // 点と点の間を埋めるように縦線を引くことで、高速な波形でも途切れないようにします。
          uint8_t idx = col - 25;
          uint8_t y = wave_buf[idx];
          uint8_t py = (idx > 0) ? wave_buf[idx-1] : y;
          uint8_t min_y = (y < py) ? y : py;
          uint8_t max_y = (y > py) ? y : py;
          uint8_t p_top = page << 3;
          uint8_t p_bot = p_top | 7;
          if (!(max_y < p_top || min_y > p_bot)) {
            uint8_t start = (min_y < p_top) ? 0 : (min_y & 7);
            uint8_t end = (max_y > p_bot) ? 7 : (max_y & 7);
            for (uint8_t b = start; b <= end; b++) outByte ^= (1 << b); // グリッドの上に波形をXORで上書き
          }

          // 【トリガーマーカーの描画】
          // 視認性を上げるため巨大化し、OR合成(|)で波形や枠線を強制的に白く塗りつぶします
          if (col >= 25 && col <= 27) {
            int half_width = (col == 25) ? 2 : (col == 26 ? 1 : 0);
            for (int i = -half_width; i <= half_width; i++) {
              int py = trig_y + i;
              if (py >= (page << 3) && py < ((page + 1) << 3)) outByte |= (1 << (py & 7));
            }
          }
        }
      }
      Wire.write(outByte);
      
      // I2Cバッファあふれ防止のための定期的な送信区切り
      if ((col & 15) == 15) { 
        Wire.endTransmission(); 
        if (col != 127) {
          Wire.beginTransmission(OLED_ADDR); 
          Wire.write(0x40); 
        }
      }
    }
    Wire.endTransmission();
  }
}
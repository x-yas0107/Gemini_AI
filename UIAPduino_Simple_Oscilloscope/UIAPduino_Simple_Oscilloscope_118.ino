/*
 * UIAPduino Simple Oscilloscope (Direct I2C Mode)
 * Version: 1.18 (Public Release Version)
 * Date: 2026-03-08
 * Developers: Gemini & yas
 * * ==========================================
 * ピン配置 (CH32V003F4P6 / UIAPduino)
 * ==========================================
 * Pin 4  (PD7) : RST (リセット)
 * Pin 7  (GND) : GND
 * Pin 8  (VDD) : 電源 (3.3V/5V)
 * Pin 9  (PC1) : I2C SDA (OLEDディスプレイ用)
 * Pin 10 (PC2) : アナログ入力 A2 (TL431 2.49V 基準電圧入力)
 * Pin 12 (PC4) : アナログ入力 A0 (オシロスコープ入力プローブ / 100kΩ・10kΩ 分圧)
 * Pin 13 (PC5) : SW1 (FUNボタン / 長押しでオートゼロ・キャリブレーション)
 * Pin 14 (PC6) : SW2 (UP / ＋ボタン)
 * Pin 15 (PC7) : SW3 (DOWN / －ボタン)
 * * ==========================================
 * 開発・改変履歴 (History)
 * ==========================================
 * 1.00 - 1.11: 
 * - CH32V003の極小ROM(16KB)の限界に挑み、OLEDへのダイレクトI2C制御、24MHz高速サンプリングを実装。
 * - UIの最適化、波形補間、トリガー機能、オートセンタリングなどの基本機能を16KB内に収めることに成功。
 * 1.12 - 1.16: 
 * - シングルショット機能の実装に伴うROM容量オーバー（サイレントクラッシュ）を解消。
 * - 軽量な非同期ステートマシンを導入し、ACモード時のコンデンサ飽和を防ぐフェイルセーフ機能を追加。
 * 1.17: 
 * - UP/DOWNボタンによる3状態ロータリー切替（Run⇔Single⇔Hold）を実装。ACモード時はSingleを自動スキップする安全設計を確立。
 * 1.18: 
 * - 一般公開（リリース）に向けて、ヘッダー情報とコード内コメントを初心者にも分かりやすいように再整備・クリーンアップ。コードの論理自体はV1.17と完全同一。
 */

#include <Wire.h>

#define OLED_ADDR 0x3C
#define SW1_PIN PC5
#define SW2_PIN PC6
#define SW3_PIN PC7

// ==========================================
// フォントデータ (5x7ドットマトリクス)
// シングルショット用の 'S', 'H', 'R' を含む全23文字
// ==========================================
const uint8_t font[23][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
  {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
  {0x00, 0x60, 0x60, 0x00, 0x00}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
  {0x7F, 0x09, 0x09, 0x01, 0x01}, {0x01, 0x01, 0x7F, 0x01, 0x01},
  {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x3E, 0x41, 0x41, 0x41, 0x22},
  {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x04, 0x02, 0x7F, 0x02, 0x04},
  {0x20, 0x40, 0x7F, 0x40, 0x20}, {0x00, 0x00, 0x00, 0x00, 0x00},
  {0x26, 0x49, 0x49, 0x49, 0x32}, // 20: 'S' (Single Wait / 単発待機)
  {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 21: 'H' (Hold / 一時停止)
  {0x7F, 0x09, 0x19, 0x29, 0x46}  // 22: 'R' (Run / 連続サンプリング)
};

// ==========================================
// オシロスコープの測定スケール設定
// ==========================================
// 電圧スケール (表示上の縦幅倍率)
const int offset[4] = {279, 112, 56, 28};
// 時間スケール (サンプリング間の待機マイクロ秒数)
const unsigned int t_wait[4] = {0, 5, 20, 50};

// ==========================================
// UI画面表示用の文字インデックスマッピング
// ==========================================
const uint8_t vStrs[4][4] = {{11,5,10,0},{11,2,10,0},{11,1,10,0},{11,0,10,5}};  // V/div表示
const uint8_t tStrs[4][4] = {{12,19,1,0},{12,19,2,0},{12,19,5,0},{12,1,0,0}};  // ms/div表示
const uint8_t trigStr[2][4] = {{13,19,19,18},{13,19,19,17}};                   // トリガー矢印表示
const uint8_t acdcStr[2][4] = {{19,14,15,19},{19,16,15,19}};                   // AC/DC表示

// ==========================================
// グローバル変数群 (状態管理)
// ==========================================
uint8_t scale_mode = 0, time_mode = 0, ui_mode = 0;
uint8_t run_mode = 0; // 0:RUN(連続), 1:SINGLE(単発待機), 2:HOLD(一時停止)
bool is_dc_mode = false, trig_edge = true, is_long_press = false;
int trig_offset = 0, center_val = 510, stored_ref_val = 510, wave_center = 510;
uint8_t prev_sw1 = HIGH, prev_sw2 = HIGH, prev_sw3 = HIGH;
unsigned long sw_press_time = 0;
uint8_t wave_buf[101]; // 画面に描画する波形データ(101ピクセル分)を保存する配列

void setup() {
  Wire.begin();
  Wire.setClock(400000); // I2C通信を高速モード(400kHz)に設定し描画速度を稼ぐ
  
  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);

  // ADCのクロックを分周し、マイコンの動作に最適な速度に設定
  RCC_ADCCLKConfig(RCC_PCLK2_Div2);

  // 【ROM節約】OLEDの初期化コマンドを配列にまとめ、ループで一筆書き送信
  const uint8_t init_cmd[] = {0x00, 0xAE, 0x20, 0x00, 0x8D, 0x14, 0xAF};
  Wire.beginTransmission(OLED_ADDR);
  for (uint8_t i = 0; i < sizeof(init_cmd); i++) Wire.write(init_cmd[i]);
  Wire.endTransmission();
}

void loop() {
  // A2ピンから基準電圧(TL431)を読み取り、現在の電源電圧の変動を補正
  int ref_val = analogRead(A2);
  if (ref_val < 100) ref_val = 510; // 異常値のフェイルセーフ
  int current_center = (center_val * (long)ref_val) / stored_ref_val;

  // DCモードの場合は波形の中心位置を基準電圧に固定
  if (is_dc_mode) wave_center = current_center;

  // ==========================================
  // 1. ボタン入力とUI状態の更新処理
  // ==========================================
  uint8_t cur_sw1 = digitalRead(SW1_PIN);
  uint8_t cur_sw2 = digitalRead(SW2_PIN);
  uint8_t cur_sw3 = digitalRead(SW3_PIN);
  
  // SW1 (FUNボタン) : 短押しで項目移動、長押し(1秒)でゼロ点キャリブレーション
  if (cur_sw1 == LOW && prev_sw1 == HIGH) { sw_press_time = millis(); is_long_press = false; delay(20); }
  else if (cur_sw1 == LOW && prev_sw1 == LOW && !is_long_press && (millis() - sw_press_time >= 1000)) {
      center_val = analogRead(A0); stored_ref_val = ref_val; trig_offset = 0; is_long_press = true;
  } else if (cur_sw1 == HIGH && prev_sw1 == LOW) { if (!is_long_press) ui_mode = (ui_mode + 1) % 6; delay(20); }
  prev_sw1 = cur_sw1;
  
  // SW2 (UP) と SW3 (DOWN) : 選択中の項目の数値を増減する
  uint8_t dir = 0;
  if (cur_sw2 == LOW && prev_sw2 == HIGH) dir = 1; // UP方向
  else if (cur_sw3 == LOW && prev_sw3 == HIGH) dir = 3; // DOWN方向

  if (dir) {
    if (ui_mode == 0) { scale_mode = (scale_mode + dir) & 3; trig_offset = 0; }
    else if (ui_mode == 1) { time_mode = (time_mode + dir) & 3; trig_offset = 0; }
    else if (ui_mode == 2) trig_offset += (dir == 1 ? 5 : -5);
    else if (ui_mode == 3) {
      // 動作モード切替: 3状態ロータリー (Run -> Single -> Hold)
      // dir==1(UP)なら順送り、dir==3(DOWN)なら逆送り
      run_mode = (run_mode + (dir == 1 ? 1 : 2)) % 3;
      // 【安全装置】ACモード時はコンデンサ飽和を防ぐためSingle(1)をスキップする
      if (!is_dc_mode && run_mode == 1) {
        run_mode = (run_mode + (dir == 1 ? 1 : 2)) % 3;
      }
    }
    else if (ui_mode == 4) trig_edge = !trig_edge;             
    else if (ui_mode == 5) is_dc_mode = !is_dc_mode;           
    delay(20); // チャタリング防止
  }
  prev_sw2 = cur_sw2;
  prev_sw3 = cur_sw3;

  // 画面描画用のスケール計算
  long range = (offset[scale_mode] * 2 * (long)ref_val) / 510;
  long current_min = is_dc_mode ? 0 : wave_center - (range >> 1);
  int trig_level = (is_dc_mode ? (range >> 1) : wave_center) + trig_offset;

  // ==========================================
  // 2. 波形のサンプリング処理 (ADC読み取り)
  // ==========================================
  // HOLDモード(2)の時はADCを読まず、波形配列(wave_buf)のデータを維持(フリーズ)する
  if (run_mode != 2) {
    int arm_level = trig_level - (trig_edge ? 10 : -10); // ヒステリシス(ノイズ誤動作防止)
    uint16_t timeout = 1500;
    
    // トリガー準備(ARM)待ち：波形が一度逆方向に振れるのを待つ
    while(--timeout && (((analogRead(A0) >= arm_level) ^ !trig_edge)));
    
    timeout = 1500;
    int prev_v = analogRead(A0);
    bool triggered = false; 
    
    // トリガー(TRIG)待ち：波形が指定レベルを指定方向に横切るのを待つ
    while(--timeout) {
      int cur_v = analogRead(A0);
      if (trig_edge ? (prev_v < trig_level && cur_v >= trig_level) : (prev_v > trig_level && cur_v <= trig_level)) {
        triggered = true; // トリガー条件成立
        break;
      }
      prev_v = cur_v;
    }

    // RUN(0)は常に波形を取得、SINGLE(1)はトリガーが掛かった瞬間だけ波形を取得する
    if (run_mode == 0 || (run_mode == 1 && triggered)) {
      unsigned int tw = t_wait[time_mode];
      int max_val = 0, min_val = 1023;
      
      // 画面の横幅分(101ピクセル)、超高速で波形を読み取る
      for (uint8_t i = 0; i < 101; i++) {
        int raw = analogRead(A0);
        if (!is_dc_mode) { // ACモード用の最大/最小値取得
          if (raw > max_val) max_val = raw;
          if (raw < min_val) min_val = raw;
        }
        // 読み取った電圧値をOLEDのY座標(2～61)にマッピング
        int y = 61 - ((raw - current_min) * 59L) / range;
        wave_buf[i] = (y < 2) ? 2 : (y > 61 ? 61 : y);
        if(tw) delayMicroseconds(tw); // 時間スケールに応じた待機
      }
      
      // ACモードの時は波形の上下の真ん中を画面の中心に自動追従させる (オートセンタリング)
      if (!is_dc_mode) {
        wave_center = (max_val + min_val) / 2;
      }

      // SINGLE(1)モードで波形を捕獲完了したら、自動的にHOLD(2)モードに切り替えて画面を止める
      if (run_mode == 1 && triggered) run_mode = 2; 
    }
  }

  // トリガーラインのY座標計算
  int trig_y = 61 - ((trig_level - current_min) * 59L) / range;
  trig_y = (trig_y < 2) ? 2 : (trig_y > 61 ? 61 : trig_y);

  // ==========================================
  // 3. OLED画面描画処理 (ダイレクトI2C制御)
  // ==========================================
  // 描画範囲を画面全体に設定
  const uint8_t win_cmd[] = {0x00, 0x21, 0, 127, 0x22, 0, 7};
  Wire.beginTransmission(OLED_ADDR);
  for (uint8_t i = 0; i < sizeof(win_cmd); i++) Wire.write(win_cmd[i]);
  Wire.endTransmission();

  // 画面を縦8分割(ページ)して左から右へ順に描画していく
  for (uint8_t page = 0; page < 8; page++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40); // データ送信モード
    
    for (uint8_t col = 0; col < 128; col++) {
      uint8_t outByte = 0x00; // この1バイトが縦8ピクセルの白黒を表す
      
      // 左側(0～23列目) : メニューや数値などの文字UIエリア
      if (col < 24) { 
        if (!(page & 1)) { 
          uint8_t c = col / 6, b = col % 6;
          if (b < 5) {
            uint8_t f_idx = 19; // デフォルトは空白
            if (page == 0) f_idx = vStrs[scale_mode][c];
            else if (page == 2) f_idx = tStrs[time_mode][c];
            else if (page == 4) {
              f_idx = trigStr[trig_edge][c];
              if (c == 2) { // 動作モード表示 (R, S, H)
                if (run_mode == 0) f_idx = 22;      // 'R'
                else if (run_mode == 1) f_idx = 20; // 'S'
                else if (run_mode == 2) f_idx = 21; // 'H'
              }
            }
            else if (page == 6) f_idx = acdcStr[is_dc_mode][c];
            outByte |= font[f_idx][b]; // フォントデータを書き込む
          }
          // 選択中の項目にアンダーライン(最下位ビット=0x80)を引いて強調表示する
          if ((ui_mode == 0 && page == 0) || (ui_mode == 1 && page == 2) || (ui_mode == 5 && page == 6)) outByte |= 0x80;
          else if (page == 4) { 
            if ((ui_mode == 2 && c <= 1) || (ui_mode == 3 && c == 2) || (ui_mode == 4 && c == 3)) outByte |= 0x80; 
          }
        }
      } 
      // 右側(24～126列目) : オシロ波形とグリッド線の描画エリア
      else if (col <= 126) { 
        if (page == 0) outByte |= 0x03; // 上枠線
        if (page == 7) outByte |= 0xC0; // 下枠線
        if (col == 24 || col == 126) outByte |= 0xFF; // 左右の枠線
        else {
          // グリッド線(点線)の描画
          if (col == 75) outByte |= 0x55; // 中央の縦軸
          else if ((col - 75) % 10 == 0) {
            if (page == 1 || page == 6) outByte |= 0x08;
            if (page == 2) outByte |= 0x20; if (page == 5) outByte |= 0x02;
          }
          if (page == 3 && (col & 1)) outByte |= 0x80; // 中央の横軸

          // 取得した波形データを繋げて線を描画する(補間処理)
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
            for (uint8_t b = start; b <= end; b++) outByte ^= (1 << b);
          }

          // トリガーレベルのマーカー(小突起)を描画
          if (col >= 25 && col <= 27) {
            int half_width = (col == 25) ? 2 : (col == 26 ? 1 : 0);
            for (int i = -half_width; i <= half_width; i++) {
              int py = trig_y + i;
              if (py >= (page << 3) && py < ((page + 1) << 3)) outByte |= (1 << (py & 7));
            }
          }
        }
      }
      Wire.write(outByte); // 計算した1列分のグラフィックデータをOLEDへ送信
      
      // ArduinoのI2Cバッファ上限(32バイト)を回避するための区切り処理
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
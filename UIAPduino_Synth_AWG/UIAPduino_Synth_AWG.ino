/*
 * =================================================================
 * Project: UIAPduino_Synth_AWG
 * Version: Ver 1.00
 * Date:    2026-03-29
 * Authors: yas & Gemini
 * 
 * [I/O接続設定 (物理Board番号対応)]
 * - Board 0 (PA1/ADC): ボリューム (音量 / X軸カーソル移動)
 * - Board 1 (PA2/ADC): ピッチ (音程 / Y軸カーソル移動) ※右回しで上昇
 * - Board 3 (PC1/I2C): SDA (OLEDディスプレイ データ通信)
 * - Board 4 (PC2/I2C): SCL (OLEDディスプレイ クロック)
 * - Board 6 (PC4/PWM): オーディオ出力 (スピーカー/アンプへ)
 * - Board 7 (PC5/DIG): ENTERボタン (点打ち / 長押し波形生成 / 3秒長押しでMENU)
 * - Board 8 (PC6/DIG): Xボタン (メニューでの波形切替)
 * - Board 9 (PC7/DIG): Yボタン (メニューでの波形切替)
 * - Board 10 (PD0/DIG): 状態表示LED (操作のフィードバック)
 * =================================================================
 */

#include <Arduino.h>

// --- ピンの役割設定 (Board番号に合わせたピン定義) ---
const int PIN_VOL_X    = PA1; // Board 0: アナログ入力 (X軸/音量)
const int PIN_VOL_Y    = PA2; // Board 1: アナログ入力 (Y軸/音程)
const int PIN_SW_ENTER = PC5; // Board 7: デジタル入力 (ENTERボタン)
const int PIN_SW_X     = PC6; // Board 8: デジタル入力 (Xボタン)
const int PIN_SW_Y     = PC7; // Board 9: デジタル入力 (Yボタン)
const int PIN_LED      = PD0; // Board 10: デジタル出力 (LED)

#define OLED_ADDR 0x3C // OLEDディスプレイのI2Cアドレス

// --- 高速I2C通信用の待機関数 (フリーズ防止のタイムアウト付き) ---
// 通信が完了するまで待ちます。20000回カウントしても終わらなければ異常とみなしループを抜けます。
bool i2c_wait_sb() { uint32_t t=20000; while(!(I2C1->STAR1 & I2C_STAR1_SB) && t--); return t>0; }
bool i2c_wait_txe() { uint32_t t=20000; while(!(I2C1->STAR1 & I2C_STAR1_TXE) && t--); return t>0; }
bool i2c_wait_btf() { uint32_t t=20000; while(!(I2C1->STAR1 & I2C_STAR1_BTF) && t--); return t>0; }

// --- I2C通信の初期設定 ---
void i2c_setup() {
    // I2C通信に必要な回路(GPIOC, I2C1など)の時計(クロック)を動かしてONにします
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
    
    // ピンのモードを「別の機能(I2C)」として使うように設定します
    GPIOC->CFGLR &= ~(0xFFF << 20); GPIOC->CFGLR |= (0x888 << 20); 
    GPIOC->OUTDR |= (1 << 5) | (1 << 6) | (1 << 7);
    GPIOC->CFGLR &= ~(0xFF << 4); GPIOC->CFGLR |= (0xEE << 4); 
    
    // I2Cのシステムをリセットして、通信速度を安全な約200kHz(CKCFGR=120)に設定します
    I2C1->CTLR1 |= I2C_CTLR1_SWRST; I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;
    I2C1->CTLR2 = 48; I2C1->CKCFGR = 120; I2C1->CTLR1 |= I2C_CTLR1_PE;
}

// --- I2C通信の開始・終了・送信 ---
void i2c_start() { I2C1->CTLR1 |= I2C_CTLR1_START; i2c_wait_sb(); } // 通信スタート
void i2c_stop() { i2c_wait_txe(); i2c_wait_btf(); I2C1->CTLR1 |= I2C_CTLR1_STOP; } // 通信ストップ
void i2c_write(uint8_t data) { I2C1->DATAR = data; i2c_wait_txe(); } // 1バイトのデータを送る

// --- OLEDへのコマンド送信と初期化 ---
void oled_cmd(uint8_t c) { i2c_start(); i2c_write(OLED_ADDR << 1); (void)I2C1->STAR2; i2c_write(0x00); i2c_write(c); i2c_stop(); }
void oled_init() { 
    delay(100); // 画面が起動するまで少し待つ
    // 画面を正常に映すための魔法の呪文(初期化コマンド群)
    uint8_t cmds[] = {0xAE, 0x20, 0x00, 0x81, 0x7F, 0xA0, 0xA6, 0xA8, 0x3F, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0x22, 0xDA, 0x12, 0xDB, 0x20, 0x8D, 0x14, 0xAF}; 
    for(uint8_t i=0; i<sizeof(cmds); i++) oled_cmd(cmds[i]); 
}

// --- 音の波形データと座標の管理 ---
const int WAVE_SIZE = 128; // 波形の長さ(画面の横幅と同じ)
uint8_t waveTable[WAVE_SIZE]; // 実際の音になる波形データを入れる箱
struct Point { int x; int y; }; // XとYの座標をセットにしたデータ型
Point anchors[20]; // ユーザーが打った点(アンカー)を最大20個まで記憶する箱
int anchorCount = 0; // 現在打たれている点の数
int curX = 64, curY = 127; // カーソルの現在位置 (初期位置は画面中央下)

// --- ボタン操作とLEDの管理用変数 ---
unsigned long btnPressStart = 0; // ボタンを押した瞬間の時間を記録
bool btnActive = false; // ボタンが押されているかどうかの状態
int lastLedStep = 0; // 長押しの段階(1秒、2秒、3秒)を記憶
unsigned long ledTurnOnTime = 0; // LEDを光らせた時間を記録
bool ledOn = false; // LEDが光っているかどうかの状態

// --- 音を鳴らすための変数 ---
volatile uint16_t phaseCounter = 0; // 波のどこを再生しているかのカウンター
volatile uint16_t phaseIncrement = 256; // 音の高さ(進むスピード)
volatile uint16_t gain = 255; // 音の大きさ(ボリューム)

HardwareTimer *sampleTimer; // 音を一定のテンポで出力するためのタイマー

int mode = 0; // 画面のモード (0: MENU画面, 1: PLAY画面)
int currentPreset = 0; // 現在選ばれているプリセット番号
bool btnX_prev = true; // Xボタンの前の状態(押しっぱなし防止用)
bool btnY_prev = true; // Yボタンの前の状態

// --- 画面に表示する文字のデータ ---
const char menuText[8][9] = {
    "1:SINE  ", "2:TRI   ", "3:SAW   ", "4:SQU   ",
    "5:ENV 1 ", "6:DAMP  ", "7:DUAL  ", "8:FREE  "
};

// --- 小さな文字(5x7サイズ)のドット絵データ ---
const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 0: Space
    {0x00,0x00,0x5F,0x00,0x00}, // 1: 1
    {0x42,0x61,0x51,0x49,0x46}, // 2: 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3: 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4: 4
    {0x27,0x45,0x45,0x45,0x39}, // 5: 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6: 6
    {0x01,0x71,0x09,0x05,0x03}, // 7: 7
    {0x36,0x49,0x49,0x49,0x36}, // 8: 8
    {0x00,0x36,0x36,0x00,0x00}, // 9: :
    {0x32,0x49,0x49,0x49,0x26}, // 10: S
    {0x00,0x41,0x7F,0x41,0x00}, // 11: I
    {0x7F,0x04,0x08,0x10,0x7F}, // 12: N
    {0x7F,0x49,0x49,0x49,0x41}, // 13: E
    {0x01,0x01,0x7F,0x01,0x01}, // 14: T
    {0x7F,0x09,0x19,0x29,0x46}, // 15: R
    {0x7E,0x11,0x11,0x11,0x7E}, // 16: A
    {0x3F,0x40,0x38,0x40,0x3F}, // 17: W
    {0x3E,0x41,0x51,0x21,0x5E}, // 18: Q
    {0x3F,0x40,0x40,0x40,0x3F}, // 19: U
    {0x1F,0x20,0x40,0x20,0x1F}, // 20: V
    {0x7F,0x09,0x09,0x09,0x01}, // 21: F
    {0x3E,0x41,0x41,0x41,0x22}, // 22: C
    {0x08,0x08,0x08,0x08,0x08}, // 23: -
    {0x7F,0x41,0x41,0x22,0x1C}, // 24: D
    {0x7F,0x02,0x0C,0x02,0x7F}, // 25: M
    {0x7F,0x09,0x09,0x09,0x06}, // 26: P
    {0x7F,0x01,0x01,0x01,0x01}  // 27: L
};

// 文字(char)をフォントデータの番号に変換する関数
int c2i(char c) {
    if(c==' ') return 0;
    if(c>='1' && c<='8') return c-'1'+1;
    if(c==':') return 9;
    if(c=='S') return 10; if(c=='I') return 11; if(c=='N') return 12; if(c=='E') return 13;
    if(c=='T') return 14; if(c=='R') return 15; if(c=='A') return 16; if(c=='W') return 17;
    if(c=='Q') return 18; if(c=='U') return 19; if(c=='V') return 20; if(c=='F') return 21;
    if(c=='C') return 22; if(c=='-') return 23;
    if(c=='D') return 24; if(c=='M') return 25; if(c=='P') return 26;
    if(c=='L') return 27;
    return 0;
}

// --- なめらかに線を繋ぐ計算(スプライン補間) ---
int get_spline(int t, int p0, int p1, int p2, int p3) {
    int32_t t2 = (t * t) >> 7;
    int32_t t3 = (t2 * t) >> 7;
    int32_t val = (2 * p1);
    val += (-p0 + p2) * t >> 7;
    val += (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 >> 7;
    val += (-p0 + 3 * p1 - 3 * p2 + p3) * t3 >> 7;
    return (int)(val >> 1);
}

// --- 打った点から波の形を作る処理 ---
void generate_wave(bool useSpline) {
    // 点が1つもない場合は、真っ平らな波(無音)にする
    if (anchorCount == 0) {
        for(int i=0; i<WAVE_SIZE; i++) waveTable[i] = 127;
        return;
    }
    // 点を左(Xが小さい順)から並べ替える
    for(int i=0; i<anchorCount; i++) {
        for(int j=i+1; j<anchorCount; j++) {
            if(anchors[i].x > anchors[j].x) { Point t=anchors[i]; anchors[i]=anchors[j]; anchors[j]=t; }
        }
    }
    
    // 計算用の大きな箱を用意(バッファオーバーフロー対策済の24個)
    Point p[24];
    p[0] = {0, 127}; p[1] = {0, 127}; // はじまりの点
    for(int i=0; i<anchorCount; i++) p[i+2] = anchors[i]; // 打った点を入れる
    p[anchorCount+2] = {127, 127}; p[anchorCount+3] = {127, 127}; // おわりの点

    // 点と点の間を埋めていくループ
    for(int i=1; i < anchorCount+2; i++) {
        int steps = p[i+1].x - p[i].x; // 次の点までの距離
        if (steps <= 0) continue;
        for(int s=0; s <= steps; s++) {
            int t = (s << 7) / steps;
            // useSplineがtrueなら滑らかな曲線、falseなら直線の計算をする
            int y = useSpline ? get_spline(t, p[i-1].y, p[i].y, p[i+1].y, p[i+2].y) : (p[i].y + (p[i+1].y - p[i].y) * s / steps);
            if (y < 0) y = 0; if (y > 255) y = 255; // 画面からはみ出ないように制限
            waveTable[p[i].x + s] = y; // 波のデータとして保存
        }
    }
}

// --- 内蔵されている波形(プリセット)を呼び出す ---
void load_preset(int p) {
    anchorCount = 0;
    bool useSpline = false; // 曲線を使うかどうかの設定
    switch(p) {
        case 0: // 1: SINE (サイン波 / なめらか)
            anchors[0]={32, 255}; anchors[1]={64, 127}; anchors[2]={96, 0};
            anchorCount = 3; useSpline = true; break;
        case 1: // 2: TRI (三角波 / カクカク)
            anchors[0]={32, 255}; anchors[1]={64, 127}; anchors[2]={96, 0};
            anchorCount = 3; break;
        case 2: // 3: SAW (ノコギリ波)
            anchors[0]={0, 255}; anchors[1]={127, 0}; 
            anchorCount = 2; break;
        case 3: // 4: SQU (矩形波・四角い波)
            anchors[0]={0, 255}; anchors[1]={63, 255}; anchors[2]={64, 0}; anchors[3]={127, 0};
            anchorCount = 4; break;
        case 4: // 5: ENV 1 (エンベロープ・減衰)
            anchors[0]={5, 255}; anchors[1]={30, 100}; anchors[2]={127, 0};
            anchorCount = 3; break;
        case 5: // 6: DAMP (減衰振動波形)
            anchors[0]={8, 255}; anchors[1]={24, 40}; anchors[2]={40, 200}; anchors[3]={56, 70};
            anchors[4]={72, 160}; anchors[5]={88, 100}; anchors[6]={104, 140}; anchors[7]={120, 115};
            anchorCount = 8; useSpline = true; break;
        case 6: // 7: DUAL (基本波＋2倍音の重畳サイン波)
            anchors[0]={16, 229}; anchors[1]={32, 187}; anchors[2]={48, 109}; anchors[3]={64, 127};
            anchors[4]={80, 145}; anchors[5]={96, 67}; anchors[6]={112, 25};
            anchorCount = 7; useSpline = true; break;
        case 7: // 8: FREE (空っぽ・自由入力)
            anchorCount = 0; break;
    }
    generate_wave(useSpline); // 選んだ点で波形を作る
}

// --- OLED画面の描画処理 ---
void refresh_display() {
    for (int p = 0; p < 8; p++) { // 画面を横長の8本の帯(ページ)に分けて描画する
        oled_cmd(0xB0 + p); oled_cmd(0x00); oled_cmd(0x10); // 描画する位置を指定
        i2c_start(); i2c_write(OLED_ADDR << 1); (void)I2C1->STAR2; i2c_write(0x40); // データ送信開始
        for (int x = 0; x < 128; x++) { // 左端(X=0)から右端(X=127)まで1ドットずつ送る
            uint8_t out = 0; // 縦8ドット分の光らせるデータを格納する箱
            bool drawWave = true;
            
            // メニュー画面の文字を描画する
            if (mode == 0 && p == 0) {
                if (x >= 40 && x < 40 + 48) {
                    int c_idx = (x - 40) / 6; // 何番目の文字か
                    int px = (x - 40) % 6; // 文字の中の何ドット目か
                    if (px < 5) {
                        char c = menuText[currentPreset][c_idx];
                        out = font5x7[c2i(c)][px]; // フォントデータから取り出す
                        if (out != 0) drawWave = false; // 文字がある場所は波を描かない
                    }
                }
            }
            
            // 波の線を描画する
            if (drawWave) {
                int wy = 63 - (waveTable[x] >> 2); // 波の高さを画面の高さ(64)に合わせる
                if (wy >> 3 == p) out |= (1 << (wy & 7)); // 今描いている帯(ページ)なら光らせる
            }
            
            // 画面の中央の点線(目安)を描画する
            if (p == 4 && (x % 4 == 0)) out |= 0x01;
            
            // プレイ画面のカーソルとアンカーを描画する
            if (mode == 1) {
                // 打った点(アンカー)を描画 (フリーズ回避の三項演算子処理)
                for (int i = 0; i < anchorCount; i++) {
                    int ax = anchors[i].x;
                    int ay = 63 - (anchors[i].y >> 2);
                    if (abs(x - ax) <= 1 && (ay >> 3 == p)) {
                        if (x == ax) {
                            int shift = (ay & 7) - 1;
                            out |= (shift < 0) ? 0x03 : (0x07 << shift); // 画面一番上でマイナスエラーにならないようにする
                        } else {
                            out |= (1 << (ay & 7));
                        }
                    }
                }
                // カーソル(十字)を描画
                int cy = 63 - (curY >> 2);
                if (abs(x - curX) <= 3) {
                    if (x == curX) {
                        int top = cy - 3, bot = cy + 3;
                        for(int bit=0; bit<8; bit++) {
                            int current_y = (p << 3) + bit;
                            if(current_y >= top && current_y <= bot) out |= (1 << bit);
                        }
                    } else if (cy >> 3 == p) out |= (1 << (cy & 7));
                }
            }
            i2c_write(out); // 出来上がった1列分のデータを画面に送る
        }
        i2c_stop(); // 1ページ分の送信終わり
    }
}

// --- タイマー割り込みで音を出す処理 (超高速で呼ばれる) ---
void audio_isr() {
    phaseCounter += phaseIncrement; // 波を進める(ピッチ調整)
    uint8_t index = (phaseCounter >> 8) & 0x7F; // 128段階のどこを読めばいいか計算
    uint32_t sample = waveTable[index]; // 波形データから波の高さを取り出す
    TIM1->CH4CVR = (sample * gain) >> 8; // ボリュームを掛けてPWM(ピンの出力)に渡す
}

// --- 音を出すためのハードウェア初期設定 ---
void audio_setup() {
    RCC->APB2PCENR |= RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOC;
    GPIOC->CFGLR &= ~(0xF << 16); GPIOC->CFGLR |= (0xB << 16);
    TIM1->CTLR1 = 0; TIM1->PSC = 0; TIM1->ATRLR = 255;
    TIM1->CHCTLR2 = (6 << 12); TIM1->CCER = (1 << 12); TIM1->BDTR = (1 << 15);
    TIM1->CTLR1 = 1;
    sampleTimer = new HardwareTimer(TIM2); // タイマー2を使って一定間隔で処理を呼ぶ
    sampleTimer->setOverflow(10000, HERTZ_FORMAT); // 1秒間に10000回(10kHz)のペース
    sampleTimer->attachInterrupt(audio_isr); // 呼ぶ関数を指定
    sampleTimer->resume(); // タイマースタート
}

// --- 起動時に1回だけ実行される設定 ---
void setup() {
    pinMode(PIN_LED, OUTPUT); // LEDピンを出力に設定
    digitalWrite(PIN_LED, LOW); // 最初はLEDを消しておく
    i2c_setup(); // I2Cの準備
    oled_init(); // 画面の準備
    audio_setup(); // 音の準備
    mode = 0; // 最初はメニュー画面からスタート
    currentPreset = 0; // プリセット1番(SINE)
    load_preset(currentPreset); // プリセットを読み込む
    refresh_display(); // 画面に描画する
}

// --- ずっと繰り返されるメインループ ---
void loop() {
    // ボタンの状態を読み取る (LOWなら押されている)
    bool btnX = (digitalRead(PIN_SW_X) == LOW);
    bool btnY = (digitalRead(PIN_SW_Y) == LOW);
    bool btnNow = (digitalRead(PIN_SW_ENTER) == LOW);

    // Xツマミでボリューム調整
    if (mode == 0 || !btnX) gain = analogRead(PIN_VOL_X) >> 2;
    // Yツマミでピッチ(音の高さ)調整
    if (mode == 0 || !btnY) phaseIncrement = (analogRead(PIN_VOL_Y) * 4) + 40;

    // 【メニュー画面の時の処理】
    if (mode == 0) {
        bool changed = false;
        // ボタンが押されたらプリセット番号を切り替える
        if (btnX && !btnX_prev) { currentPreset = (currentPreset + 1) % 8; changed = true; }
        if (btnY && !btnY_prev) { currentPreset = (currentPreset + 7) % 8; changed = true; }
        if (changed) {
            load_preset(currentPreset);
            refresh_display();
        }

        // ENTERボタンを押して離したらプレイ画面へ
        if (btnNow && !btnActive) { 
            btnActive = true; 
        } else if (!btnNow && btnActive) {
            mode = 1; 
            btnActive = false;
            curX = 64; curY = 127; // カーソルを初期位置へ
            refresh_display();
        }
    } 
    // 【プレイ画面の時の処理】
    else {
        bool move = false;
        // Xボタンを押しながらXツマミを回すとカーソルが左右に動く
        if (btnX) {
            int nx = analogRead(PIN_VOL_X) >> 3;
            if (nx != curX) { curX = (nx > 127) ? 127 : nx; move = true; }
        }
        // Yツマミを回すとカーソルが上下に動く (右回しで上昇)
        if (btnY) {
            int ny = analogRead(PIN_VOL_Y) >> 2;
            // ノイズで画面がチカチカしないように、値が2以上変わった時だけ動かす(ヒステリシス)
            if (abs(ny - curY) > 1) { curY = ny; move = true; }
        }
        if (move) refresh_display(); // 動いたら画面を更新

        // ENTERボタンを押した長さを測る処理
        if (btnNow && !btnActive) { 
            btnPressStart = millis(); // 押し始めの時間を記録
            btnActive = true; 
            lastLedStep = 0;
        }
        // 押しっぱなしの間の処理 (長さに応じてLEDを光らせる)
        else if (btnNow && btnActive) {
            uint32_t duration = millis() - btnPressStart;
            if (duration >= 3000 && lastLedStep < 3) {
                digitalWrite(PIN_LED, HIGH); ledTurnOnTime = millis(); ledOn = true; lastLedStep = 3;
            } else if (duration >= 2000 && lastLedStep < 2) {
                digitalWrite(PIN_LED, HIGH); ledTurnOnTime = millis(); ledOn = true; lastLedStep = 2;
            } else if (duration >= 1000 && lastLedStep < 1) {
                digitalWrite(PIN_LED, HIGH); ledTurnOnTime = millis(); ledOn = true; lastLedStep = 1;
            }
        }
        // ボタンを離した瞬間の処理 (押していた長さで行動を変える)
        else if (!btnNow && btnActive) {
            uint32_t duration = millis() - btnPressStart;
            if (duration >= 3000) {
                // 3秒以上: メニューへ戻る
                mode = 0;
                load_preset(currentPreset);
            } else if (duration >= 1000) {
                // 1秒以上: 線を引いて波を作る (2秒以上なら滑らかな曲線にする)
                bool useSpline = (duration >= 2000);
                generate_wave(useSpline);
            } else if (duration > 50) { 
                // チョン押し: カーソルの場所に点(アンカー)を打つ (最大20個まで)
                if (anchorCount < 20) anchors[anchorCount++] = {curX, curY}; 
            }
            btnActive = false;
            refresh_display();
        }
    }

    // 次のループのためにボタンの状態を覚えておく
    btnX_prev = btnX;
    btnY_prev = btnY;

    // LEDが光ってから200ミリ秒(0.2秒)経ったら消す
    if (ledOn && (millis() - ledTurnOnTime >= 200)) {
        digitalWrite(PIN_LED, LOW);
        ledOn = false;
    }

    delay(10); // 少し休む(システムを安定させるため)
}
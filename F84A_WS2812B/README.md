# PIC16F84A "Retro-Future" LED Controller

![Badge](https://img.shields.io/badge/CPU-PIC16F84A-blue) ![Badge](https://img.shields.io/badge/Language-Assembly-red) ![Badge](https://img.shields.io/badge/LED-WS2812B-green)

**25年前の伝説的マイコン「PIC16F84A」で、最新の「WS2812B (NeoPixel)」を完全駆動する狂気のプロジェクト。**

> "They said it was impossible. We said it was fun."
> (無理だと言われた。だから面白いと言い返した。)

## 🚀 概要 (Overview)

現代の「Lチカ」は贅沢になりすぎました。LEDを光らせるために32bitマイコンやWi-Fiモジュールを使う時代へのアンチテーゼとして、1990年代の8bitマイコン **PIC16F84A** を召喚しました。

* **CPU**: PIC16F84A-20 (RAM: 68 bytes, ROM: 1K words)
* **Clock**: 20 MHz (1 Cycle = 200 ns)
* **Language**: Pure Assembly (MPASM)
* **Target**: WS2812B (NeoPixel) x 10 LEDs

C言語もライブラリも使いません。**命令サイクルの手計算 (Cycle Counting)** と **ビット演算の魔術** だけで、ナノ秒精度の信号を生成し、残像（トレイル）エフェクト付きのナイトライダーを実現しました。

## ✨ 機能 (Features)

シングルタクトスイッチ (RA0) を押すたびにモードが切り替わります。

1.  **OFF Mode**: 待機状態 (省電力...ではないが消灯)
2.  **Rainbow Mode**: ゲーミングPCのように七色に光りながら回転
3.  **Knight Rider Mode**: 赤い光が左右に往復スキャン。**ビットシフトによる残像（減衰）処理** を実装。

## 🧠 技術的詳細 (Technical Deep Dive)

なぜ25年前の石で最新LEDが動くのか？その秘密は「命令サイクルの支配」にあります。

### 1. サイクル厳密計算による波形生成
WS2812Bは通信にシビアなタイミングを要求します。
* **'0' Code**: High 0.4µs -> Low 0.85µs
* **'1' Code**: High 0.8µs -> Low 0.45µs

PIC16F84Aを **20MHz** で駆動すると、1命令サイクル (1 Tcy) は正確に **200ns** になります。我々はこの物理法則を利用しました。

* **'0' 送出ロジック**:
    * `BSF` (High) : 1命令 (200ns)
    * `BTFSS` (判定) : 1命令 (200ns)
    * **合計: 400ns** (WS2812Bの要求と完全一致)
* **'1' 送出ロジック**:
    * `BSF` (High) : 1命令 (200ns)
    * `BTFSS` (判定/Skip) : 2命令 (400ns)
    * `NOP` (調整) : 1命令 (200ns)
    * **合計: 800ns** (WS2812Bの要求と完全一致)

コンパイラ任せでは不可能な、**ジッター(揺らぎ)ゼロ** の波形生成を実現しています。

### 2. 割り算なしの「残像」生成
PIC16F84Aには乗算・除算命令がありません。しかし、ナイトライダーの美しい「尾を引く光」には輝度の減衰計算が必要です。
我々は全バッファを読み出し、**「右ビットシフト命令 (RRF)」** を適用することでこれを解決しました。

* `RRF` 1回 = 1/2 (50%)
* `RRF` 2回 = 1/4 (25%)

これにより、重い計算処理を一切行わず、滑らかなフェードアウト効果を実現しています。

## 🛠 ハードウェア構成 (Hardware)

### 配線図 (Wiring Diagram)

```text
       PIC16F84A (20MHz HS)
      +--------------------+
      |                    |
      | 17 (RA0)           | <--- Button Switch (Connect to GND)
      |                    |      *Pull-up with 10kohm to 5V
      |                    |
      | 6  (RB0)           | ---> WS2812B (DIN)
      |                    |      *Series resistor 330ohm recommended
      |                    |
      | 16 (OSC1)          | <--- 20MHz Resonator (Ceralock)
      | 15 (OSC2)          |      or Crystal + 22pF x2
      |                    |
      | 14 (VDD)           | <--- +5V (Stable Power Source)
      | 5  (VSS)           | <--- GND
      +--------------------+
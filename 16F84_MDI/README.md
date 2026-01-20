
# 🏎️ PIC16F84A MDI Ignition Simulator [Ultimate Edition]

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-PIC16F84A-red.svg)
![Language](https://img.shields.io/badge/language-Assembly-orange.svg)

**Legacy meets Technology.** 1990年代の名機「PIC16F84A」の限界に挑んだ、高機能MDI（マルチ・ディスチャージ・イグニッション）シミュレーターです。

## 📖 概要 (Overview)

このプロジェクトは、安価で古典的なマイコン `PIC16F84A` (20MHz) を使用して、最新のバイクや車のような**デジタル・イグニッション・システム**をシミュレートします。

本来、シングルタスクで低機能なこの石で「OLEDグラフィック描画」と「精密な点火タイミング制御」を両立させることは困難です。しかし、本システムでは**「タスク・スライシング（擬似リアルタイムOS）」**技術を実装することで、描画による処理落ち（息継ぎ）を完全に排除し、スムーズなエンジンの鼓動を実現しました。

## ✨ 特徴 (Features)

* **MDI (多重放電点火) 制御**:
    * **低回転域**: 3回連続点火（強力な火花で燃焼促進）
    * **中回転域**: 2回連続点火
    * **高回転域**: 1回強力点火
    * **0rpm**: 自動出力カット（コイル保護）
* **OLED グラフィカルダッシュボード**:
    * I2C接続のSSD1306 OLEDを使用。
    * **倍角フォント**: 視認性の高い10x16ドットの巨大数字表示。
    * **アナログバーグラフ**: 回転数にリニア連動するゲージ表示。
* **リアルな挙動**:
    * 回転数に応じた「可変ウェイト」により、エンジンの点火リズム（ドッドッドッ...）を再現。
    * ボタン入力はノンブロッキング処理で、点火を止めずにスムーズな操作が可能。
* **完全なI/O論理**:
    * 入力は全て負論理(Active Low)、出力は全て正論理(Active High)に統一。

## 🛠️ ハードウェア構成 (Hardware)

### 必要パーツ
* **MCU**: Microchip PIC16F84A
* **Clock**: 20MHz (セラロック または 水晶発振子)
* **Display**: 0.96 inch OLED (SSD1306 / I2C接続)
* **Power**: +5V DC
* **Components**: タクトスイッチ x4, LED x4, 抵抗器 (プルアップ用/LED用)

### ピンアサイン (Pinout)

```text
                  PIC16F84A (18-Pin DIP)
                +--------\/--------+
 [SW] RA2(UP) -| 1 (In)    (Out) 18 |- RA1 [OLED SCL]
 [SW] RA3(DW) -| 2 (In)    (Out) 17 |- RA0 [OLED SDA]
 [SW] RA4(NC) -| 3 (In)          16 |- OSC1 (20MHz)
        MCLR -| 4 (Rst)         15 |- OSC2 (20MHz)
         Vss -| 5 (GND)    (PWR) 14 |- Vdd (+5V)
[IG] RB0(OUT)-| 6 (Out)    (Out) 13 |- RB7 (NC)
[SW] RB1(ON) -| 7 (In)     (Out) 12 |- RB6 [LED_HI]
[SW] RB2(OFF)-| 8 (In)     (Out) 11 |- RB5 [LED_MID]
[LED] RB3(PW)-| 9 (Out)    (Out) 10 |- RB4 [LED_LO]
                +------------------+




🎮 操作マニュアル (How to Use)
1. スタンバイモード (電源投入時)
回路に電源(+5V)を繋いでも、システムはまだ眠っています。画面もLEDも消灯しています。

操作: POWER_ON (RB1) スイッチを押します。

2. アイドリング待機
POWER LED が点灯し、システムが起動します。

回転数は 0rpm です。安全のため点火信号は出ていません。

LED_LO が点灯します。

3. エンジン始動
操作: BTN_UP (RA2) を押します。

回転数が上昇し、MDI点火が開始されます。

OLEDに回転数とバーグラフが表示されます。

4. 回転数コントロール
BTN_UP / BTN_DW で回転数を制御します。

回転数に応じて点火パターンが自動的に切り替わります（3発→2発→1発）。

LEDインジケーター (LO/MID/HI) がシフトします。

5. システム停止
操作: POWER_OFF (RB2) スイッチを押します。

システムがシャットダウンし、全ての表示が消え、スタンバイモードに戻ります。

💻 技術的詳細 (Technical Details)
1. タスク・スライシング (Task Slicing)
I2C通信（画面更新）は非常に低速なため、一度に全画面を描画すると数十ミリ秒のラグが発生し、点火タイミングが狂います。 本システムでは、画面更新処理を15個の小さなタスクに分割。メインループ1周につき1タスクのみを実行することで、点火処理（MDIロジック）を常に最優先で回し続けています。

2. 高速 I2C Bit-Banging
PIC16F84AにはI2Cハードウェアがありません。全ての通信波形をソフトウェア（アセンブラ命令）で生成しています。20MHz駆動に最適化したウェイト調整を行い、安定した高速通信を実現しています。

3. フォント演算処理
メモリ容量(1KB)の制約上、大きなフォントデータを持つことができません。 5x8ドットの極小フォントデータを持ち、リアルタイムのビット演算（シフト＆論理和）によって10x16ドットへ拡大表示しています。

👨‍💻 開発者 (Authors)
yas (Lead Developer, Hardware Design, Concept)

Gemini (Co-Developer, AI Assistant, Low-End PIC Prof.)
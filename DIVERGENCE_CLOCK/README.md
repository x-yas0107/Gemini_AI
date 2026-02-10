# TRUE DIVERGENCE CLOCK (Ver 1.00)

> **"Time is not just a number. It's a divergence."**
> 時間は単なる数字ではない。それは世界線の変動である。

![Badge](https://img.shields.io/badge/Version-1.00-blue.svg) ![Badge](https://img.shields.io/badge/Device-PIC16F84A-green.svg) ![Badge](https://img.shields.io/badge/Authors-yas%20%26%20Gemini-orange.svg)

## 📖 概要 / Overview

**TRUE DIVERGENCE CLOCK** は、たった1桁の7セグメントLEDと、古き良きマイコン `PIC16F84A` を用いて、SF作品に登場する「ダイバージェンス・メーター」の雰囲気を再現したガジェットです。

通常、デジタル時計は「常に時間を表示」しますが、この時計は違います。
**ボタンを押したその瞬間、世界線を探索するような「シャッフル演出」を経て、現在時刻を1桁ずつ、色を変えながらドラマチックに投影します。**

単なる時計ではありません。これは、あなたの手のひらで時を刻む「観測装置」です。

---

## 💡 こだわりと特徴 / Key Features & Obsessions

このプロジェクトは、シンプルさの中に狂気的なまでの「こだわり」を詰め込んでいます。

### 1. 直感的な「色」による時刻表現
1桁しか表示できない制約を逆手に取り、**「色」で桁の意味を定義**しました。これにより、今表示されている数字が「時」なのか「分」なのかを直感的に認識できます。

* 🔴 **RED (時 - Hour):** 警告色のような赤。世界線の始まり。
* 🟠 **ORANGE (分 - Minute):** 赤と緑のLEDを高速点滅(PWM)させて生成した美しい琥珀色。
* 🟢 **GREEN (秒 - Second):** 世界線の確定を示す緑。

### 2. 世界線を探索する「シャッフル・シークエンス」
ボタンを押しても、すぐには時間は表示されません。
ニキシー管が数値を確定させる時のように、**数字がパラパラと高速で回転（シャッフル）**します。
「時間は静止しているものではなく、常に揺れ動いている」というコンセプトを視覚化しました。

### 3. 表示ブレを許さない「ラッチ(Latch)機構」
最大の技術的挑戦は、**「表示している最中(0.5秒間)に時計が進んでしまったらどうするか？」**という問題でした。
通常の時計プログラムでは、表示中に秒が繰り上がると、画面上の数字が途中で変わってしまう「チラつき」が発生します。

本システムでは、以下のロジックでこれを完全に解決しました。
1.  シャッフル演出が終わった瞬間に、内部時計から**その瞬間の時刻を「撮影（コピー）」**する。
2.  表示シークエンス中は、内部時計が進み続けても、表示には**「撮影した静止画データ」**を使用する。
3.  これにより、**「0.5秒の表示中に数字が裏返る」という物理的な矛盾をゼロにしました。**

### 4. ボタン連打に負けない「高精度フリーランニング・エンジン」
時計の精度を司る水晶発振（32.768kHz）とカウンタは、表示プログラムとは完全に独立して動き続けています。
ユーザーがボタンを100回連打しようが、演出を長時間見続けようが、**時計の進み（歩度）には1ミリ秒の影響も与えません。**

---

## 🛠 技術仕様 / Technical Specs

* **MCU:** Microchip PIC16F84A
* **Clock Source:** 32.768kHz Crystal Oscillator (Low Power Mode)
* **Display:** 7-Segment LED (Common Anode/Cathode depending on circuit)
* **Colors:** Red / Green (Dual LED per segment) -> Orange created via PWM
* **Language:** Assembly (MPASM)

### ピン配置 / Pinout
```text
                 _____________
 (RA2) BUTTON --|1          18|-- (RA1) GREEN LED Control
 (RA3) ------ --|2          17|-- (RA0) RED LED Control
 (RA4) ------ --|3          16|-- (OSC1) 32.768kHz
 (MCLR)RESET  --|4          15|-- (OSC2) 32.768kHz
 (Vss) GND    --|5          14|-- (Vdd) +3V ~ +5V
 (RB0) SEG A  --|6          13|-- (RB7) SEG DP
 (RB1) SEG B  --|7          12|-- (RB6) SEG G
 (RB2) SEG C  --|8          11|-- (RB5) SEG F
 (RB3) SEG D  --|9          10|-- (RB4) SEG E
                 -------------
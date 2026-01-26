# SKY-SYNC006 - Vintage Aircraft Compass Driver

![Project Status](https://img.shields.io/badge/Status-Stable-green)
![MCU](https://img.shields.io/badge/MCU-CH32V006F8P6-blue)
![License](https://img.shields.io/badge/License-MIT-yellow)

## 📖 Overview (概要)
**SKY-SYNC006** は、ビンテージ航空機用コンパス（Synchro Motor / Selsyn）を駆動するために設計された、CH32V006マイクロコントローラ用の高精度ファームウェアです。

通常、シンクロモーターの駆動にはアナログ発振回路や専用ICが必要ですが、本プロジェクトでは安価なRISC-Vマイコン1つで **400Hz 3相交流波形** を生成し、航空計器をスムーズかつ正確に制御することを実現しました。

**Target Device:** Vintage Aircraft Compass (Model: 6263-D-18200-2, etc.)

## ✨ Key Features (特徴)
* **Precision 400Hz 3-Phase Sine Wave:**
    * 24MHzのシステムクロックから、正確な400Hzの正弦波（0°, 120°, 240°位相差）を生成。
    * PWM DAC方式（キャリア周波数 48kHz/96kHz）を採用。
* **Adaptive Filtering Logic (V1.07):**
    * **高速応答:** ボリューム操作時はフィルタを弱め、指の動きに即座に追従。
    * **超安定ホールド:** 操作停止時は強力な指数移動平均(EMA)がかかり、ADCノイズやジッタによる針の微細な震えを完全に排除。
* **Real-time Dashboard:**
    * 128x32 OLEDディスプレイに現在の角度（DEG）とバーグラフをリアルタイム表示。
    * CPU負荷モニタリング用ステータスLED搭載。
* **Oscilloscope Sync Output:**
    * 波形観測用に、正弦波の位相0°に同期したトリガー信号を出力。

## 🛠 Hardware Specifications (ハードウェア仕様)

| Component | Specification | Note |
| :--- | :--- | :--- |
| **MCU** | WCH CH32V006F8P6 | TSSOP20 Package |
| **Display** | SSD1306 OLED | 128x32 pixels, I2C Interface |
| **Input** | Potentiometer (VR) | 10kΩ recommended |
| **Output Filter** | RC Low-Pass Filter | R=1kΩ, C=0.1μF (Cut-off approx 1.6kHz) |

### 🔌 Pin Assignment (I/O マップ)

| Pin No. | Pin Name | Function | Description |
| :---: | :---: | :--- | :--- |
| **1** | PD4 | **SYNC OUT** | オシロスコープ用トリガー信号 (400Hz Square Wave) |
| **5** | PA1 | **PWM S1** | フェーズ1 出力 (0 deg) |
| **6** | PA2 | **ADC IN** | 角度制御ボリューム入力 (0-3.3V) |
| **10** | PC0 | **Status LED** | ハートビート / CPU負荷モニター |
| **11** | PC1 | **OLED SDA** | I2C Data (Display) |
| **12** | PC2 | **OLED SCL** | I2C Clock (Display) |
| **13** | PC3 | **PWM S2** | フェーズ2 出力 (+120 deg) |
| **14** | PC4 | **PWM S3** | フェーズ3 出力 (+240 deg) |
| **19** | PD2 | **PWM REF** | リファレンス相出力 (Constant Amplitude) |

> **Note:** PWM出力ピン(S1, S2, S3, REF)には、必ずCRローパスフィルター(1kΩ + 0.1μF)を通して平滑化した信号を接続してください。直接接続すると高周波PWMにより機器を破損する恐れがあります。

## ⚙️ Software Architecture
本ファームウェアは、CH32V006の限られたリソースを最大限に活用しています。

1.  **Waveform Generation (TIM2 Interrupt):**
    * 25.6kHzの割り込み頻度で正弦波テーブル(64ステップ)を参照。
    * 各相(S1, S2, S3)の振幅をリアルタイム計算し、TIM1のPWMデューティ比を更新。
2.  **Adaptive ADC Processing:**
    * **Oversampling:** 1回につき16サンプリングを行い平均化。
    * **Dynamic EMA:** 入力変化量（Δ）に応じて、フィルタ係数（α）を `0.05` (静止時) ～ `0.6` (操作時) の間で動的に可変。
3.  **Hysteresis Control:**
    * 表示およびPWM更新に不感帯（±1度）を設け、境界値でのチャタリングを防止。

## 👥 Authors / Credits

This project was collaboratively developed by:

* **yas** (Hardware Design, Concept, Testing)
* **Gemini** (Firmware Architecture, Algorithm Design, AI Co-Pilot)

## 📜 License
This project is released under the MIT License.
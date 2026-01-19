# CH32V006 High-Speed Voltmeter "Fireworks Edition"
# CH32V006 高速応答・高精度デジタル電圧計「花火エディション」

![License](https://img.shields.io/badge/license-MIT-blue.svg) ![Language](https://img.shields.io/badge/Language-C-orange) ![Platform](https://img.shields.io/badge/Platform-CH32V006-green)

**Developed by yas & Gemini**

**[English]** A high-precision, ultra-fast response digital voltmeter project based on the WCH CH32V006 microcontroller.  
It features 4096x oversampling for stability without compromising the "live feel" of voltage fluctuations, and a custom vector-based 7-segment font rendering engine.

**[日本語]** WCH社の激安マイコン CH32V006 を使用した、高精度かつ超高速応答のデジタル電圧計プロジェクトです。  
4096回のオーバーサンプリングによる安定性と、電圧変動の「ライブ感」を両立しました。表示フォントはビットマップを使わず、ベクター描画による独自の7セグメントフォントエンジンを搭載しています。

---

## ✨ Features / 特徴

### 1. Ultra-High Oversampling / 驚異のオーバーサンプリング
- **4096 samples per reading:** Instead of using a slow moving average filter, this system bursts 4096 ADC readings instantly to cancel noise.
- **"Fireworks" Response:** Displays raw stability with high responsiveness. No laggy feeling typical of filtered meters.
- **[JP]** 移動平均フィルタを使わず、一瞬で4096回の測定を行い平均化することでノイズを除去。フィルタ特有の「遅れ」を排除し、電圧の揺らぎが花火のようにパラパラと見える「ライブ感」を実現しました。

### 2. Custom Graphics Engine / 独自グラフィックエンジン
- **Vector-based 7-Segment:** The numbers are drawn using coordinate transformation logic (skewed/italic style), not static images.
- **Hand-Tuned "V":** The unit "V" is hand-tuned for perfect aesthetic balance.
- **[JP]** 数字は画像データではなく、プログラムによる座標計算で「斜体（イタリック）」に描画されます。「V」の文字はバランスを考慮し、手動でドット単位の調整を行いました。

### 3. Calibration Ready / キャリブレーション機能
- Simple software trim via `#define CORRECTION_FACTOR`.
- **[JP]** プログラム内の係数を書き換えるだけで、市販のテスターに合わせた微調整（トリム）が可能です。

---

## 🛠 Hardware Setup / ハードウェア接続

### MCU: WCH CH32V006 (TSSOP20)
This project uses the CH32V006F8P6.  
本プロジェクトは CH32V006F8P6 を使用しています。

### Wiring List / 接続リスト

| Component | Pin Name | MCU Pin | Note |
| :--- | :--- | :--- | :--- |
| **Input (+)** | ADC Input | **PA2 (Pin 6)** | Connect via Divider |
| **OLED (I2C)** | SCL | **PC2 (Pin 12)** | SSD1306 |
| **OLED (I2C)** | SDA | **PC1 (Pin 11)** | SSD1306 |
| **Power** | VDD | 3.3V | |
| **Power** | VSS | GND | |

### Voltage Divider Circuit / 分圧回路
To measure up to ~200V:  
200V付近まで測定するための構成：

```text
High Voltage Input (+)
      │
      │
     [ 1 MΩ ]  (R1)
      │
      ├───> To MCU PA2 (Pin 6)
      │
     [ 15 kΩ ] (R2)
      │
      │
     GND (-)
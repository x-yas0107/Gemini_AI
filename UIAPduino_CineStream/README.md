# =================================================================
# Project: UIAPduino_CineStream
# Document: README.md
# Date:    2026-03-31
# Authors: yas & Gemini
# =================================================================

# UIAPduino CineStream
**〜 2KB RAMの極小マイコンで実現する、限界突破の完全同期ストリーミングプレイヤー 〜**
**~ The Limit-Pushing Perfectly Synchronized Streaming Player on a 2KB RAM Microcontroller ~**

---

## 🇯🇵 日本語版 (Japanese)

### 🎬 概要 (Overview)
数十円・RAMわずか2KBの極小マイコン「CH32V003」を使って、SPIフラッシュメモリから直接映像と音声を読み込み、OLEDディスプレイとスピーカーへ**「ノイズレスで完全同期ストリーミング再生」**する究極のメディアプレイヤーです。
高価なオーディオチップ（DAC）は一切不使用。マイコンのデジタルピン（PWM）とRCフィルターだけで、驚きの高音質を叩き出します！

### 🚀 究極の3大特長 (Features)
1. **限界突破のダブルバッファ・オーディオエンジン**
   たった2KBのRAM領域に、映像転送の隙間を縫って次の音声を先読みする「ダブルバッファ」を構築。マイコン特有の息継ぎノイズ（ブーン音）を完全に消滅させました。
2. **TIM2ストップウォッチによる1μsの絶対同期**
   タイマー割り込みを使わず、ハードウェアタイマー（TIM2）を1マイクロ秒精度のストップウォッチとして監視。映像の描画遅延に一切影響されない、完璧な音声ピッチ（15.38kHz）を維持します。
3. **全自動Pythonコンバーター同梱**
   どんなサイズ（16:9や4:3など）のMP4動画でも、自動で黒帯（レターボックス）を追加し、アスペクト比を維持。さらに音声の音割れ（クリッピング）を自動回避するノーマライズ処理を搭載。誰でも簡単に専用ROMデータ（最大約6分/16MB）を作成できます！

### 🔌 I/Oマップ (ハードウェア接続図)
| CH32V003 ピン | 役割 (機能) | 接続先デバイス | 備考 |
| :--- | :--- | :--- | :--- |
| **PC3** | SPI_CS | SPI Flash ROM (CS) | W25Q128等 (最大16MB) |
| **PC5** | SPI_SCK | SPI Flash ROM (CLK) | 高速SPIクロック |
| **PC6** | SPI_MOSI | SPI Flash ROM (DI) | マイコンからのコマンド送信 |
| **PC7** | SPI_MISO | SPI Flash ROM (DO) | 動画・音声データの爆速受信 |
| **PC1** | I2C_SDA (Bit-bang) | SSD1306 OLED (SDA) | 128x64 ディザリング映像出力 |
| **PC2** | I2C_SCL (Bit-bang) | SSD1306 OLED (SCL) | 高速ソフトウェアI2C |
| **PC4** | Audio PWM (TIM1_CH4)| オーディオアンプ入力 | **※必ずRCフィルター(1kΩ+0.1μF等)を経由** |
| **VDD / GND** | 電源 | 各デバイスの電源 | 3.3V駆動推奨 |

### 🛠️ 遊び方 (How to use)
1. 同梱のPythonスクリプトで、お好きなMP4動画を専用のバイナリファイル（.bin）に変換します。
2. 変換したバイナリデータを、ROMライター等でSPIフラッシュメモリに書き込みます。
3. 上記のI/Oマップ通りにハードウェアを配線し、電源を入れれば極上のシネマ体験が始まります！

---

## 🇬🇧 English Version

### 🎬 Overview
The ultimate media player that streams perfectly synchronized, noise-free video and audio directly from SPI Flash memory to an OLED display and speaker, using the ultra-cheap CH32V003 microcontroller with only 2KB of RAM.
Absolutely NO expensive audio DACs are used. It achieves surprisingly high sound quality using only the microcontroller's digital pin (PWM) and a simple RC filter!

### 🚀 Top 3 Extreme Features
1. **Limit-Pushing Double-Buffer Audio Engine**
   Constructed a "double buffer" within the mere 2KB RAM to pre-load the next audio chunk during the tiny gaps in video rendering. This completely eliminates the microcontroller's processing bottleneck noise (buzzing/stuttering).
2. **Absolute 1μs Sync via TIM2 Stopwatch**
   Instead of using timer interrupts, it monitors the hardware timer (TIM2) as a stopwatch with 1-microsecond precision. It maintains a flawless audio pitch (15.38kHz) completely unaffected by video drawing latency.
3. **Fully Automatic Python Converter Included**
   Automatically adds letterboxing to maintain the aspect ratio of any MP4 video (16:9, 4:3, etc.). It also features audio normalization to automatically prevent clipping distortion. Anyone can easily create dedicated ROM data (up to ~6 mins / 16MB)!

### 🔌 I/O Map (Hardware Wiring)
| CH32V003 Pin | Role (Function) | Connected Device | Notes |
| :--- | :--- | :--- | :--- |
| **PC3** | SPI_CS | SPI Flash ROM (CS) | e.g., W25Q128 (Up to 16MB) |
| **PC5** | SPI_SCK | SPI Flash ROM (CLK) | High-speed SPI Clock |
| **PC6** | SPI_MOSI | SPI Flash ROM (DI) | Command TX from MCU |
| **PC7** | SPI_MISO | SPI Flash ROM (DO) | Ultra-fast A/V data RX |
| **PC1** | I2C_SDA (Bit-bang) | SSD1306 OLED (SDA) | 128x64 Dithered Video Output |
| **PC2** | I2C_SCL (Bit-bang) | SSD1306 OLED (SCL) | High-speed Software I2C |
| **PC4** | Audio PWM (TIM1_CH4)| Audio Amplifier Input | ***MUST pass through an RC filter (e.g., 1kΩ+0.1μF)** |
| **VDD / GND** | Power | All Devices | 3.3V recommended |

### 🛠️ How to Use
1. Use the included Python script to convert your favorite MP4 video into a dedicated binary file (.bin).
2. Write the converted binary data to your SPI Flash memory using a ROM writer.
3. Wire the hardware according to the I/O map above, power it on, and enjoy your ultimate cinema experience!
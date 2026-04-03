# UIAPduino I2C Neuron - The 16KB Standalone Micro-OS
**Version 1.00** | **Release Date:** 2026-04-04 | **Developer:** yas / Gemini

---

## 🇯🇵 16KBの奇跡。PC不要で完結する極限のグラフィカルOS
**「UIAPduino I2C Neuron」**は、わずか16KBのフラッシュメモリしか持たないRISC-Vマイコン（CH32V003）上に構築された、完全スタンドアロンのマイクロ・オペレーティングシステムです。

PCはもう必要ありません。アナログスティックを握り、OLEDディスプレイ上のGUI（グラフィカル・ユーザー・インターフェース）を操作してください。直感的なマウス操作、ホバーエフェクト、ポップアップウィンドウによる快適な操作感。そして、外部EEPROMに直接シーケンスを書き込み、その場でハードウェアを制御する「手のひらサイズのビジュアルプログラミング環境」がここにあります。

### 🔥 Core Features (主要機能)
* **Analog Mouse GUI Engine:** アナログスティックによる直感的でヌルヌル動くマウス操作。クリック、ホバー反転、ポップアップUIを完全実装。
* **On-Board Sequence Editor (SED):** PCレスでプログラムを組めるScratchライクなシーケンスエディタ。4種類のコマンド（LED, SRV, DLY, END）をパレットから選び、ポップアップでパラメータを設定可能。
* **High-Speed I2C Network:** ソフトウェアI2Cを極限までチューニング。OLEDへの超高速描画とEEPROMキャッシュシステムにより、処理落ちのないレスポンスを実現。
* **Direct Hardware Control:** GUIからマイコンのピンを直接叩き、LEDやPWMサーボモーターをリアルタイム制御。

### 🗺️ Hardware I/O Map (ハードウェア構成)
* **MCU:** CH32V003 (RISC-V 48MHz, 16KB Flash, 2KB SRAM)
* **Display:** 0.96" SSD1306 OLED (I2C)
* **Storage:** 24LC512 EEPROM (I2C)

| Pin / Ch | Device & Function | Description (役割) |
| :--- | :--- | :--- |
| **PC6** | Soft I2C SDA | I2Cデータ通信線 (OLED & EEPROM) |
| **PC7** | Soft I2C SCL | I2Cクロック通信線 (OLED & EEPROM) |
| **PC4** | Mouse Click SW | アナログスティックの押し込みボタン (内部プルアップ) |
| **PC0** | LED Output | テスト用LED制御ピン (HIGH/LOW) |
| **PC2** | Servo PWM (TIM1_CH3) | サーボモーター制御用PWM信号出力 |
| **ADC_Ch1**| Analog Stick X-Axis | マウスのX軸（左右）移動量読み取り |
| **ADC_Ch0**| Analog Stick Y-Axis | マウスのY軸（上下）移動量読み取り |

### 💾 Memory Map (EEPROM領域)
* `0x0000` : システムチェック用フラグ領域 (0xAAで正常)
* `0x0100` : システムフォント領域 (ASCII文字データ 5x7px)
* `0x0500 - 0x0563` : SEDシーケンス保存領域 (50行分 / 1行あたりコマンド1Byte + 値1Byte)

---
<br>

## 🇺🇸 A Miracle in 16KB. The Ultimate Standalone Graphical OS.
**"UIAPduino I2C Neuron"** is a fully standalone micro-operating system built on the CH32V003 RISC-V microcontroller, utilizing a mere 16KB of flash memory.

You no longer need a PC. Grab the analog stick and interact with the GUI on the OLED display. Experience intuitive mouse controls, hover effects, and popup windows. This is a palm-sized visual programming environment where you can write sequences directly to an external EEPROM and control hardware on the fly.

### 🔥 Core Features
* **Analog Mouse GUI Engine:** Smooth and intuitive mouse operation via an analog stick. Fully implements clicking, hover highlights, and popup UI elements.
* **On-Board Sequence Editor (SED):** A Scratch-like visual sequence editor that requires no PC. Select from 4 commands (LED, SRV, DLY, END) from the palette and set parameters via interactive popups.
* **High-Speed I2C Network:** Extreme tuning of software I2C. Achieves lag-free response through ultra-fast OLED rendering and an EEPROM caching system.
* **Direct Hardware Control:** Control microcontroller pins directly from the GUI to manipulate LEDs and PWM servo motors in real-time.

### 🗺️ Hardware I/O Map
* **MCU:** CH32V003 (RISC-V 48MHz, 16KB Flash, 2KB SRAM)
* **Display:** 0.96" SSD1306 OLED (I2C)
* **Storage:** 24LC512 EEPROM (I2C)

| Pin / Ch | Device & Function | Description |
| :--- | :--- | :--- |
| **PC6** | Soft I2C SDA | I2C Data Line (OLED & EEPROM) |
| **PC7** | Soft I2C SCL | I2C Clock Line (OLED & EEPROM) |
| **PC4** | Mouse Click SW | Analog Stick Push Button (Internal Pull-up) |
| **PC0** | LED Output | Test LED Control Pin (HIGH/LOW) |
| **PC2** | Servo PWM (TIM1_CH3) | PWM Signal Output for Servo Motor |
| **ADC_Ch1**| Analog Stick X-Axis | Mouse X-Axis (Left/Right) Movement |
| **ADC_Ch0**| Analog Stick Y-Axis | Mouse Y-Axis (Up/Down) Movement |

### 💾 Memory Map (EEPROM Allocation)
* `0x0000` : System Check Flag (0xAA = OK)
* `0x0100` : System Font Data (ASCII 5x7px)
* `0x0500 - 0x0563` : SED Sequence Storage (50 steps / 1Byte Cmd + 1Byte Val per step)

---
*Developed with passion and endless debugging by yas & Gemini.*
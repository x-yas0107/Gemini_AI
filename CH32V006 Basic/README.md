# CH32V006 Tiny BASIC v2.0 (World Edition)

**Run Tiny BASIC on the cheapest RISC-V microcontroller (CH32V006F8P6)!**
激安RISC-Vマイコン「CH32V006」の限界を突破する、Tiny BASICインタプリタです。

![License](https://img.shields.io/badge/license-MIT-blue.svg) ![Release](https://img.shields.io/badge/release-v2.0-green.svg)

## 🌟 Overview
This project brings an interactive BASIC interpreter to the CH32V006F8P6 (TSSOP20).
Despite the limited resources (16KB Flash, 2KB RAM), it supports full GPIO control, 5-ch ADC reading, 4-ch PWM output, and even Mandelbrot set calculation.

CH32V006F8P6 (TSSOP20) に、整数演算のみのTiny BASICを完全移植しました。
48MHzで動作し、全ピンのGPIO制御、アナログ入力(ADC)、PWM制御（蛍の光）、マンデルブロー集合の描画まで可能です。

## ✨ Features
* **Interactive Interpreter**: Programming via UART (TeraTerm, Putty, etc.)
* **Full GPIO Control**: `OUT`, `IN` commands for almost all pins.
* **ADC Support**: Read voltage values (0-4095) from 8 pins.
* **PWM Support**: LED dimming / Motor control (0-255) on 4 pins.
* **High Performance**: Integer math running at 48MHz.

## 🗺️ Pinout & Functions (TSSOP20)
| Pin # | Pin Name | Digital (OUT/IN) | ADC In | PWM Out | Note |
|:---:|:---:|:---:|:---:|:---:|:---|
| **1** | PD4 | ✅ | - | ✅ | **PWM** |
| **2** | PD5 | - | - | - | **UART TX (Fixed)** |
| **3** | PD6 | - | - | - | **UART RX (Fixed)** |
| **4** | NRST | - | - | - | Reset |
| **5** | PA1 | ✅ | ✅ | - | **ADC** |
| **6** | PA2 | ✅ | ✅ | - | **ADC** |
| **7** | VSS | - | - | - | GND |
| **8** | PD0 | ✅ | - | - | GPIO |
| **9** | VDD | - | - | - | 3.3V |
| **10** | PC0 | ✅ | ✅ | ✅ | **Super Pin** |
| **11** | PC1 | ✅ | ✅ | ✅ | **Super Pin** |
| **12** | PC2 | ✅ | ✅ | - | ADC |
| **13** | PC3 | ✅ | - | - | GPIO |
| **14** | PC4 | ✅ | ✅ | - | ADC |
| **15** | PC5 | ✅ | - | - | GPIO |
| **16** | PC6 | ✅ | - | - | GPIO |
| **17** | PC7 | ✅ | - | - | GPIO |
| **18** | PD1 | - | - | - | SWIO (Debug) |
| **19** | PD2 | ✅ | ✅ | - | ADC |
| **20** | PD3 | ✅ | ✅ | ✅ | **Super Pin** |

## 🔌 Wiring (Connection)
* **UART**: Connect USB-Serial converter to **Pin 2 (TX)** and **Pin 3 (RX)**.
* **Baud Rate**: 115200 bps
* **Terminal Settings**: 
    * Transmit delay: **50 msec/line** (Required!)
    * Local echo: ON (Optional)

## 📜 Command List

### System
* `PRINT "Text", Var` : Print text or variables. (End with `;` for no newline)
* `LIST` : Show current program.
* `RUN` : Execute program.
* `NEW` : Clear memory.
* `CLS` : Clear screen.
* `WAIT ms` : Wait for milliseconds.
* `Ctrl+C` : Stop execution.

### I/O Control
* `OUT pin, value` : Set GPIO High(1) or Low(0).
    * Ex: `OUT 10, 1`
* `A = IN(pin)` : Read GPIO status (0 or 1).
    * Ex: `IF IN(10)=1 THEN ...`
* `A = ADC(pin)` : Read Analog value (0-4095).
    * Ex: `PRINT ADC(5)` (Reads Pin 5/PA1)
* `PWM pin, duty` : Output PWM signal (Duty: 0-255).
    * Ex: `PWM 1, 128` (50% brightness on Pin 1)

### Logic flow
* `GOTO line` : Jump to line number.
* `IF condition THEN statement` : Simple condition.
    * (Note: `FOR` loop is not implemented to save space. Use `IF` & `GOTO`.)

## 🚀 Sample Code

### 1. Breathing LED (PWM Test)
Connect LED to **Pin 1**.
```basic
10 I = 0
20 PWM 1, I
30 WAIT 10
40 I = I + 5
50 IF I <= 255 THEN GOTO 20
60 I = 255
70 PWM 1, I
80 WAIT 10
90 I = I - 5
100 IF I >= 0 THEN GOTO 70
110 GOTO 10
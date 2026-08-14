# MouseTamer-RP2040 (Ver 0.90)

## Overview
MouseTamer-RP2040 is a powerful, standalone dual-core USB host and protocol analyzer built on the Raspberry Pi Pico (RP2040) architecture. 
Utilizing the `PIO-USB` library, it acts as a USB host to read inputs from standard USB mice (3-point and 5-point) and outputs the corresponding movements and clicks via Hardware UART and I2C.

Beyond simple input translation, MouseTamer features a comprehensive suite of onboard debugging tools, including a hybrid hardware/software Serial Monitor with visual waveform reproduction, a real-time I2C Sniffer, and a fully functional "Whack-A-Mole" game to test input latency and precision. All configurations are manageable via a built-in OLED UI and saved directly to EEPROM.

## Key Features
*   **True Dual-Core Processing**: Core 1 is strictly dedicated to PIO-USB bit-banging and BTL audio generation, ensuring zero dropped inputs. Core 0 handles UI, display updates, and UART/I2C communication.
*   **Universal Mouse Support**: Recognizes both standard 3-button and advanced 5-button USB mice.
*   **Flexible Data Output**: Transmits X/Y delta and button states via UART (configurable baud rates from 1200 to 921600, parity, stop bits) and I2C (Master/Slave modes up to 1MHz).
*   **Standalone OLED Interface**: No PC required. Complete menu system controlled via a rotary encoder and tactile switches.
*   **Onboard Logic Analyzers**:
    *   **Hybrid Serial Monitor**: Captures hardware RX and recreates signal waveforms (Start/Data/Stop/Gap) on a precise pixel timeline with automatic hex decoding.
    *   **I2C Sniffer**: Records SCL/SDA line states (up to 4000 samples) and decodes Start, Stop, ACK/NAK, and Hex data, complete with speed (kHz) and packet counting.
*   **Whack-A-Mole Precision Test**: A built-in arcade game using the mouse input, featuring sprite animations, BTL audio feedback, and EEPROM high-score tracking.

## Hardware I/O Map

| Pin | Function | Description |
| :--- | :--- | :--- |
| **GP0** | SNIFF SDA | Input pin for I2C Sniffer (SDA line) |
| **GP1** | SNIFF SCL | Input pin for I2C Sniffer (SCL line) |
| **GP2** | OLED SDA | I2C1 SDA for SSD1306 OLED Display |
| **GP3** | OLED SCL | I2C1 SCL for SSD1306 OLED Display |
| **GP4** | UART TX | Serial2 TX Output (Mouse Data) |
| **GP5** | UART RX | Serial2 RX Input (For Serial Monitor) |
| **GP8** | I2C SDA | I2C0 SDA (Mouse Data Output) |
| **GP9** | I2C SCL | I2C0 SCL (Mouse Data Output) |
| **GP10** | USB D+ | PIO-USB D+ line (Connect to Mouse) |
| **GP11** | USB D- | PIO-USB D- line (Connect to Mouse) |
| **GP12** | SPK P | BTL Piezo Speaker (+) |
| **GP13** | SPK N | BTL Piezo Speaker (-) |
| **GP15** | SET SW | Tactile Switch (Menu Select/Confirm) |
| **GP16** | LED | WS2812 NeoPixel (Status Indicator) |
| **GP26** | ENC A | Rotary Encoder Pin A |
| **GP27** | ENC B | Rotary Encoder Pin B |
| **GP28** | ENC SW | Rotary Encoder Push Switch (Menu Select) |
| **GP29** | ESC SW | Tactile Switch (Menu Back/Cancel) |

## Dependencies & Environment
*   **Board Package**: Earle F. Philhower's `arduino-pico` (Raspberry Pi Pico/RP2040)
*   **USB Stack**: Adafruit TinyUSB Library (configured for PIO-USB Host)
*   **Libraries**:
    *   `Adafruit_GFX` & `Adafruit_SSD1306` (OLED Display)
    *   `Adafruit_NeoPixel` (Status LED)
    *   `EEPROM` (Configuration saving)

## Menu Structure
1.  **Run**: Start/Stop data transmission, toggle Auto-Start, and enter Visual Mode (mouse cursor demo).
2.  **Mouse Setting**: Select 3-Point or 5-Point mouse data parsing.
3.  **Serial Setting**: Configure Baudrate, Parity, and Stop Bits.
4.  **I2C Setting**: Configure Mode (Master/Slave), Speed (100k/400k/1M), and Device Address.
5.  **Application**:
    *   **Serial Monitor**: Capture incoming UART data or run a demo loopback. View continuous waveforms and hex/ascii values.
    *   **I2C Sniffer**: Capture and decode external I2C traffic.
    *   **Whack-A-Mole**: A latency and precision testing game.
6.  **Save Settings**: Commit current configurations to EEPROM.
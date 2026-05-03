# 🚀 GemOS (CH32V006 Port)

![Version](https://img.shields.io/badge/Version-0.70-blue.svg)
![MCU](https://img.shields.io/badge/MCU-WCH_CH32V006F8P6-orange.svg)
![Architecture](https://img.shields.io/badge/Architecture-Custom_VM-success.svg)

A highly optimized, ultra-lightweight Virtual Machine-based Operating System designed specifically for the WCH CH32V006 microcontroller. 

GemOS pushes the limits of embedded systems by integrating a custom VM engine, EEPROM cartridge management, I2C OLED driving, and hardware PWM sound—all within an extremely constrained memory footprint.

## ✨ Core Features

* **Custom VM Engine:** A proprietary bytecode interpreter capable of running up to 31 isolated payloads (apps/games). Features hardware-accelerated sprite rendering, bounding-box collision detection, and tilemap processing.
* **EEPROM Cartridge System:** Acts as external storage for VM payloads. Dynamically loads 128-byte to 2KB apps into the VM memory space seamlessly.
* **Terminal Commander (PC Link):** A powerful serial dashboard for real-time debugging, EEPROM hex dumping, VM execution tracing, and payload formatting.
* **Hardware-Level Integration:** * Analog Joystick input with dynamic deadzone calibration.
    * Native I2C OLED (SSD1306) driver with page-loop rendering.
    * Hardware PWM Sound via `TIM2_CH2` for low-overhead audio.
* **Built-in UI:** Includes a native OS App Launcher, System Menu, and Hardware I/O testing suite.

## 🛠️ System Architecture

### Memory Map (VM Environment)
The VM operates within a strict memory constraint, offering a rich set of features for payload developers:
* `vm_vars[64]`: 8-bit general-purpose variables.
* `vm_sprites[32]`: Hardware-accelerated sprite objects.
* `vm_rects[64]`: Dedicated bounding boxes for generic physics and UI.
* `vm_map[256]`: Background tilemap array for board/grid-based games.

## 📜 Update Log (v0.70)
* **VM Engine Fix:** Restored correct OS memory integrity by preventing 9-byte payload header overwrites during app loading.
* **Tilemap Processing:** Added OpCode `0x1D` (READ_MAP) and `0x1C` (WRITE_MAP) for robust game logic.
* **Memory Expansion:** Upgraded variable capacity to support complex mechanics.

## 👨‍💻 Developers
**yas & Gemini**
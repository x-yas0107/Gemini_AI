# CHANGELOG

### 🎮 Applications & Games (Ver 0.80 - 0.90)
* **0.90:** Fixed missing variable declarations (`last_data_time`, `start_time`) in `record_serial_demo()`.
* **0.89:** Enhanced Whack-A-Mole game. Added swinging hammer animations with impact effects. Implemented high-score tracking via EEPROM and added a fanfare sequence upon setting a new record.
* **0.88:** Overhauled Whack-A-Mole logic. Introduced a 3x2 fan-shaped layout (6 holes) with independent state-machine animations (peeking, full, hit).
* **0.87:** Added BTL (Bridge-Tied Load) piezo speaker driving functionality via GP12 and GP13 using non-blocking execution on Core 1. Implemented initial game logic, scoring, and placeholder graphics for "Whack-A-Mole".
* **0.86:** Expanded I2C Sniffer packet display to show "Current Packet / Total Packets" navigation. Shifted the right-bottom stats block to accommodate double-digit values.
* **0.85:** Fixed I2C Sniffer decode text positioning. Implemented true centering for byte hex labels to resolve overlap with the Stop condition ("P") symbol.
* **0.84:** Adjusted I2C Sniffer right-edge scroll calculations to perfectly fit the final sample waveform and labels within the screen.
* **0.83:** Overhauled I2C Sniffer scrolling to a pixel-based continuous timeline system.
* **0.82:** Fixed text overlap between the bottom-left list and bottom-right stats in the I2C Sniffer.
* **0.81:** Improved I2C Sniffer UI (e.g., formatting to "0x2C ACK") and enhanced navigation displays.
* **0.80:** Disabled automatic text wrapping across sniffer screens to prevent layout breakage.

### 📈 Serial Monitor Advancements (Ver 0.65 - 0.79)
* **0.79:** Fixed character corruption and buffer overflows in the Serial Monitor Demo Mode by introducing basic flow control.
* **0.75 - 0.78:** Fine-tuned Serial Monitor waveform rendering. Added a timing grid (0.78), fixed Gap display (0.77), extended the waveform area to the screen edge (0.76), and implemented line-clipping logic (0.75).
* **0.73 - 0.74:** Redesigned the Serial Monitor layout. Shifted from chunk-based jumping to a 1-byte pixel-continuous scroll. Unified waveform and HEX/ASCII lists to track smoothly together, using the screen center as the focus point.
* **0.72:** Transitioned the Serial Monitor to a hybrid approach: Hardware UART handles precise data reception, while software reconstructs and draws the ideal protocol waveform.
* **0.67 - 0.71:** Rebuilt waveform mapping based on strict time-axis calculations inspired by PulseView. Changed from hardware RX to software pin-sniffing temporarily, before moving to the final hybrid model.
* **0.65 - 0.66:** Moved the Serial Monitor waiting screen to a menu format, added a "Demo Loopback" feature (0.66), and officially implemented it as an Application (0.65).

### 🔍 Sniffer & Visual Mode Introduction (Ver 0.56 - 0.64)
* **0.63 - 0.64:** Implemented the "I2C Sniffer" application. Added precise `time_us_32()` timestamps, automatic speed (kHz) calculation, and total packet counting.
* **0.61 - 0.62:** Redesigned Visual Mode graphics. Removed debug numbers and implemented a PC-like pointer trail (continuous circular interpolation) for smooth visual feedback.
* **0.60:** Solved data-drop issues between Dual Cores by shifting mouse data sharing to a lock-free accumulated delta method. Added interrupt masking during encoder reads.
* **0.56 - 0.59:** Introduced Visual Mode (Mouse cursor demo) directly to the main screen, later moving it to a dedicated full-screen interface within the RUN menu.

### ⚙️ UI Refinements & Core Settings (Ver 0.28 - 0.55)
* **0.53 - 0.55:** Implemented software debouncing for the encoder switch, smoothed rotational tracking, and adjusted encoder step thresholds.
* **0.48 - 0.52:** Refreshed main screen layout. Updated fonts to FreeSansBold, adjusted mouse icon coordinates, and mitigated idle-state freezing by rate-limiting `pixel.show()`.
* **0.42 - 0.47:** Added external I2C communication logic (0.46) and configuration menus for I2C Mode/Speed/Address (0.45). Implemented EEPROM bulk saving (0.44) and Serial Parity/Stop Bit/Baudrate (1200-921600) settings (0.42, 0.43).
* **0.39 - 0.41:** Introduced a unified UI base framework, added a Run toggle feature, and implemented a corner activity indicator (0.41).
* **0.28 - 0.38:** Corrected internal NeoPixel color orders (0.38), added menu-hierarchy-based LED colors (0.37), and built the graphical mouse monitor UI featuring 8-directional movement arrows on the main screen.

### 🛠️ System Foundations & Dual-Core Architecture (Ver 0.01 - 0.27)
* **0.27:** Major Architecture Update: Implemented Dual-Core separation. Core 1 isolated strictly for PIO-USB host tasks; Core 0 handles UI and UART entirely.
* **0.26:** Restored and integrated the menu UI drawing, state machine, and encoder interrupt processing from legacy version 0.09.
* **0.21 - 0.23:** Resolved system freezes by mapping the hardware serial instance to `Serial2` (0.23) and implemented the foundational UART TX string format (`M,x,y,btn`) (0.22).
* **0.15 - 0.16:** Added LED debug markers within the startup sequence to isolate OLED initialization freezes (0.16) and established the basic menu state machine (0.15).
* **0.01 - 0.04:** Initial builds establishing Dual Core FIFO tests, RGB LED control, Adafruit_SSD1306 OLED initialization, and hardware switch/encoder polling.
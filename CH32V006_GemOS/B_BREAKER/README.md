# 🧱 B_BREAKER (GemOS VM Payload)

![Version](https://img.shields.io/badge/Version-0.30-blue.svg)
![Target Slot](https://img.shields.io/badge/Slot-10-orange.svg)
![Platform](https://img.shields.io/badge/Platform-GemOS_VM-success.svg)

A high-speed, fully featured block-breaking game engineered entirely in custom bytecode for the GemOS Virtual Machine. 

## ✨ Features
* **Dynamic Physics:** Smooth ball reflection and paddle boundary collision detection implemented natively via VM OpCodes.
* **Hardware Audio:** Integrated 600Hz PWM beep sounds for block hits and paddle bounces.
* **Centered UI:** Pixel-perfect "GAME OVER" text rendering centered dynamically based on variable pointers.
* **EEPROM execution:** Runs entirely within the constraints of GemOS Slot 10.

## 📜 Update Log (V0.30)
* **CENTER OVER TEXT:** Changed Y-coordinate for "OVER" text for exact vertical centering on a cleared screen.
* **GAME OVER TEXT FIX:** Corrected DRAW CHAR variable pointers from literal coordinates to variable IDs.
* **INPUT-BASED GAME OVER:** Resolved VM crashes by strictly following the instruction set.

## 🚀 Payload Code
Copy and install this payload into GemOS Slot 01.


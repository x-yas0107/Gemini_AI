### 3. OTHELLO用 README.md

```markdown
# ⚪⚫ OTHELLO (GemOS VM Payload)

![Version](https://img.shields.io/badge/Version-0.34-blue.svg)
![Target Slot](https://img.shields.io/badge/Slot-00-orange.svg)
![Platform](https://img.shields.io/badge/Platform-GemOS_VM-success.svg)

A fully functional Reversi/Othello clone pushing the extreme limits of the GemOS VM bytecode engine. Built to test complex recursive map logic and array manipulation.

## ✨ Features
* **Full 8-Axis Flip Logic:** Accurately detects and flips stones horizontally, vertically, and across all 4 diagonals using a highly optimized recursive check loop.
* **Dynamic Score Counter:** Real-time tile scanning algorithm renders "SC:00" dynamically in the safe pixel margins of the OLED display without corrupting the 8x8 game board.
* **Audio Feedback:** Smartly injected `BEEP_SHORT` (OpCode 0x08) into free payload bytes to generate satisfying sounds when placing and flipping stones.

## 📜 Update Log (V0.34)
* **BEEP SOUNDS RESTORED:** Embedded OpCode 0x08 into the free bytes of existing blocks (INITGAME, PUT_BLK, PUT_WHT, DO_FLIP) without using any new blocks.
* **INITIAL STONES RESTORED:** Recovered the starting 4 stones.
* **SCORE RENDER FIX:** Corrected X-coordinates for DRAW NUMBER and safely integrated dynamic score counting into the main loop.

## 🚀 Payload Code
Copy and install this payload into GemOS Slot 02.
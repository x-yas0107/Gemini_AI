# WireFlow CAD

**WireFlow CAD** is a lightweight browser CAD for hand-drawn PCB traces and physical wiring diagrams.
No install: open the single HTML file in a modern browser.

## Features

* Single-file HTML (CSS/JS included). Works offline.
* Direct KiCad footprint import (`.kicad_mod`) with layer mapping.
* Four drawable layers: front copper (blue), back copper (red), silk (white), outline (yellow).
* Traces with square corners, 45° chamfers, fillets, or free-form splines.
* Offset-copy tool for bus-style parallels.
* Print for home PCB making: actual-size A4, mirror, X/Y scale correction, color / mono / negative.
* Board JSON save/load. Settings persist in localStorage.

## Getting started

Open `WireFlow_CAD.html` in Chrome or Firefox.

Most tools start from the **right-click menu**.

* **F1** — File / Library (board, KiCad parts, part library)
* **F2** — Print
* **F3** — Toggle the on-canvas help
* Coordinates sit on the right of the top bar (ABS always, REL while drawing)

## Shortcuts

| Input | Action |
| :--- | :--- |
| Right-click | Context menu / finish or cancel |
| Left-click | Select, place, start or bend a trace |
| Left-drag | Box select |
| Middle-drag | Pan |
| Double middle-click | Fit drawing |
| Ctrl + wheel | Zoom at cursor |
| F1 / F2 / F3 | File menu / print / help |
| Ctrl+C / Ctrl+V | Copy / paste (paste uses the **active** layer for traces and shapes) |
| Delete / Backspace | Delete selection |
| Alt (hold) | Disable grid snap |
| Shift (hold) | Add to selection / flip L-bend axis |
| Space | Rotate part or text / apply corner style while routing |
| V | Place a via and flip front/back copper |
| Double-click title block or text | Edit |

## KiCad import

**F1 → Import KiCad footprint**. Map F.Cu / B.Cu / silk / fab onto WireFlow layers while watching the preview.

## Print (F2)

* Color — as on screen
* Mono / solid black — toner transfer
* Negative — photosensitive boards
* Measure the printed 100 mm X/Y scales and enter them to correct printer error
* Use the system dialog **Actual size** and **no margins**

## License

MIT License. Copyright (c) 2026 yas0107.

Development used AI assistants (Gemini, Grok). There is no extra restriction on the generated code.

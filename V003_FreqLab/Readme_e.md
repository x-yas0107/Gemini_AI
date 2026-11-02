# CH32V003 High Precision Frequency Counter
### DIY Frequency Counter Project — Firmware V0.23

**A compact, low-cost frequency counter built around the WCH CH32V003 MCU.**

A DIY frequency counter designed to achieve stable frequency measurements using hardware timers, a carefully designed analog input stage, and a built-in calibration system.

Originally developed as an entry-level DIY measuring instrument, this project evolved into a surprisingly capable frequency counter through repeated hardware and firmware improvements.

---

## 1. Project Overview

| Item | Specification |
|---|---|
| Project | CH32V003 Frequency Counter |
| MCU | WCH CH32V003A4M6 |
| Package | TSSOP-20 |
| Core Clock | 48MHz |
| Clock Source | 24MHz External Crystal × PLL 2 |
| Firmware Version | V0.23 |
| Development Environment | Arduino IDE |
| Platform | arduino_core_ch32 |
| Display | 128 × 32 OLED |
| Measurement Method | Hardware Timer / Gate Counting |
| Calibration | Built-in Software Calibration |

### Measurement Performance

**Observed measurement range: approximately 5kHz to 30MHz.**

The following measurements were confirmed during hardware testing:

| Input Frequency | Counter Display |
|---:|---:|
| 15kHz | 15.017 |
| 20kHz | 20.000 |
| 100kHz | 100.000 |
| 1MHz | 1.00000 |
| 10MHz | 10.0000 |
| 30MHz | 30.00000 |

*These are observed test results, not guaranteed specifications. Absolute accuracy and long-term stability require verification against a calibrated frequency reference.*

---

## 2. Hardware Architecture

The counter uses the CH32V003 hardware timers to perform frequency measurement with minimal CPU overhead.

### Main MCU Functions

- TIM2: External pulse counting through the ETR input.
- TIM1: Periodic interrupt generation for the measurement gate.
- OLED: Frequency display.
- GPIO: User controls, speaker output and reset.
- Internal Flash: Calibration data storage.

### Measurement Principle

The external signal is applied to the TIM2 external counter input.

TIM1 generates an interrupt every 0.5ms.

The firmware reads the TIM2 counter and calculates the difference between consecutive readings. These differences are accumulated to obtain the total pulse count.

After 2,000 timer intervals:

**0.5ms × 2,000 = 1 second**

The accumulated pulse count therefore represents the measured frequency in Hz.

This approach uses hardware counting rather than relying on software pulse detection.

### Automatic Prescaler Control

To extend the usable input frequency range, the firmware automatically switches the external input prescaler.

- Below approximately 14MHz: Direct counting mode.
- Above approximately 16MHz: Divide-by-4 mode.
- Between these thresholds: Hysteresis prevents unnecessary switching.

The hysteresis helps prevent unstable mode switching near the threshold.

---

## 3. Analog Input Stage

The analog input stage is an important part of the overall design.

The prototype was assembled using readily available components rather than a dedicated PCB.

### Input Stage Components

- 2SK128 — FET input stage
- 2SC1815 — Signal amplification stage
- Ferrite bead — High-frequency noise suppression
- Coupling capacitors — Signal path coupling
- Separated amplifier power supply — Reduction of digital noise interference

The FET input stage is intended to provide high input impedance and improve the handling of relatively weak input signals.

The amplifier supply is separated from the digital circuitry to reduce unwanted noise coupling.

The combination of input impedance, amplification, coupling and power-supply isolation may contribute significantly to the observed stability.

The prototype also supports measurements using an H-field probe, allowing switching signals to be detected without direct electrical connection.

**Note:** The prototype was assembled on a universal PCB without a ground plane. No complete circuit schematic is currently available.

---

## 4. Calibration System

A built-in calibration system is provided to compensate for errors in the measurement time base.

The calibration system adjusts the TIM1 timing parameters.

### Calibration Features

- Adjustment in approximately 0.25Hz steps of the calibration tone.
- 1kHz nominal calibration tone.
- UP / DOWN button adjustment.
- Visual calibration indication.
- Calibration data saved to internal Flash.

Calibration data is stored in the final 64-byte Flash page:

`0x08003FC0`

This allows calibration settings to be retained after power is removed.

### Calibration Without an Oscilloscope

The project was designed with DIY users in mind.

Even without an oscilloscope, calibration can be performed using an audio-frequency beat method.

This provides an accessible calibration approach using audio-frequency signals.

For more precise calibration, an oscilloscope or a suitable frequency reference can be used.

---

## 5. Pin Assignment

| MCU Pin | Function |
|---|---|
| PD4 | TIM2 External Counter Input (ETR) |
| PC1 | OLED SDA |
| PC2 | OLED SCL |
| PC3 | UP Button |
| PC4 | Speaker Output |
| PC6 | DOWN Button |
| PC7 | FUNCTION Button |
| PD7 | Software Reset |

---

## 6. OLED Display

The display uses a 128 × 32 OLED module.

Features include:

- Large frequency digits.
- Automatic Hz / kHz / MHz formatting.
- Eight-digit display layout.
- Custom slanted seven-segment-style characters.
- Compact unit indication.

The display is designed to make frequency readings easy to recognize at a glance.

---

## 7. Development History

Firmware development has progressed through multiple revisions.

Version 0.23 includes accumulated improvements to measurement, display, calibration, clock configuration and input prescaler control.

The project has also undergone repeated hardware testing and firmware debugging.

One important development issue involved the delay implementation when operating with the external crystal and PLL clock configuration. The delay routine was subsequently changed to a CPU-based wait loop.

---

## 8. Experimental Results

The prototype has been tested using several measurement methods.

### Direct Frequency Input

Stable operation was observed over an approximate range of 5kHz to 30MHz.

### TinySA Comparison

At a 30MHz test frequency, the TinySA displayed:

`30.000 MHz`

The DIY frequency counter displayed:

`30.00000 MHz`

The matching readings were an encouraging result during prototype evaluation.

This comparison demonstrates agreement between the displayed readings, but does not independently establish absolute measurement accuracy.

### H-Field Probe Measurement

The counter was also tested with an H-field probe to detect switching signals from electronic circuitry.

This demonstrated that the input stage could handle signals captured through near-field magnetic coupling.

---

## 9. Future Improvements

Possible future development includes:

- Evaluation of higher-frequency input transistors.
- Improvement of the analog input stage.
- Dedicated PCB design.
- Detailed circuit documentation.
- Testing with an OCXO frequency reference.
- Further investigation of input sensitivity and maximum operating frequency.
- Long-term frequency stability evaluation.

The current universal-PCB prototype will be retained as a reference for future hardware revisions.

---

## 10. Project Philosophy

This project started as a simple DIY frequency counter intended for beginners.

The goal was to create an accessible measuring instrument that could be built using inexpensive components and calibrated without requiring expensive laboratory equipment.

Through repeated experimentation and development, the prototype achieved performance beyond its original expectations.

**Built with simple components. Improved through experimentation. Driven by curiosity.**

---

## Credits

**Hardware / Project Development:** yas  
**Firmware Development:** yas & Gemini  
**AI Technical Analysis / Documentation:** ChatGPT

Firmware: V0.23

*This project is an experimental DIY measuring instrument. Specifications and performance figures are based on prototype testing and may vary depending on construction, components and calibration.*
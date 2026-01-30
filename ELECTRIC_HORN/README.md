# 🎺 CH32V-Harmonic-Synth-Horn
**High-performance 3-tone electronic horn & synth platform based on RISC-V CH32V006.**

## 🌟 Overview
This project pushes the limits of the budget-friendly RISC-V MCU **CH32V006** to create a 3-tone polyphonic electronic horn system with independent 3-channel outputs. 

Unlike simple buzzers, this system focuses on "mathematically recreating air vibration," featuring a "Real Yankee Edit" micro-detune algorithm that breaks the mechanical coldness of digital sound.

## 🚀 Key Features
* **Triple-Harmonic Synthesis**: Luxury chord composition using three independent oscillators.
* **Real Yankee Edit (Micro-Detune)**: Intentionally shifts frequencies by 0.1Hz units and adds jitter to recreate the "beat" and "separation" of real air horns.
* **12-bit Precision Fade-out**: Uses the 12-bit ADC for smooth, professional-grade envelope release.
* **On-Demand ADC Architecture**: Samples the potentiometer only at the trigger moment to prevent bus interference with the audio timers.
* **Sound Library**: Includes various wavetable presets like Brass, Organ, and Strings.

## 🎹 Sound Presets
You can easily change the horn's "voice" by swapping the `sine_table`. We have included:
* **Trumpet/Brass**: Sharp, harmonic-rich brass sound.
* **Rich Organ**: Deep, church organ style for beautiful chords.
* **Strings**: Elegant and thick orchestral sound.
* **8-bit Chiptune**: Retro arcade style pulse-wave.
* **Cyber Synth**: Metallic, futuristic EV-like sound.

## 🛠 Hardware Configuration (TSSOP20)
* **Pin 01 (PD4)**: Mixed Chord Out (TIM2)
* **Pin 19/05/13**: Solo Channels (Low/Mid/High)
* **Pin 20 (PD3)**: ADC Fade-out Volume
* **Pin 02/03/11**: Control Switches (Waveform / Trigger / Style)

## 👨‍💻 Development Story
Developed jointly by **yas & Gemini**. 
We tackled technical challenges such as TIM1 output locking and bus contention to achieve a "warm analog soul" within a digital RISC-V chip.
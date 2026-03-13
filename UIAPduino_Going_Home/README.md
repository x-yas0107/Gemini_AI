# UIAPduino Note-Expression Engine (NEE) - "Going Home" Ver 2.00

WCH CH32V003 (RISC-V) マイコンを用いた、高精度かつ音楽的表現力に特化した演奏エンジンです。単なる矩形波の再生を超え、管楽器のようなアーティキュレーションをプログラム制御で実現しています。

## 🚀 Overview
本プロジェクトは、安価な8ピンマイコン CH32V003 を「楽器」へと昇華させる試みです。
ドヴォルザークの名曲『家路』を、100ステップの動的エンベロープと、トランペット奏法を模した「ソフトウェア・タンギング」ロジックによって演奏します。

## 🛠 Technical Features

### 1. ADSR Envelope Synthesis
各音符ごとに、立ち上がり（Attack）と減衰速度（Decay/Release）を定義。
- **Dynamic Duty Modulation**: 1音の持続時間を100分割し、PWMのDuty比をリアルタイムで変調。
- **Logarithmic Decay**: 高音域から低音域まで、自然な減衰曲線を描くアルゴリズムを搭載。

### 2. Software Tonguing Logic
同音連打（Repeated Notes）における旋律のボヤけを解消するための新機軸。
- **Articulation Gap**: 音の開始直後の 25ms に微細な無音（または極低音量）区間を挿入。
- **Nini Rosso Style**: トランペットのタンギング技法をコード化し、音の輪郭を鮮明に分離。

### 3. High Efficiency
- **Flash Usage**: 約 5.8KB (35%)
- **RAM Usage**: 約 0.5KB (25%)
この圧倒的なリソースの余裕が、将来のマルチチャネル（和音）拡張への可能性を担保しています。

## 🔌 Hardware Configuration

| Component | Pin | Function |
| :--- | :--- | :--- |
| **PWM Output** | **PC0** | TIM2_CH3 (Alternate Function) |
| **Trigger Switch** | **PC5** | Internal Pull-up (Active Low) |
| **Speaker/Buzzer** | - | Connected to PC0 via Resistor |

## 🎼 Sequence Logic (Ver 2.00)
1.3x系のデバッグを経て、旋律の完全修復を完了。
- 正しい旋律ルート (MI-SO-SO / MI-RE-DO) の再定義。
- 「懐かしい顔」の終止線における欠落音の補完。
- リリーステールの50ms最適化による、潔い余韻。

## 📜 License
MIT License / Collaborative work by Gemini & yas.
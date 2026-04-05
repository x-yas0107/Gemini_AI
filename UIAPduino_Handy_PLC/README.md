# UIAPduino Handy PLC Core 
**The Ultimate Standalone Palm-sized PLC**

![Version](https://img.shields.io/badge/Version-V0.26-blue.svg)
![ROM Usage](https://img.shields.io/badge/ROM_Usage-97%25_(15904B)-red.svg)
![Platform](https://img.shields.io/badge/Platform-CH32V003-orange.svg)

## 🚀 Overview (概要)
**「手のひらサイズで、すべてが完結する。」**

UIAPduino Handy PLC Coreは、わずか数十円・16KBの極小マイコン「CH32V003」の限界を極限まで引き出した、完全スタンドアロンの超小型プログラマブル・ロジック・コントローラ（PLC）です。

PCや外部エディタは一切不要。本体の極小ディスプレイとアナログジョイスティックのみで、ラダーロジックの直感的な入力、EEPROMへのセーブ／ロード、そしてリアルタイムなI/O制御とライブモニタリングを実現します。

**Developed by yas & Gemini**

---

## 🔥 Key Features (主な機能)

* **💻 スタンドアロン・ラダーエディタ**
    * PC接続不要。ジョイスティックと1画面完結のポップアップパレットにより、直感的なラダー編集が可能。
    * サポート命令: A接点 / B接点 / 出力コイル / SET / RST / タイマー(T) / カウンター(C) / テキストラベル(TXT)
    * グリッドサイズ: 最大16行 × 8列
* **👁️‍🗨️ ライブモニタリング (MON Mode)**
    * 実行中の接点状態（ON/OFF）をラダー図上でリアルタイムに反転表示。
    * 専用の `RUN` / `STP` ランプアイコンにより、一目で動作状態を把握可能。
* **💾 EEPROM ファイルシステム**
    * 外部I2C EEPROM (24Cシリーズ等) を使用し、最大10個のプログラムスロットを搭載。
    * 初回起動時に5つの実用的なサンプルプログラム（LATCH, TIMER, COUNT, BLINK, STEP）を自動フォーマット＆生成。
* **⚡ 独自開発の軽量PLC Virtual Machine**
    * 入力（X）、出力（Y）、内部リレー（M）、タイマー（T）、カウンター（C）をリアルタイム演算。
    * カウンターのオートリセット（K=0によるRST命令）対応。

---

## 🛠 Hardware Specifications (ハードウェア要件)

| Component | Description |
| :--- | :--- |
| **MCU** | CH32V003 (Flash: 16KB / RAM: 2KB) |
| **Display** | 0.96 inch OLED (I2C: `0x78` or `0x3C`) |
| **Storage** | I2C EEPROM (Address: `0x50` / 例: AT24Cxxx) |
| **Input** | アナログジョイスティック (X/Y軸ADC) + タクトスイッチ群 |
| **I/O** | 物理入力(X): 4ch / 物理出力(Y): 4ch |

*※I2C通信は任意のピンを割り当て可能な「ソフトウェアI2C」にて実装。*

---

## 🎮 How to Use (操作モード)

本システムは、トップ画面（SYS）から各モードへシームレスに遷移します。

### 1. SYS Mode (System)
システムの基本画面。各モードへのゲートウェイとして機能します。
* `PRG`: ラダー編集モードへ
* `MON`: ライブモニターモードへ
* `SAV` / `LOD`: EEPROMへのセーブ・ロード画面へ

### 2. PRG Mode (Program Editor)
ラダー図を直接編集するモードです。
* ジョイスティックでカーソルを移動し、クリックでパレットを展開。
* パレットから任意の接点、コイル、縦線、削除（DEL）、テキスト（TXT）を選択して配置。

### 3. MON Mode (Live Monitor)
PLCの実行と監視を行うモードです。
* `RUN`: PLC仮想マシンを起動し、制御を開始。右上に輝く運転アイコンが点灯します。
* `STP`: PLCを即座に停止し、すべての出力をリセットします。
* `MNM`: (ニーモニック表示) ラダー図をニーモニックコードに変換してリスト表示します。

---

## ⚠️ The 97% Miracle (限界突破の記録)
本ソフトウェアは、CH32V003の最大16,384バイトのフラッシュメモリのうち、**15,904バイト (97%)** を消費して実装されています。
これ以上の機能追加は、1バイト単位での徹底的な最適化、またはハードウェアのアップグレードを必要とする「神の領域」に到達しています。

---

> *"Hardware limitation is just an illusion. True engineering makes it possible."*
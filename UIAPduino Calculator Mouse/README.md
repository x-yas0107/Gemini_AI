# UIAPduino GemOS - Tiny Desktop OS & Calculator

極小のOLEDディスプレイ上でマウス操作可能なGUIを実現した、手のひらサイズのデスクトップOS＆電卓システムです。
マイコン2つ（CH32V003 + RP2040）を連携させ、USBマウスによる本格的なポインティング操作と安定したUI描画を両立させています。

![動作デモ画像またはGIF動画のURLをここに貼る]

## 🌟 特徴 (Features)

* **本格的なGUI環境**: マウス操作によるフリーカーソル、複数ウィンドウライクなメニュー（SYS, FIL1, APP）を備えたOS風UI[cite: 1]。
* **高機能電卓アプリ (APP Ver0.32c)**[cite: 1]:
  * 15行の計算履歴表示（EEPROMによる記憶機能付き）[cite: 1]。
  * 進数変換機能（DEC:10進数 / HEX:16進数 / BIN:2進数）搭載[cite: 1]。
  * 独自の当たり判定（ラッチ・システム）による、重い画面描画中の「クリック取りこぼし」を完全に防ぐアーキテクチャ[cite: 1]。
* **USBマウスホスト (RP2040)**:
  * 一般的な有線マウスと、多機能な5ボタンマウスの両方に対応[cite: 2]。
  * GP1ピンにスイッチを繋ぐだけの簡単モード切替（LEDインジケータ付き）[cite: 2]。

## 🧱 ハードウェア構成 (Hardware Architecture)

本システムは、役割を分担する2つのマイクロコントローラーで構成されています。

1. **Host MCU (Waveshare RP2040-Zero)**[cite: 2]: 
   USBマウスを接続するホストとして機能。マウスからの移動量とクリック情報を解読し、115200bpsのシリアル通信(`M,x,y,click`フォーマット)でMain MCUへ送信します[cite: 2]。
2. **Main MCU (CH32V003F4P6)**[cite: 1]: 
   「GemOS」を実行するメイン頭脳。RP2040からのシリアルデータをバックグラウンドで受信し、I2C接続されたOLEDの描画と電卓の演算ロジックを処理します[cite: 1]。

### 必要な部品
* CH32V003F4P6 マイコンボード
* Waveshare RP2040-Zero (または同等のRP2040ボード)
* OLEDディスプレイ (I2C接続 / アドレス `0x78`)[cite: 1]
* EEPROM (I2C接続 / アドレス `0x50`)[cite: 1]
* 物理スイッチ (トグルスイッチやスライドスイッチ等、モード切替用)
* USB Type-C to Type-A 変換アダプタ (マウス接続用)
* USBマウス

## 🔌 配線マップ (Wiring / Pinout)

### RP2040 (Mouse Host)
| RP2040 Pin | 接続先 | 役割 |
| :--- | :--- | :--- |
| **TX (Serial1)** | CH32V003 `PD6` | CH32V003へのデータ送信 (115200bps)[cite: 1, 2] |
| **GP1** | モード切替スイッチ | スイッチでGNDと接続/切断を切り替え<br>・ON (GND接続): 5ボタンマウスモード (紫LED)[cite: 2]<br>・OFF (開放): 標準有線マウスモード (白LED)[cite: 2] |
| **GND** | スイッチの片側 / CH32のGND | モード切替スイッチ用 および 共通グラウンド |
| **USB D+/D-** | マウス | USBホスト通信 |

### CH32V003 (GemOS Main)
| CH32V003 Pin | 接続先 | 役割 |
| :--- | :--- | :--- |
| **PD6 (RX)** | RP2040 `TX` | マウスデータの受信 (USART1)[cite: 1] |
| **PC6 (SDA)** | OLED / EEPROM `SDA` | ソフトウェアI2C データライン[cite: 1] |
| **PC7 (SCL)** | OLED / EEPROM `SCL` | ソフトウェアI2C クロックライン[cite: 1] |
| **PC3** | タクトスイッチ等 | PC LINK MODE スイッチ (Active Low)[cite: 1] |

## 💻 ソフトウェアのセットアップ (Software Setup)

開発には **Arduino IDE** を使用します。

### RP2040側の書き込み
1. ボードマネージャーから `Raspberry Pi Pico/RP2040` 関連のパッケージをインストール。
2. 以下のライブラリをインストール:
   * `Adafruit TinyUSB Library`[cite: 2]
   * `Adafruit NeoPixel`[cite: 2]
3. ツールメニューから **USB Stack: "Adafruit TinyUSB"** を選択して書き込み。

### CH32V003側の書き込み
1. UIAP CH32V ボードパッケージをインストール（FQBN: `UIAP:ch32v:CH32V00x_EVT`）。
2. WCH-LinkE等の専用書き込み器を使用してプログラム（`UIAPduino_GemOS`）を書き込みます。

## 🎮 使い方 (How to Use)

1. システムに電源を入れ、RP2040にマウスを接続します。
2. RP2040上のLEDが光り、現在のマウスモードを示します（白：標準有線モード、紫：5ボタンモード）[cite: 2]。マウスがうまく動かない場合はGP1のスイッチでモードを切り替えてください[cite: 2]。
3. 画面上にカーソルが表示されたら、マウスで操作可能です。
4. 画面上部のタブから `SYS` (システム情報), `FIL1`, `APP` (電卓) を切り替えられます[cite: 1]。
5. 電卓画面の左上にある `DEC`/`HEX`/`BIN` の枠をクリックすると、進数変換モードに入ります[cite: 1]。

## 📄 License
This project is released under the [MIT License](LICENSE).
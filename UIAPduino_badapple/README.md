# 🚀 UIAPduino Bad Apple!! Dedicated Player (v1.00)

**[English Follows Japanese]**

CH32V00xの極限リソースを絞り尽くす、狂気の「Bad Apple!!」専用ハードウェア・プレイヤー。
数キロバイトのメモリ空間で、16MB外部FlashからのバーストリードとOLEDへのダイレクト転送をハックし、驚異の30FPSストリーミングを実現。さらにハードウェアシリアルによるMP3モジュールの完全同期を達成しました。

あとはROMライターでバイナリを焼き、スケッチを書き込むだけ。
制約だらけの極小マイコンが魅せる、限界突破のパフォーマンスを楽しんでください！

### 📦 パッケージ内容 (ZIP Contents)
1. `UIAPduino_BadApple_v1_00.ino` : プレイヤー本体のスケッチ（ソースコード）
2. `video_data.bin` : W25Q128（外部Flash）書き込み用の映像バイナリデータ
3. `0001.mp3` / `0002.mp3` : DFPlayer Mini用の音声ファイル（SDカード用）
4. `README.md` : 本マニュアル

### 🛠️ ハードウェア構成と I/O マップ (Hardware & I/O Map)
UIAPduino (CH32V00x) を中心に、以下の通り結線してください。

| UIAPduino Pin | 接続先デバイス | ピン名称 / 備考 |
| :--- | :--- | :--- |
| **PC1** | SSD1306 (OLED) | SDA (I2C High-Speed Bit-bang) |
| **PC2** | SSD1306 (OLED) | SCL (I2C High-Speed Bit-bang) |
| **PC3** | W25Q128 (Flash)| CS (SPI Chip Select) |
| **PC4** | タクトスイッチ | Button (GNDへ接続・内部プルアップ) |
| **PC5** | W25Q128 (Flash)| SCK (SPI Clock) |
| **PC6** | W25Q128 (Flash)| MOSI (SPI Data Out) |
| **PC7** | W25Q128 (Flash)| MISO (SPI Data In) |
| **PD5** | DFPlayer Mini | RX (UART TX 9600bps 固定) |

### 🚀 遊び方 (How to Play)
1. **ROM焼き:** お手持ちのROMライター（CH341Aなど）を使用し、`video_data.bin` を W25Q128 (16MB SPI Flash) に書き込んでください。
2. **SDカード準備:** 32GB以下・FAT32でフォーマットされたMicroSDカードのルート階層に、同梱の `0001.mp3` などをコピーし、DFPlayer Miniに挿入します。
3. **スケッチ書き込み:** Arduino IDE等からUIAPduinoへ `UIAPduino_BadApple_v1_00.ino` を書き込みます。
4. **起動:** PC4のボタン短押しで動画を選択し、**長押しで再生開始**です。映像と音楽の奇跡のシンクロを体験してください！

*※Hacker's Note:*
映像のタイミングがずれる場合は、スケッチ内の `FRAME_DELAY` の数値を調整してください。1ミリ秒の変更が全体の再生時間に大きく影響します。限界までチューニングして遊んでください！
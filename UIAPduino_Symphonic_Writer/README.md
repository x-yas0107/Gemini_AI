# UIAPduino Symphonic Writer (93C66 Edition)

## 概要 (Overview)
CH32Vマイコン（UIAPduino）を使用して、シリアル通信経由でピアノのように演奏し、その記録を外部ROM（93C66）に保存・再生できるハードウェア・シーケンサーです。独自の非ブロッキング発音処理と、自然な減衰音を生み出すNEE（Non-Exponential Envelope）アルゴリズムにより、心地よいレスポンスと音色を実現しています。

## 開発者 (Developers)
* **yas** : Lead Developer / Hardware Architect
* **Gemini** : Co-Architect / Logic Assistant

## 特徴 (Features)
* **リアルタイム・キーボード演奏:** シリアルコンソール（115200bps）からPCのキーボードを使って直感的に演奏が可能。
* **ゼロ・レイテンシ発音:** 次のキー入力で前の余韻を即座にキャンセルする割り込み処理により、高速な連打や滑らかなメロディラインに対応。
* **ピアノ・エンベロープ (NEE):** 単純な矩形波のビープ音ではなく、アタックとディケイ（1000msのフェードアウト）を持たせた自然な減衰音を生成。
* **ハードウェア録音・再生:** 演奏したHz（音階）と打鍵タイミングを正確に外部EEPROM（93C66）へ記録・再生。
* **デバッグUI内蔵:** ROMのダンプ、アドレス指定書き込み、表示モードの切り替えなど、エンジニア向けのCUIメニューをマイコン内に統合。

## ピン配置 (Pin Map)
| Pin | 機能 (Function) | 接続先 (Connection) |
| :--- | :--- | :--- |
| `PC0` | Audio PWM Output (TIM2 CH3) | スピーカー / 圧電サウンダ |
| `PC2` | 93C66 CS | EEPROM Chip Select |
| `PC3` | 93C66 SK | EEPROM Serial Clock |
| `PC4` | 93C66 DI | EEPROM Data Input |
| `PC7` | 93C66 DO | EEPROM Data Output |
| `PC5` | Play Button | 再生トリガーボタン (Active Low) |
| `PD5` | UART TX | シリアル通信送信 (115200bps) |
| `PD6` | UART RX | シリアル通信受信 (115200bps) |

## シリアルコマンド一覧 (Commands)
シリアルモニタ（改行なし または CR+LF）から以下のコマンドを入力して操作します。

* `K` : **キーボード＆録音モード**。`a`〜`k`キー（大文字で1オクターブ上）で演奏。`R`で録音の開始/終了、`Q`でメニューに戻る。
* `P` : **再生**。ROMに記録されたシーケンスデータを演奏。
* `M` : **表示モード切替 (NORMAL/SILENT)**。`SILENT`にするとシリアル出力が停止し、通信負荷による処理落ちを防ぎます。
* `R` : **ROMダンプ**。全データを一覧表示。（例: `R 2` でアドレス2のデータを確認）
* `W` : **ROM書き込み**。指定アドレスに直接数値を書き込む。（例: `W 2 523`）
* `T` : **テスト発音**。指定アドレスのデータを使って1音だけ発音。（例: `T 2`）
* `C` : **フォーマット**。ROMのデータを全て消去（0xFFFF）して初期化。

## バージョン履歴 (Version History)
* **v3.09** : 録音タイミングの厳密化、レスポンスの最適化、およびデバッグ表示（アドレス、Duration、Attack、Decay）の完全同期版。

---
*Created for the UIAPduino Project.*
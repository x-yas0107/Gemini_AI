# WireFlow CAD

**WireFlow CAD** は、ブラウザ上で動作する軽量かつ高機能な自作基板向けパターン配線・実体配線図作成CADツールです。
インストールは一切不要で、HTMLファイルをブラウザで開くだけですぐに使用できます。

## 🌟 主な特徴

* **ゼロ・インストール:** 単一のHTMLファイル（JS/CSS内包）で動作するため、オフライン環境でも利用可能です。
* **KiCadフットプリント対応:** KiCadのフットプリントファイル（`.kicad_mod`）を直接読み込み、任意のレイヤーへ柔軟にマッピングして取り込むことができます。
* **マルチレイヤー対応:** 表面パターン（青）、裏面パターン（赤）、シルク（白）、外形（黄）の4層をサポートしています。
* **強力な作画・配線機能:**
    * 直角、45度面取り、R丸め、フリー曲線（スプライン）による柔軟な配線。
    * 平行添え（オフセット複写）機能による効率的なバス配線。
* **高精度な印刷機能:** 自作基板作成（トナー転写、感光基板など）に不可欠な、A4実寸印刷、ミラー反転、スケール補正（X/Y独立）、カラー/モノクロ/ネガ反転印刷を標準搭載しています。
* **状態の永続化:** データのJSON形式での保存/読み込み機能に加え、設定（グリッド、レイヤー状態など）はブラウザのローカルストレージに自動保存・復元されます。

## 🚀 使い方

### 起動方法
`WireFlow_CAD 083.html` をダウンロードし、Google Chrome や Firefox などのモダンブラウザで開くだけで起動します。

### 基本操作
基本操作は **「右クリックメニュー」** から開始します。

1. キャンバス上で右クリックし、コンテキストメニューを開きます。
2. 「作画」「編集」「表示」などの大項目から目的のツールやパーツを選択します。
3. 左クリックで配置・作画を行います。

### 画面UI
* **上部メニューバー:**
    * `F1`: ファイル / ライブラリ（基板データの保存・読込、KiCad部品の読込、ライブラリ管理）
    * `F2`: 印刷（印刷ダイアログの表示）
    * `F3`: ヘルプ表示（左上の操作ヘルプ表示ON/OFF）
* **右端・サブメニューバー:** 座標表示（ABS: 用紙原点からの絶対座標 / REL: 作画起点からの相対座標・長さ等の詳細情報）

## ⌨️ マウス＆キーボードショートカット

作画効率を上げるための多彩なショートカットが用意されています。

| キー / マウス | 動作 |
| :--- | :--- |
| **右クリック** | コンテキストメニューを開く / 作画・機能のキャンセル・完了 |
| **左クリック** | 選択、配置、配線の開始・曲がる・確定 |
| **左ドラッグ** | 範囲選択 |
| **中ボタンドラッグ**| 画面のパン（キャンバスの移動） |
| **中ボタンWクリック**| 全体表示（作画範囲にズームフィット） |
| **ホイール (Ctrl押下)**| マウスカーソル位置を基準にズームイン・アウト |
| **F1 / F2 / F3** | ファイルメニュー / 印刷ダイアログ / 操作ヘルプ表示切替 |
| **Ctrl + C / V** | 選択項目のコピー / ペースト（ペースト時は現在のレイヤーへ載せ替え可能） |
| **Delete / Backspace**| 選択項目の削除 |
| **Alt (押下中)** | グリッドスナップを一時解除し、フリー座標で操作 |
| **Shift (押下中)** | 選択状態の追加 / 配線時の優先方向（X/Y）の反転 |
| **Space** | 部品・文字の90度回転 / 配線時のコーナー処理（直角/45度/R）の有効化 |
| **V** | ビア打設 ＆ 表裏レイヤーの自動切り替え |
| **図面枠/文字をWクリック**| テキストの直接編集ダイアログ表示 |

## 🔌 KiCad部品のインポート

KiCadの `.kicad_mod` ファイルを直接インポートし、独自部品として使用可能です。
`F1` メニュー > **「KiCad部品読込」** からファイルを選択します。

* **レイヤーマッピング機能:** 読み込み時にプレビュー小窓を見ながら、KiCad側のレイヤー（F.Cu, B.Cu, F.SilkS, F.Fab など）を、WireFlowのどの基本レイヤー（表面、裏面、シルク、外形）に割り当てるか、直感的に選択できます。
* カスタムパッド形状、円弧、ポリゴンベタ塗り（fp_poly）、部品内テキストなど高度な図形も解析・再現します。

## 🖨️ 印刷機能 (F2)

自作基板のエッチング用マスク作成に最適化された印刷設定ダイアログを備えています。

* **出力カラーモード:**
    * **カラー:** 画面表示通りの色（確認用）。
    * **モノクロ / 黒ベタ:** トナー転写方式などに適した黒塗り潰し。
    * **ネガ反転 / 白抜き:** 感光基板などに適したネガ出力。
* **スケール実測補正:** プリンタの印刷誤差を厳密に補正するため、試し刷りした100mmスケールバーの実測値（X/Y）を入力することで、完璧な実寸印刷が可能です。
* **ミラー反転印刷:** 裏面パターンなどを反転して印刷できます。

## 📂 データの保存とライブラリ管理

* **基板データの保存:** `F1` > 「基板保存 (.json)」で現在のすべての状態（回路図形、カスタム部品など）を1つのファイルとして書き出します。
* **カスタム部品ライブラリ:** インポートしたKiCad部品や作成したカスタム部品は、基板データと一緒に保存されるほか、「ライブラリ保存 (.json)」で部品定義群のみを独立して書き出し、別の基板作画時に再利用することが可能です。

---

### ライセンス (License)
<!--
Version: 0.01
Change History:
- v0.01: MITライセンス条項の作成
-->
## ライセンス (License)

本プロジェクトは **MITライセンス** のもとで公開されています。
（本ツールの開発およびデバッグにはGemini・Grok等のAIアシスタントを活用していますが、生成されたコードの利用に一切の制限はありません）

商用・非商用を問わず、誰でも無償で自由に利用・改変・再配布が可能です。

MIT License

Copyright (c) 2026 yas0107

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
(※必要に応じてここにMIT Licenseなどのライセンス情報を記載してください)
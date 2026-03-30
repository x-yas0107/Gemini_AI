# =================================================================
# Project: UIAPduino_CineStream_Converter
# Version: Ver 1.01
# Date:    2026-03-31
# Authors: yas & Gemini
# * [Change History]
# Ver 1.00: 新規作成。動画からストリーミング用バイナリ生成。
# Ver 1.01: アスペクト比を維持したままリサイズする「自動黒帯(レターボックス)機能」を追加。
#           どんな動画サイズ(16:9, 4:3等)でもキャラが太らず正常な比率で変換可能に。
# * [Description]
# MP4ファイルを読み込み、CH32V003で直接再生可能な専用バイナリデータを作成します。
# 必要なライブラリ: pip install opencv-python pydub numpy Pillow
# ※ FFmpegがシステムにインストールされている必要があります。
# =================================================================

import cv2
import numpy as np
from PIL import Image
from pydub import AudioSegment

# --- 設定値 ---
INPUT_FILE = "input.mp4"       # 変換したい元の動画ファイル名
OUTPUT_FILE = "output.bin"     # 出力される専用バイナリファイル名
TARGET_FPS = 30                # 目標フレームレート
AUDIO_RATE = 15360             # 音声のサンプリング周波数 (1秒間のデータ数)
AUDIO_CHUNK_SIZE = 512         # 1フレームあたりの音声データ量 (15360 / 30)
IMG_WIDTH = 128                # OLEDの横幅
IMG_HEIGHT = 64                # OLEDの縦幅
VIDEO_CHUNK_SIZE = 1024        # 1フレームあたりの映像データ量 (128x64 / 8)

def convert():
    print("変換を開始します...")
    
    # 1. 音声データの準備 (15.36kHz, 8bit, モノラルに変換)
    print("音声を抽出・変換中...")
    try:
        audio = AudioSegment.from_file(INPUT_FILE)
        audio = audio.set_frame_rate(AUDIO_RATE).set_channels(1).set_sample_width(1)
        audio_data = np.array(audio.get_array_of_samples(), dtype=np.uint8)
    except Exception as e:
        print(f"音声の読み込みに失敗しました: {e}")
        return

    # 2. 映像データの準備
    print("映像を抽出・変換中...")
    cap = cv2.VideoCapture(INPUT_FILE)
    if not cap.isOpened():
        print("動画ファイルが開けません。ファイル名を確認してください。")
        return
        
    # 元動画の情報を取得
    fps = cap.get(cv2.CAP_PROP_FPS)
    if fps == 0:
        fps = TARGET_FPS
        
    with open(OUTPUT_FILE, 'wb') as f:
        frame_count = 0
        
        while True:
            # ターゲットFPSに合わせて適切なフレームを読み飛ばしつつ取得
            expected_frame = int(frame_count * (fps / TARGET_FPS))
            cap.set(cv2.CAP_PROP_POS_FRAMES, expected_frame)
            
            ret, frame = cap.read()
            if not ret:
                break # 動画の最後まで来たら終了
            
            # --- V1.01: アスペクト比を維持したリサイズ処理 ---
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            orig_h, orig_w = gray.shape
            
            # 縦横の縮小率を計算し、より「縮小が必要な方」に合わせる
            scale = min(IMG_WIDTH / orig_w, IMG_HEIGHT / orig_h)
            new_w = int(orig_w * scale)
            new_h = int(orig_h * scale)
            
            # 比率を保ったまま縮小
            resized_gray = cv2.resize(gray, (new_w, new_h))
            
            # 128x64の「真っ黒なキャンバス」を作成
            canvas = np.zeros((IMG_HEIGHT, IMG_WIDTH), dtype=np.uint8)
            
            # キャンバスのど真ん中に縮小した画像を貼り付け (上下または左右が黒帯になる)
            x_offset = (IMG_WIDTH - new_w) // 2
            y_offset = (IMG_HEIGHT - new_h) // 2
            canvas[y_offset:y_offset+new_h, x_offset:x_offset+new_w] = resized_gray
            # -----------------------------------------------
            
            # PILライブラリを使って、綺麗に中間色を表現するディザリング処理(1bit化)を行う
            pil_img = Image.fromarray(canvas)
            dithered_img = pil_img.convert('1') # '1'は1bit(白黒ディザリング)モード
            bw_data = np.array(dithered_img, dtype=np.uint8) * 255 # 0と255の配列に戻す
            
            # SSD1306(OLED)が理解できる「縦8ピクセルを1バイト」にする形式に変換
            oled_buffer = bytearray(VIDEO_CHUNK_SIZE)
            for page in range(8):
                for x in range(IMG_WIDTH):
                    byte_val = 0
                    for bit in range(8):
                        y = page * 8 + bit
                        if bw_data[y, x] > 0: # 白(光る)ならビットを立てる
                            byte_val |= (1 << bit)
                    oled_buffer[page * IMG_WIDTH + x] = byte_val
            
            # 音声の切り出し (このフレームに対応する512バイト分)
            audio_start = frame_count * AUDIO_CHUNK_SIZE
            audio_end = audio_start + AUDIO_CHUNK_SIZE
            audio_chunk = audio_data[audio_start:audio_end]
            
            # 音声データが足りない場合(動画の末尾など)は無音(128)で埋める
            if len(audio_chunk) < AUDIO_CHUNK_SIZE:
                pad = np.full(AUDIO_CHUNK_SIZE - len(audio_chunk), 128, dtype=np.uint8)
                audio_chunk = np.concatenate((audio_chunk, pad))
                
            # 【合体】音声(512B) ＋ 映像(1024B) の順番でファイルに書き込む
            f.write(audio_chunk.tobytes())
            f.write(oled_buffer)
            
            frame_count += 1
            if frame_count % 100 == 0:
                print(f"{frame_count} フレーム処理完了...")
                
    cap.release()
    print(f"変換完了！出力ファイル: {OUTPUT_FILE}")
    print(f"総出力サイズ: {frame_count * (AUDIO_CHUNK_SIZE + VIDEO_CHUNK_SIZE)} バイト")

if __name__ == "__main__":
    convert()
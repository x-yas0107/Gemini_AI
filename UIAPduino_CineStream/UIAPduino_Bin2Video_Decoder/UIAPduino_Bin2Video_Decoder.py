# =================================================================
# Project: UIAPduino_Bin2Video_Decoder
# Version: 0.05
# Author: yas / Gemini
# Date: 2026-04-02
# Description: Reverse converter from UIAPduino .bin to MP4. Uses pydub for audio upsampling to fix PC playback noise.
# History:
# 0.01 - Initial release.
# 0.02 - Fixed data interleave order (Audio 512B -> Video 1024B). Added 4x image scaling.
# 0.03 - Upgraded audio decoding to 16-bit and 44.1kHz resampling.
# 0.04 - Switched to AVI format with Uncompressed PCM Audio.
# 0.05 - Reverted to MP4. Integrated Pydub for robust audio decoding (8bit/15.36kHz -> 16bit/44.1kHz) to ensure PC compatibility.
# =================================================================

import cv2
import numpy as np
import os
import subprocess
from pydub import AudioSegment

# --- 設定値 ---
INPUT_BIN = "TRUTH.bin"       # 読み込むバイナリファイル
OUTPUT_MP4 = "preview.mp4"     # 完成する動画ファイル
TARGET_FPS = 30                # 元の変換時に設定したFPS
AUDIO_RATE = 15360             # 音声のサンプリング周波数
AUDIO_CHUNK_SIZE = 512         # 1フレームの音声サイズ
VIDEO_CHUNK_SIZE = 1024        # 1フレームの映像サイズ
IMG_WIDTH = 128
IMG_HEIGHT = 64
SCALE_FACTOR = 4               # PC再生用の拡大率 (4倍 = 512x256)

TEMP_VIDEO = "temp_video.mp4"
TEMP_AUDIO = "temp_audio.wav"

def decode_oled_frame(byte_data):
    image = np.zeros((IMG_HEIGHT, IMG_WIDTH), dtype=np.uint8)
    for page in range(8):
        for x in range(IMG_WIDTH):
            byte = byte_data[page * IMG_WIDTH + x]
            for bit in range(8):
                if (byte & (1 << bit)):
                    image[page * 8 + bit, x] = 255
    return image

def main():
    print("=== UIAPduino Reverse Converter V0.05 ===")
    
    if not os.path.exists(INPUT_BIN):
        print(f"エラー: '{INPUT_BIN}' が見つかりません。")
        return

    file_size = os.path.getsize(INPUT_BIN)
    chunk_total = AUDIO_CHUNK_SIZE + VIDEO_CHUNK_SIZE
    total_frames = file_size // chunk_total

    print(f"変換を開始します... (全 {total_frames} フレーム)")

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out_video = cv2.VideoWriter(TEMP_VIDEO, fourcc, TARGET_FPS, (IMG_WIDTH * SCALE_FACTOR, IMG_HEIGHT * SCALE_FACTOR), isColor=True)
    
    all_audio_data = bytearray()
    frame_count = 0

    with open(INPUT_BIN, "rb") as f:
        while True:
            # 1. 音声データの読み込み (512バイト)
            audio_data = f.read(AUDIO_CHUNK_SIZE)
            if not audio_data or len(audio_data) < AUDIO_CHUNK_SIZE:
                break
            all_audio_data.extend(audio_data)

            # 2. 映像データの読み込み (1024バイト)
            video_data = f.read(VIDEO_CHUNK_SIZE)
            if not video_data or len(video_data) < VIDEO_CHUNK_SIZE:
                break
            
            # ドット絵の復元と拡大
            img_array = decode_oled_frame(video_data)
            img_scaled = cv2.resize(img_array, (IMG_WIDTH * SCALE_FACTOR, IMG_HEIGHT * SCALE_FACTOR), interpolation=cv2.INTER_NEAREST)
            img_bgr = cv2.cvtColor(img_scaled, cv2.COLOR_GRAY2BGR)
            out_video.write(img_bgr)
            
            frame_count += 1
            if frame_count % 100 == 0:
                print(f"{frame_count} / {total_frames} フレーム復元完了...")

    out_video.release()
    print("映像の抽出が完了しました。音声の高音質化処理を開始します...")

    # Pydubによる音声の復元と高音質化 (8bit/15.36kHz -> 16bit/44.1kHz)
    try:
        audio_segment = AudioSegment(
            data=bytes(all_audio_data),
            sample_width=1,
            frame_rate=AUDIO_RATE,
            channels=1
        )
        audio_segment = audio_segment.set_frame_rate(44100).set_sample_width(2)
        audio_segment.export(TEMP_AUDIO, format="wav")
        print("音声の処理が完了しました。")
    except Exception as e:
        print("エラー: Pydubでの音声処理に失敗しました。pydubがインストールされているか確認してください。")
        print(e)
        return

    print("映像と音声の合体処理(FFmpeg)を開始します...")

    # FFmpegで映像と音声を結合 (MP4/AAC規格に合わせてエンコード)
    command = [
        "ffmpeg", "-y", 
        "-i", TEMP_VIDEO, 
        "-i", TEMP_AUDIO, 
        "-c:v", "copy", 
        "-c:a", "aac", 
        "-b:a", "192k",
        OUTPUT_MP4
    ]
    
    try:
        subprocess.run(command, check=True)
        print("合体成功！")
    except Exception as e:
        print("エラー: FFmpegでの合体に失敗しました。")
        print(e)
        return
    finally:
        if os.path.exists(TEMP_VIDEO): os.remove(TEMP_VIDEO)
        if os.path.exists(TEMP_AUDIO): os.remove(TEMP_AUDIO)

    print("=========================================")
    print(f"逆変換が完全に完了しました！")
    print(f"出力ファイル: {OUTPUT_MP4}")
    print("=========================================")

if __name__ == "__main__":
    main()
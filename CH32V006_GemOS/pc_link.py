# *********************************************************************************
# Project Name : GemOS Transfer Tool
# Version      : 1.01
# Date         : 2026-04-26
# Developers   : yas & Gemini
#
# [Change History]
# V1.01 - Added APPVER command parsing to dynamically update APP version.
# V1.00 - Initial transfer script setup with TITLE and PAYLOAD support.
# *********************************************************************************

import serial
import time
import os

# 接続設定
COM_PORT = 'COM7'
BAUD_RATE = 115200

def send_command(ser, cmd):
    print(f"Sending: {cmd}")
    ser.write((cmd + "\r\n").encode())
    time.sleep(0.5)
    print(ser.read_all().decode().strip())

def main():
    if not os.path.exists("CODE.TXT"):
        print("Error: CODE.TXT not found in the current directory.")
        return

    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        print("--- Connected to GemOS ---")
        
        with open("CODE.TXT", "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                
                parts = line.split(",")
                cmd_type = parts[0]
                
                if cmd_type == "TITLE":
                    # TITLE, スロット番号, ID(Hex), カセット名
                    slot = int(parts[1])
                    dev_id = parts[2]
                    name = parts[3].ljust(16, ' ')[:16]
                    cmd = f"W,{slot:02d},{dev_id},{name}"
                    send_command(ser, cmd)
                    
                elif cmd_type == "PAYLOAD":
                    # PAYLOAD, スロット番号, パターン番号, ラベル名, バイト数(Hex), ペイロード(Hex)
                    slot = int(parts[1])
                    pat = int(parts[2])
                    name = parts[3].ljust(8, ' ')[:8]
                    count_hex = parts[4]
                    payload_hex = parts[5]
                    cmd = f"S,{slot:02d},{pat:02d},{name},{count_hex},{payload_hex}"
                    send_command(ser, cmd)
                    
                elif cmd_type == "APPVER":
                    # APPVER, メジャー, マイナー
                    # 例: APPVER, 0, 1 (Ver 0.01 の場合)
                    major = int(parts[1])
                    minor = int(parts[2])
                    # 16進数2桁ずつ（V,00,01）にフォーマットして送信
                    cmd = f"V,{major:02X},{minor:02X}"
                    send_command(ser, cmd)

        ser.close()
        print("--- Transfer Complete ---")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
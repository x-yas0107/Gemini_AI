# ======================================================================================
# Project: UIAPduino Font Uploader
# File: FontUploader.py
# Version: 1.00
# Architect: Gemini & yas
# Date: 2026/03/25
#
# [Change History]
# v1.00: Initial public release. Extracts 7-byte Misaki font data, pads to 8-byte,
#        and transfers via reliable serial handshake to external SPI Flash.
# ======================================================================================

import serial
import time
import sys
import re

COM_PORT = 'COM4'
BAUD_RATE = 115200
TABLE_ADDR = 0x0F0000
FONT_ADDR  = 0x100000
FONT_FILE = "misakiUTF16FontData.h"

def send_block(ser, start_addr, data_bytes):
    hex_stream = "".join([f"{v:02X}" for v in data_bytes])
    total = len(data_bytes)
    sectors = (total // 4096) + 1
    for i in range(sectors):
        addr = start_addr + (i * 4096)
        ser.write(f"E {addr:X}\n".encode())
        time.sleep(1.2)
        print(f"  -> Erased sector at 0x{addr:X}")
    
    print(f"  -> Writing {total} bytes to 0x{start_addr:X}...")
    ser.write(f"B {start_addr:X}\n".encode())
    time.sleep(0.5)
    ser.reset_input_buffer()
    for i in range(0, len(hex_stream), 2):
        ser.write(hex_stream[i:i+2].encode())
        while ser.read(1) != b'.': pass
    ser.write(b"X\n")
    time.sleep(0.5)

print(f"[ START ] Reading {FONT_FILE}...")
try:
    with open(FONT_FILE, 'r', encoding='utf-8') as f:
        content = f.read()

    # --- 1. ftable (16bit配列) を確実に抽出 ---
    # uint16_t ftable の後ろの { ... } を探す
    table_match = re.search(r'uint16_t\s+ftable\s*\[\]\s*=\s*\{(.*?)\};', content, re.DOTALL)
    table_raw = re.findall(r'0x([0-9A-Fa-f]+)', table_match.group(1))
    table_bytes = []
    for h in table_raw:
        val = int(h, 16)
        table_bytes.append(val & 0xFF)
        table_bytes.append((val >> 8) & 0xFF)

    # --- 2. fdata (8bit配列) を確実に抽出 ---
    # uint8_t fdata の後ろの { ... } を探す
    fdata_match = re.search(r'uint8_t\s+fdata\s*\[\]\s*=\s*\{(.*?)\};', content, re.DOTALL)
    fdata_raw = re.findall(r'0x([0-9A-Fa-f]+)', fdata_match.group(1))
    fdata_ints = [int(v, 16) for v in fdata_raw]
    
    font_bytes = []
    char_count = len(fdata_ints) // 7
    for i in range(char_count):
        font_bytes.extend(fdata_ints[i*7 : (i+1)*7])
        font_bytes.append(0x00)

    print(f"--- CHECK ---")
    print(f"Table Entries (Chars): {len(table_raw)}") # ここが1710付近ならOK
    print(f"Font Chars: {char_count}")               # ここも1710付近ならOK
    print(f"-------------")

    if len(table_raw) != char_count:
        print("[ WARN ] Table and Font count mismatch! But continuing...")

    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=3)
    time.sleep(2)
    ser.reset_input_buffer()

    print("\n[ Phase 1 ] Uploading Table...")
    send_block(ser, TABLE_ADDR, table_bytes)

    print("\n[ Phase 2 ] Uploading Font Data...")
    send_block(ser, FONT_ADDR, font_bytes)

    ser.close()
    print("\n[ SUCCESS ] Completed. Now 'U 3042' will work!")

except Exception as e:
    print(f"\n[ ERROR ] {e}")
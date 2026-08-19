/*
 * MouseTamer PIO-USB Test (Ver 0.96)
 * GP10 = D+, GP11 = D-
 * 変更履歴：
 * - 0.96: メイン画面および保存画面のUIに無断で追加された解像度レベル表示を削除し、元の表示に完全復元。
 * - 0.95: ホイールクリック(中ボタン)によるマウス解像度(移動量)の5段階間引き機能を再実装。設定値のEEPROM保存と、切替時のBTLスピーカー音階通知を追加。
 * - 0.94: USBマウスの受信待機コマンド(tuh_hid_receive_report)の実行タイミングをマウント時に復元し、マウス入力が認識されない不具合を修正。プロトコル変更は再接続時に反映される仕様に変更。
 * - 0.93: Arduino IDEのプロトタイプ自動生成仕様による 'MouseSnapshot does not name a type' エラーを回避するため、構造体(MouseSnapshot)の定義位置をファイル上部へ移動。
 * - 0.92: Core間のマウス入力同期(critical_sectionによる完全排他)、EEPROM(CRC検証処理の統合)、5ボタンマウス接続時のHIDプロトコル(Report)要求を修正。
 * - 0.91: 静的解析に基づく重要不具合の全面修正。
 *         ・マウス移動量の周回(オーバーフロー)を防止するため、Core 0/1間のデータ同期をロック付きの消費(ゼロリセット)方式に修正。
 *         ・5-Point Mouseモード時にHIDブートプロトコルを強制せず、正しいレポート形式を要求するよう修正。
 *         ・シリアルモニタの波形描画を改修。パリティおよびストップビットの設定値を反映した正確なフレーム幅(10〜12bit)と波形を描画。
 *         ・EEPROM読み込み時に各設定値の範囲チェックを追加し、データ破損時のフェイルセーフを実装。
 *         ・I2Cスニッファーの波形描画を時間軸(タイムスタンプ差分)ベースに刷新し、正確な通信間隔を可視化。
 *         ・約49.7日連続稼働後のmillis()周回によるタイマー破綻を防ぐため、比較式を全て差分ベースに修正。
 * - 0.90: record_serial_demo() 内の未定義変数（last_data_time, start_time）の宣言漏れを修正。
 * - 0.89: Whack-A-Moleゲームを拡張。横振りハンマー（振りかぶり/振り下ろし＋衝撃）のグラフィックに変更。
 *         EEPROMを用いたハイスコア保存機能を追加。自己ベスト更新時に鳴るファンファーレ音と専用画面を実装。
 * - 0.88: Whack-A-Moleゲームのロジックを刷新。横3×縦2の扇形6穴配置とし、独立した状態遷移アニメーションを実装。
 * - 0.87: GP12, GP13を用いたBTL圧電スピーカー駆動機能を追加 (Core1にてノンブロッキング再生)。
 * - 0.86: I2Cスニッファーのパケット数表示を「現在のパケット/全パケット数」のナビゲーションに拡張。
 * - 0.85: I2Cスニッファーのデコード文字表示位置を修正。ストップコンディション「P」との文字被りを解消。
 * - 0.84: I2Cスニッファーの右端スクロール計算を修正。最終サンプルの波形とラベルが画面内に完全に収まるよう調整。
 * - 0.83: I2Cスニッファーのスクロールをpx連続タイムライン方式に刷新。
 * - 0.82: I2Cスニッファーの左下リスト表示と右下統計表示の文字被りを修正。
 * - 0.81: I2Cスニッファーの画面表示を改善。「0x2C ACK」と綺麗に修正し、ナビゲーション表示を改善。
 * - 0.80: I2Cスニッファー画面等の自動改行(テキストラップ)をオフに設定。
 * - 0.79: Serial Monitorのデモモードでの文字化けとバッファ溢れを修正。フロー制御導入。
 * - 0.78: Serial Monitorの波形の上部を3px下げて振幅を調整し、タイミンググリッドを追加。
 * - 0.77: Serial MonitorのUIをさらに調整。バイト間のGap表示やマーカー位置を修正。
 * - 0.76: Serial Monitorの波形エリアを右端まで拡張し、余白を詰める。
 * - 0.75: Serial Monitorの波形描画にクリップ処理を追加。
 * - 0.74: Serial Monitorのスクロールをpx連続タイムライン方式に刷新。
 * - 0.73: Serial Monitorを再設計し、波形とHEX/ASCIIリストを一体化。
 * - 0.72: Serial MonitorをハードウェアUART+波形再現のハイブリッド化。
 */

#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <pio_usb.h>
#include <EEPROM.h>
#include <pico/sync.h>
#include <limits.h>

#define PIN_SNIFF_SDA 0
#define PIN_SNIFF_SCL 1
#define PIN_OLED_SDA 2
#define PIN_OLED_SCL 3
#define PIN_UART_TX  4
#define PIN_UART_RX  5
#define PIN_I2C_SDA  8
#define PIN_I2C_SCL  9
#define PIN_SET_SW   15
#define PIN_LED      16
#define PIN_USB_DP   10
#define PIN_USB_DM   11
#define PIN_SPK_P    12
#define PIN_SPK_N    13
#define PIN_ENC_A    26
#define PIN_ENC_B    27
#define PIN_ENC_SW   28
#define PIN_ESC_SW   29

#define NUMPIXELS 1
#define MAX_BRIGHTNESS 127
#define MOVE_HOLD_MS      50
#define BLINK_INTERVAL_MS 100
Adafruit_NeoPixel pixel(NUMPIXELS, PIN_LED, NEO_RGB + NEO_KHZ800);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

#define EEPROM_MAGIC 0x4D54
#define CONFIG_VERSION 2
struct __attribute__((packed)) Config {
  uint16_t magic;
  uint8_t version;
  uint8_t auto_start;
  uint8_t mouse_mode;
  uint8_t mouse_scale;
  uint32_t baudrate;
  uint8_t parity;
  uint8_t stop_bit;
  uint8_t i2c_mode;
  uint32_t i2c_speed;
  uint8_t i2c_addr;
  uint16_t high_score;
  uint16_t crc;
};

struct MouseSnapshot {
  int16_t dx;
  int16_t dy;
  uint8_t buttons;
};

bool g_run_active = false;
bool g_auto_start = false; 
uint8_t g_mouse_scale = 1;
uint32_t g_baudrate = 115200;
int g_parity = 0;
int g_stop_bit = 1;

uint8_t g_i2c_mode = 0;
uint32_t g_i2c_speed = 100000;
uint8_t g_i2c_addr = 0x08;
uint16_t g_high_score = 0;

const uint32_t baudrate_list[] = {
  1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};
const int num_baudrates = sizeof(baudrate_list) / sizeof(baudrate_list[0]);
int baudrateCursor = 8;
int parityCursor = 0;
int stopBitCursor = 0;

int i2cModeCursor = 0;
int i2cSpeedCursor = 0;

volatile uint8_t g_i2c_reg_buttons = 0;
volatile int16_t g_i2c_reg_dx = 0;
volatile int16_t g_i2c_reg_dy = 0;

volatile uint32_t g_tone_freq = 0;
volatile uint32_t g_tone_end = 0;
volatile uint32_t g_tone_last_toggle = 0;
volatile bool g_tone_state = false;

enum AppState {
  STATE_MAIN,
  STATE_MENU_ROOT,
  STATE_MENU_RUN,
  STATE_MENU_MOUSE,
  STATE_MENU_SERIAL,
  STATE_MENU_SERIAL_BAUD,
  STATE_MENU_SERIAL_PARITY,
  STATE_MENU_SERIAL_STOP,
  STATE_MENU_I2C,
  STATE_MENU_I2C_MODE,
  STATE_MENU_I2C_SPEED,
  STATE_MENU_I2C_ADDR,
  STATE_MENU_APP,
  STATE_MENU_SAVE_CONFIRM,
  STATE_VISUAL,
  STATE_APP_I2C_SNIFFER,
  STATE_APP_I2C_SNIFFER_VIEW,
  STATE_APP_SERIAL_MONITOR,
  STATE_APP_SERIAL_MONITOR_VIEW,
  STATE_APP_WHACK_A_MOLE
};
AppState currentState = STATE_MAIN;
int menuCursor = 0;
bool buttonSetPressed = false;
bool buttonEscPressed = false;
volatile int encoderDelta = 0;

uint32_t g_ripple_start = 0;
uint8_t g_ripple_btn = 0;
int16_t g_vis_x = 64;
int16_t g_vis_y = 32;

#define VIS_HISTORY_LEN 15
int16_t g_vis_hist_x[VIS_HISTORY_LEN];
int16_t g_vis_hist_y[VIS_HISTORY_LEN];

#define SNIFFER_MAX_SAMPLES 4000
#define ZOOM_WIDTH 3
#define PRE_SAMPLES 5
#define SNIFFER_US_PER_PIXEL 2

uint8_t sniffer_state_buf[SNIFFER_MAX_SAMPLES]; 
uint32_t sniffer_time_buf[SNIFFER_MAX_SAMPLES];

struct DecodeEvent {
  uint16_t index;
  char text[6];
  uint8_t type;
};
#define MAX_DECODE_EVENTS 512
DecodeEvent sym_events[MAX_DECODE_EVENTS];
uint16_t sym_event_count = 0;

uint16_t sniffer_sample_count = 0;
uint32_t sniffer_view_offset_us = 0;
uint32_t sniffer_capture_end_time = 0;

uint32_t i2c_freq_khz = 0;
uint16_t total_packets = 0;

// === Serial Monitor ハイブリッド用 ===
#define UART_MAX_BYTES 200
struct UartByteEvent {
  uint32_t t;
  uint8_t val;
};
UartByteEvent uart_bytes[UART_MAX_BYTES];
uint16_t uart_byte_count = 0;
int32_t serial_view_offset = 0;

Adafruit_USBH_Host USBHost;

critical_section_t g_mouse_lock;
volatile bool g_mouse_lock_ready = false;
bool g_mouse_mounted = false;
uint8_t g_mouse_mode = 0;

// Core 1 accumulates reports; Core 0 consumes one signed-16-bit chunk per loop.
// The critical section protects data shared by the two RP2040 cores.
int64_t g_pending_dx = 0;
int64_t g_pending_dy = 0;
uint8_t g_current_buttons = 0;

int8_t g_led_dx = 0;
int8_t g_led_dy = 0;
uint8_t g_led_buttons = 0;
uint32_t g_last_move_time = 0;
uint32_t g_last_tx_time = 0;
uint32_t g_last_i2c_tx_time = 0;

// === Whack-A-Mole 変数と画像 ===
uint8_t wam_state = 0;
uint32_t wam_game_start = 0;
uint16_t wam_score = 0;
bool wam_hit = false;
uint32_t wam_hammer_anim_timer = 0;
uint32_t wam_mole_timer = 0;
uint8_t wam_score_step = 0;

struct Mole {
  int16_t x;
  int16_t y;
  uint8_t state;
  uint32_t timer;
};
Mole moles[6];

const unsigned char bmp_hole[] PROGMEM = {
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x01,0xff,0x80, 0x07,0x00,0xe0, 0x0c,0x00,0x30,
  0x18,0x00,0x18, 0x18,0x00,0x18, 0x0c,0x00,0x30, 0x07,0x00,0xe0, 0x01,0xff,0x80, 0x00,0x00,0x00
};

const unsigned char bmp_mole_half[] PROGMEM = {
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0xff,0x00, 0x03,0x00,0xc0,
  0x06,0x00,0x60, 0x04,0x42,0x20, 0x0c,0x00,0x30, 0x0d,0xff,0xb0, 0x0f,0x42,0xf0, 0x0c,0xc3,0x30,
  0x18,0x00,0x18, 0x18,0x00,0x18, 0x0c,0x00,0x30, 0x07,0x00,0xe0, 0x01,0xff,0x80, 0x00,0x00,0x00
};

const unsigned char bmp_mole_full[] PROGMEM = {
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x3c,0x00,
  0x00,0xff,0x00, 0x01,0x81,0x80, 0x03,0x00,0xc0, 0x02,0x42,0x40, 0x06,0x00,0x60, 0x04,0xc3,0x20,
  0x0c,0x42,0x30, 0x0c,0x00,0x30, 0x0c,0x00,0x30, 0x0d,0xff,0xb0, 0x0f,0x42,0xf0, 0x0c,0xc3,0x30,
  0x18,0x00,0x18, 0x18,0x00,0x18, 0x0c,0x00,0x30, 0x07,0x00,0xe0, 0x01,0xff,0x80, 0x00,0x00,0x00
};

const unsigned char bmp_mole_hit[] PROGMEM = {
  0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x3c,0x00,
  0x00,0xff,0x00, 0x01,0x81,0x80, 0x03,0x00,0xc0, 0x02,0x5a,0x40, 0x06,0x24,0x60, 0x04,0x5a,0x20,
  0x0c,0x00,0x30, 0x0c,0x42,0x30, 0x0c,0x00,0x30, 0x0d,0xff,0xb0, 0x0f,0x42,0xf0, 0x0c,0xc3,0x30,
  0x18,0x00,0x18, 0x18,0x00,0x18, 0x0c,0x00,0x30, 0x07,0x00,0xe0, 0x01,0xff,0x80, 0x00,0x00,0x00
};

const unsigned char bmp_hammer_up[] PROGMEM = {
  0x07,0x00, 0x0f,0x80, 0x1f,0xc0, 0x1f,0xc0, 0x0f,0x80, 0x07,0x00, 0x02,0x00, 0x01,0x00,
  0x00,0x80, 0x00,0x40, 0x00,0x20, 0x00,0x10, 0x00,0x08, 0x00,0x04, 0x00,0x00, 0x00,0x00
};

const unsigned char bmp_hammer_down[] PROGMEM = {
  0x04,0x20, 0x02,0x40, 0x00,0x00, 0x1f,0xf8, 0x3f,0xfc, 0x3f,0xfc, 0x1f,0xf8, 0x00,0x00,
  0x02,0x40, 0x04,0x20, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00
};

static inline uint32_t bit_us(uint32_t baud) {
  if (baud == 0) return 1;
  return (1000000UL + (baud / 2)) / baud;
}

// Safe even when millis()/time_us_32() wraps around.
static inline bool deadline_reached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

static inline uint8_t read_sniffer_lines() {
  uint32_t gpio = sio_hw->gpio_in;
  return (uint8_t)(((gpio >> PIN_SNIFF_SDA) & 0x01) |
                   (((gpio >> PIN_SNIFF_SCL) & 0x01) << 1));
}

static int16_t consume_mouse_axis(int64_t &pending) {
  if (pending > INT16_MAX) {
    pending -= INT16_MAX;
    return INT16_MAX;
  }
  if (pending < INT16_MIN) {
    pending -= INT16_MIN;
    return INT16_MIN;
  }
  int16_t value = (int16_t)pending;
  pending = 0;
  return value;
}

static void accumulate_mouse_report(int16_t dx, int16_t dy, uint8_t buttons) {
  critical_section_enter_blocking(&g_mouse_lock);
  g_pending_dx += dx;
  g_pending_dy += dy;
  g_current_buttons = buttons;
  critical_section_exit(&g_mouse_lock);
}

static MouseSnapshot take_mouse_snapshot() {
  MouseSnapshot snapshot;
  critical_section_enter_blocking(&g_mouse_lock);
  snapshot.dx = consume_mouse_axis(g_pending_dx);
  snapshot.dy = consume_mouse_axis(g_pending_dy);
  snapshot.buttons = g_current_buttons;
  critical_section_exit(&g_mouse_lock);
  return snapshot;
}

static uint8_t current_mouse_mode() {
  critical_section_enter_blocking(&g_mouse_lock);
  uint8_t mode = g_mouse_mode;
  critical_section_exit(&g_mouse_lock);
  return mode;
}

static void set_mouse_mode(uint8_t mode) {
  mode = (mode == 1) ? 1 : 0;
  critical_section_enter_blocking(&g_mouse_lock);
  g_mouse_mode = mode;
  critical_section_exit(&g_mouse_lock);
}

static bool mouse_is_mounted() {
  critical_section_enter_blocking(&g_mouse_lock);
  bool mounted = g_mouse_mounted;
  critical_section_exit(&g_mouse_lock);
  return mounted;
}

static int16_t saturating_add_i16(int16_t value, int16_t delta) {
  int32_t total = (int32_t)value + delta;
  if (total > INT16_MAX) return INT16_MAX;
  if (total < INT16_MIN) return INT16_MIN;
  return (int16_t)total;
}

static bool is_supported_baudrate(uint32_t baudrate) {
  for (int i = 0; i < num_baudrates; ++i) {
    if (baudrate_list[i] == baudrate) return true;
  }
  return false;
}

static uint16_t config_crc(const Config &cfg) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&cfg);
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < sizeof(Config) - sizeof(cfg.crc); ++i) {
    crc ^= (uint16_t)bytes[i] << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static bool config_is_valid(const Config &cfg) {
  if (cfg.magic != EEPROM_MAGIC || cfg.version != CONFIG_VERSION || cfg.crc != config_crc(cfg)) return false;
  if (cfg.auto_start > 1 || cfg.mouse_mode > 1 || cfg.parity > 2 ||
      (cfg.stop_bit != 1 && cfg.stop_bit != 2) || cfg.i2c_mode > 1) return false;
  if (cfg.mouse_scale < 1 || cfg.mouse_scale > 5) return false;
  if (!is_supported_baudrate(cfg.baudrate)) return false;
  if (cfg.i2c_speed != 100000 && cfg.i2c_speed != 400000 && cfg.i2c_speed != 1000000) return false;
  return cfg.i2c_addr >= 0x08 && cfg.i2c_addr <= 0x77;
}

static void save_config() {
  Config cfg = {};
  cfg.magic = EEPROM_MAGIC;
  cfg.version = CONFIG_VERSION;
  cfg.auto_start = g_auto_start ? 1 : 0;
  cfg.mouse_mode = current_mouse_mode();
  cfg.mouse_scale = g_mouse_scale;
  cfg.baudrate = g_baudrate;
  cfg.parity = (uint8_t)g_parity;
  cfg.stop_bit = (uint8_t)g_stop_bit;
  cfg.i2c_mode = g_i2c_mode;
  cfg.i2c_speed = g_i2c_speed;
  cfg.i2c_addr = g_i2c_addr;
  cfg.high_score = g_high_score;
  cfg.crc = config_crc(cfg);
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

static void load_config() {
  Config cfg = {};
  EEPROM.get(0, cfg);
  if (!config_is_valid(cfg)) {
    g_auto_start = false;
    set_mouse_mode(0);
    g_mouse_scale = 1;
    g_baudrate = 115200;
    g_parity = 0;
    g_stop_bit = 1;
    g_i2c_mode = 0;
    g_i2c_speed = 100000;
    g_i2c_addr = 0x08;
    g_high_score = 0;
    save_config();
    return;
  }
  g_auto_start = cfg.auto_start != 0;
  set_mouse_mode(cfg.mouse_mode);
  g_mouse_scale = cfg.mouse_scale;
  g_baudrate = cfg.baudrate;
  g_parity = cfg.parity;
  g_stop_bit = cfg.stop_bit;
  g_i2c_mode = cfg.i2c_mode;
  g_i2c_speed = cfg.i2c_speed;
  g_i2c_addr = cfg.i2c_addr;
  g_high_score = cfg.high_score;
}

void playToneBTL(uint32_t freq, uint32_t duration_ms) {
  critical_section_enter_blocking(&g_mouse_lock);
  g_tone_freq = freq;
  g_tone_end = millis() + duration_ms;
  g_tone_last_toggle = micros();
  g_tone_state = false;
  critical_section_exit(&g_mouse_lock);
}

void record_serial() {
  uart_byte_count = 0;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(F("WATCHING RX..."));
  display.setCursor(0, 36);
  display.print(F("Baud: "));
  display.print(g_baudrate);
  display.display();

  while (Serial2.available()) Serial2.read();

  uint32_t last_data_time = time_us_32();
  uint32_t start_wait = time_us_32();

  while (uart_byte_count < UART_MAX_BYTES) {
    if (digitalRead(PIN_ESC_SW) == LOW) return;

    if (Serial2.available()) {
      uint8_t b = Serial2.read();
      uart_bytes[uart_byte_count].val = b;
      uart_bytes[uart_byte_count].t = time_us_32();
      uart_byte_count++;
      last_data_time = time_us_32();
    } else {
      if (uart_byte_count > 0 && (time_us_32() - last_data_time) > 800000) break;
      if (uart_byte_count == 0 && (time_us_32() - start_wait) > 3000000) break;
    }
  }
}

void record_serial_demo() {
  uart_byte_count = 0;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(F("DEMO LOOPBACK..."));
  display.display();

  while (Serial2.available()) Serial2.read();
  delay(10);

  const char* demo_text = "MouseTamer Ver 0.96\r\nHello World\r\n";
  int len = strlen(demo_text);
  int sent = 0;

  uint32_t last_data_time = time_us_32();
  uint32_t start_time = time_us_32();

  while (uart_byte_count < UART_MAX_BYTES) {
    if (sent < len && Serial2.availableForWrite() > 0) {
      if (sent - uart_byte_count < 16) {
        Serial2.write(demo_text[sent++]);
      }
    }

    if (Serial2.available()) {
      uint8_t b = Serial2.read();
      uart_bytes[uart_byte_count].val = b;
      uart_bytes[uart_byte_count].t = time_us_32();
      uart_byte_count++;
      last_data_time = time_us_32();
    } else {
      if (sent >= len && uart_byte_count > 0 && (time_us_32() - last_data_time) > 300000) break;
      if (sent >= len && uart_byte_count == 0 && (time_us_32() - start_time) > 1000000) break;
      if (time_us_32() - start_time > 3000000) break; 
    }
  }
}

void draw_clipped_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, int16_t clip_x0, int16_t clip_x1) {
  if (x0 == x1) {
    if (x0 >= clip_x0 && x0 <= clip_x1) {
      display.drawLine(x0, y0, x1, y1, color);
    }
  } else {
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (x1 < clip_x0 || x0 > clip_x1) return;
    if (x0 < clip_x0) x0 = clip_x0;
    if (x1 > clip_x1) x1 = clip_x1;
    display.drawLine(x0, y0, x1, y1, color);
  }
}

int uart_frame_bits() {
  return 1 + 8 + (g_parity == 0 ? 0 : 1) + g_stop_bit;
}

void draw_uart_frame(int16_t x_start, uint8_t val, uint8_t p_y, int pixels_per_bit, int16_t clip_x0, int16_t clip_x1) {
  if (pixels_per_bit < 1) pixels_per_bit = 1;

  uint8_t high_y = p_y + 3;
  uint8_t low_y = p_y + 8;

  draw_clipped_line(x_start, low_y, x_start + pixels_per_bit - 1, low_y, SSD1306_WHITE, clip_x0, clip_x1);
  draw_clipped_line(x_start, high_y, x_start, low_y, SSD1306_WHITE, clip_x0, clip_x1);

  int x = x_start + pixels_per_bit;

  bool previous_bit = false;
  uint8_t one_count = 0;
  for (int b = 0; b < 8; b++) {
    bool bit = (val >> b) & 0x01;
    if (bit) ++one_count;
    uint8_t ly = bit ? high_y : low_y;
    draw_clipped_line(x, ly, x + pixels_per_bit - 1, ly, SSD1306_WHITE, clip_x0, clip_x1);
    if (bit != previous_bit) {
      draw_clipped_line(x, high_y, x, low_y, SSD1306_WHITE, clip_x0, clip_x1);
    }
    previous_bit = bit;
    x += pixels_per_bit;
  }

  if (g_parity != 0) {
    bool parity_bit = (g_parity == 1) ? ((one_count & 1) != 0) : ((one_count & 1) == 0);
    uint8_t ly = parity_bit ? high_y : low_y;
    draw_clipped_line(x, ly, x + pixels_per_bit - 1, ly, SSD1306_WHITE, clip_x0, clip_x1);
    if (parity_bit != previous_bit) {
      draw_clipped_line(x, high_y, x, low_y, SSD1306_WHITE, clip_x0, clip_x1);
    }
    previous_bit = parity_bit;
    x += pixels_per_bit;
  }

  for (int stop = 0; stop < g_stop_bit; ++stop) {
    draw_clipped_line(x, high_y, x + pixels_per_bit - 1, high_y, SSD1306_WHITE, clip_x0, clip_x1);
    if (!previous_bit && stop == 0) {
      draw_clipped_line(x, high_y, x, low_y, SSD1306_WHITE, clip_x0, clip_x1);
    }
    previous_bit = true;
    x += pixels_per_bit;
  }

  for (int i = 0; i <= uart_frame_bits(); i++) {
    int gx = x_start + pixels_per_bit * i;
    draw_clipped_line(gx, p_y, gx, p_y + 1, SSD1306_WHITE, clip_x0, clip_x1);
  }
}

void record_i2c() {
  sniffer_sample_count = 0;
  uint8_t reg_val;
  uint32_t idle_counter = 0;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(4, 24);
  display.print(F("WATCHING..."));
  display.display();

  while(idle_counter < 50000) {
    reg_val = read_sniffer_lines();
    if (reg_val == 0x03) idle_counter++;
    else idle_counter = 0;
    if (digitalRead(PIN_ESC_SW) == LOW) return;
  }

  while(1) {
    reg_val = read_sniffer_lines();
    if (reg_val == 0x02) break;
    if (digitalRead(PIN_ESC_SW) == LOW) return;
  }

  for(uint8_t i=0; i < PRE_SAMPLES; i++) {
    sniffer_state_buf[i] = 0x03; 
    sniffer_time_buf[i] = time_us_32();
  }
  sniffer_state_buf[PRE_SAMPLES] = reg_val;
  sniffer_time_buf[PRE_SAMPLES] = time_us_32();
  sniffer_sample_count = PRE_SAMPLES + 1;

  uint8_t last_reg = reg_val;
  uint32_t last_change = time_us_32();
  
  while(sniffer_sample_count < SNIFFER_MAX_SAMPLES) {
    reg_val = read_sniffer_lines();
    if (reg_val != last_reg) {
      sniffer_time_buf[sniffer_sample_count] = time_us_32();
      sniffer_state_buf[sniffer_sample_count++] = reg_val;
      last_reg = reg_val;
      last_change = time_us_32();
    } else {
      if ((time_us_32() - last_change) > 500000) break; 
    }
  }
  sniffer_capture_end_time = last_change;
}

void decode_i2c() {
  sym_event_count = 0; 
  uint8_t bit_count = 0, current_byte = 0;
  bool in_transfer = false;
  uint16_t byte_start_idx = 0;

  uint32_t scl_rise_time = 0;
  uint32_t total_period_us = 0;
  uint32_t period_count = 0;
  
  total_packets = 0;
  i2c_freq_khz = 0;

  for (uint16_t i = 1; i < sniffer_sample_count; i++) {
    if (sym_event_count >= MAX_DECODE_EVENTS) break; 
    uint8_t prev = sniffer_state_buf[i-1], curr = sniffer_state_buf[i];
    bool scl_p = (prev & 0x02) != 0, scl_c = (curr & 0x02) != 0;
    bool sda_p = (prev & 0x01) != 0, sda_c = (curr & 0x01) != 0;

    if (!scl_p && scl_c) {
      if (in_transfer) {
        if (scl_rise_time != 0) {
          uint32_t period = sniffer_time_buf[i] - scl_rise_time;
          if (period > 0 && period < 1000) {
            total_period_us += period;
            period_count++;
          }
        }
        scl_rise_time = sniffer_time_buf[i];
      } else {
        scl_rise_time = 0;
      }
    }

    if (scl_p && scl_c && sda_p && !sda_c) {
      sym_events[sym_event_count].index = i;
      strcpy(sym_events[sym_event_count].text, "S");
      sym_events[sym_event_count].type = 0;
      sym_event_count++;
      in_transfer = true; bit_count = 0; current_byte = 0;
      total_packets++;
    } 
    else if (scl_p && scl_c && !sda_p && sda_c) {
      sym_events[sym_event_count].index = i;
      strcpy(sym_events[sym_event_count].text, "P");
      sym_events[sym_event_count].type = 1;
      sym_event_count++;
      in_transfer = false;
    } 
    else if (!scl_p && scl_c && in_transfer) {
      if (bit_count == 0) {
        byte_start_idx = i;
      }
      if (bit_count < 8) {
        current_byte = (current_byte << 1) | (sda_c ? 1 : 0);
        bit_count++;
      } else {
        const char hex_chars[] = "0123456789ABCDEF";
        sym_events[sym_event_count].index = (byte_start_idx + i) / 2;
        sym_events[sym_event_count].text[0] = hex_chars[(current_byte >> 4) & 0x0F];
        sym_events[sym_event_count].text[1] = hex_chars[current_byte & 0x0F];
        sym_events[sym_event_count].text[2] = sda_c ? 'N' : 'A';
        sym_events[sym_event_count].text[3] = '\0';
        sym_events[sym_event_count].type = 2;
        sym_event_count++;
        bit_count = 0; current_byte = 0;
      }
    }
  }

  if (total_period_us > 0) {
    i2c_freq_khz = (1000 * period_count) / total_period_us;
  }
}

void updateSerialConfig() {
  uint16_t config = SERIAL_8N1;
  if (g_parity == 0 && g_stop_bit == 1) config = SERIAL_8N1;
  else if (g_parity == 1 && g_stop_bit == 1) config = SERIAL_8E1;
  else if (g_parity == 2 && g_stop_bit == 1) config = SERIAL_8O1;
  else if (g_parity == 0 && g_stop_bit == 2) config = SERIAL_8N2;
  else if (g_parity == 1 && g_stop_bit == 2) config = SERIAL_8E2;
  else if (g_parity == 2 && g_stop_bit == 2) config = SERIAL_8O2;

  Serial2.end();
  Serial2.begin(g_baudrate, config);
}

void onI2CRequest() {
  uint8_t data[5];
  data[0] = g_i2c_reg_buttons;
  data[1] = (uint8_t)(g_i2c_reg_dx & 0xFF);
  data[2] = (uint8_t)((g_i2c_reg_dx >> 8) & 0xFF);
  data[3] = (uint8_t)(g_i2c_reg_dy & 0xFF);
  data[4] = (uint8_t)((g_i2c_reg_dy >> 8) & 0xFF);
  Wire.write(data, 5);
  g_i2c_reg_dx = 0;
  g_i2c_reg_dy = 0;
}

void updateI2CConfig() {
  Wire.end();
  Wire.setSDA(PIN_I2C_SDA);
  Wire.setSCL(PIN_I2C_SCL);
  
  if (g_i2c_mode == 0) {
    Wire.begin();
  } else {
    Wire.begin(g_i2c_addr);
    Wire.onRequest(onI2CRequest);
  }
  Wire.setClock(g_i2c_speed);
}

void encoderISR() {
  static uint8_t old_AB = 0;
  static int8_t encval = 0;
  static uint32_t lastStepTime = 0;
  static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  
  old_AB <<= 2;
  old_AB |= ((digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B));
  
  encval += enc_states[(old_AB & 0x0F)];
  
  if (encval >= 3 && (millis() - lastStepTime > 5)) {
    encoderDelta++;
    encval = 0;
    lastStepTime = millis();
  } else if (encval <= -3 && (millis() - lastStepTime > 5)) {
    encoderDelta--;
    encval = 0;
    lastStepTime = millis();
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  uint8_t protocol = tuh_hid_interface_protocol(dev_addr, instance);
  if (protocol == HID_ITF_PROTOCOL_MOUSE || protocol == HID_ITF_PROTOCOL_NONE) {
    uint8_t mode = current_mouse_mode();
    tuh_hid_set_protocol(dev_addr, instance, mode == 0 ? HID_PROTOCOL_BOOT : HID_PROTOCOL_REPORT);
    tuh_hid_receive_report(dev_addr, instance);
    
    critical_section_enter_blocking(&g_mouse_lock);
    g_mouse_mounted = true;
    critical_section_exit(&g_mouse_lock);
    Serial.println("Mouse MOUNTED");
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  critical_section_enter_blocking(&g_mouse_lock);
  g_mouse_mounted = false;
  g_current_buttons = 0;
  critical_section_exit(&g_mouse_lock);
  Serial.println("Mouse UNMOUNTED");
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint8_t buttons = 0;
  int16_t x = 0, y = 0;
  bool parsed = false;
  uint8_t mode = current_mouse_mode();

  if (mode == 0) {
    if (len >= 3) {
      buttons = report[0] & 0x07;
      x = (int8_t)report[1];
      y = (int8_t)report[2];
      parsed = true;
    }
  } else {
    if (len >= 5) {
      uint8_t offset = 1;
      buttons = report[offset] & 0x07;
      x = (int16_t)(report[offset+1] | (report[offset+2] << 8));
      y = (int8_t)report[offset+3];
      parsed = true;
    }
  }

  if (parsed) accumulate_mouse_report(x, y, buttons);
  tuh_hid_receive_report(dev_addr, instance);
}

void setup1() {
  while (!g_mouse_lock_ready) delay(1);
  delay(10);
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
}

void loop1() {
  USBHost.task();
  
  bool set_speaker = false;
  bool stop_speaker = false;
  bool speaker_high = false;
  critical_section_enter_blocking(&g_mouse_lock);
  if (g_tone_freq > 0) {
    if (deadline_reached(millis(), g_tone_end)) {
      g_tone_freq = 0;
      stop_speaker = true;
    } else {
      uint32_t half_period = 500000 / g_tone_freq;
      if (micros() - g_tone_last_toggle >= half_period) {
        g_tone_state = !g_tone_state;
        g_tone_last_toggle = micros();
        set_speaker = true;
        speaker_high = g_tone_state;
      }
    }
  }
  critical_section_exit(&g_mouse_lock);
  if (stop_speaker) {
    digitalWrite(PIN_SPK_P, LOW);
    digitalWrite(PIN_SPK_N, LOW);
  } else if (set_speaker) {
    digitalWrite(PIN_SPK_P, speaker_high ? HIGH : LOW);
    digitalWrite(PIN_SPK_N, speaker_high ? LOW : HIGH);
  }
}

void setup() {
  critical_section_init(&g_mouse_lock);
  g_mouse_lock_ready = true;

  Serial.begin(115200);
  delay(800);
  Serial.println("MouseTamer PIO-USB v0.96");

  pinMode(PIN_SNIFF_SDA, INPUT);
  pinMode(PIN_SNIFF_SCL, INPUT);
  
  pinMode(PIN_SPK_P, OUTPUT);
  pinMode(PIN_SPK_N, OUTPUT);
  digitalWrite(PIN_SPK_P, LOW);
  digitalWrite(PIN_SPK_N, LOW);

  EEPROM.begin(512);
  load_config();

  Serial2.setTX(PIN_UART_TX);
  Serial2.setRX(PIN_UART_RX);
  updateSerialConfig();

  updateI2CConfig();

  pixel.begin();
  pixel.setBrightness(100);
  pixel.setPixelColor(0, pixel.Color(0, 0, 50));
  pixel.show();

  pinMode(PIN_SET_SW, INPUT_PULLUP);
  pinMode(PIN_ESC_SW, INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encoderISR, CHANGE);

  Wire1.setSDA(PIN_OLED_SDA);
  Wire1.setSCL(PIN_OLED_SCL);
  Wire1.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED fail");
    while (1) delay(100);
  }

  display.clearDisplay();
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("PIO-USB v0.96");
  display.println("Init Core 0...");
  display.display();

  pixel.setPixelColor(0, pixel.Color(0, 40, 0));
  pixel.show();
  
  if (g_auto_start) {
    g_run_active = true;
  }
}

void readButtons() {
  static bool lastSet = HIGH;
  static bool lastEsc = HIGH;
  static uint32_t lastChangeTime = 0;
  static bool setStable = false;
  static bool escStable = false;

  bool setNow = (digitalRead(PIN_SET_SW) == LOW) || (digitalRead(PIN_ENC_SW) == LOW);
  bool escNow = (digitalRead(PIN_ESC_SW) == LOW);

  if (setNow != lastSet || escNow != lastEsc) {
    lastChangeTime = millis();
    lastSet = setNow;
    lastEsc = escNow;
    setStable = false;
    escStable = false;
  }

  if (millis() - lastChangeTime > 30) {
    if (setNow && !setStable) {
      buttonSetPressed = true;
      setStable = true;
    }
    if (escNow && !escStable) {
      buttonEscPressed = true;
      escStable = true;
    }
    if (!setNow) setStable = false;
    if (!escNow) escStable = false;
  }
}

void readInputs(int &cursor, int maxItems) {
  noInterrupts();
  int delta = encoderDelta;
  encoderDelta = 0;
  interrupts();
  
  if (delta != 0) {
    cursor += delta;
    while (cursor >= maxItems) cursor -= maxItems;
    while (cursor < 0) cursor += maxItems;
  }
  readButtons();
}

void drawMenu(const char* title, const char** items, int itemCount, int cursorObj) {
  display.clearDisplay();
  display.setTextWrap(false);
  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  int maxVis = 5;
  int topIdx = 0;

  if (itemCount > maxVis) {
    topIdx = cursorObj - 2;
    if (topIdx < 0) topIdx = 0;
    if (topIdx > itemCount - maxVis) topIdx = itemCount - maxVis;
  }

  for (int i = 0; i < maxVis && (topIdx + i) < itemCount; i++) {
    int actualIdx = topIdx + i;
    int yPos = 12 + (i * 10);
    
    if (actualIdx == cursorObj) {
      display.fillRect(0, yPos - 1, 118, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(2, yPos);
    display.println(items[actualIdx]);
  }
  display.setTextColor(SSD1306_WHITE);

  if (topIdx > 0) {
    display.fillTriangle(124, 12, 120, 16, 128, 16, SSD1306_WHITE);
  }
  if (topIdx + maxVis < itemCount) {
    display.fillTriangle(124, 60, 120, 56, 128, 56, SSD1306_WHITE);
  }
}

void update_led() {
  uint16_t r = 0, g = 0, b = 0;

  if (currentState == STATE_MAIN || currentState == STATE_VISUAL || currentState == STATE_APP_WHACK_A_MOLE) {
    bool left_click  = (g_led_buttons & 0x01);
    bool right_click = (g_led_buttons & 0x02);

    if (millis() - g_last_move_time < MOVE_HOLD_MS) {
      int8_t dx = g_led_dx;
      int8_t dy = g_led_dy;
      if (dx > 0) r += (uint16_t)dx * MAX_BRIGHTNESS / 127;
      else if (dx < 0) { uint8_t v = (uint16_t)(-dx) * MAX_BRIGHTNESS / 127; g += v; b += v; }
      if (dy > 0) g += (uint16_t)dy * MAX_BRIGHTNESS / 127;
      else if (dy < 0) { uint8_t v = (uint16_t)(-dy) * MAX_BRIGHTNESS / 127; r += v; b += v; }
    }

    if (left_click || right_click) {
      bool blink_on = ((millis() / BLINK_INTERVAL_MS) % 2) == 0;
      if (blink_on) {
        if (left_click && right_click) {
          bool toggle = ((millis() / (BLINK_INTERVAL_MS * 2)) % 2) == 0;
          if (toggle) { r = MAX_BRIGHTNESS; g = 0; b = 0; }
          else        { r = 0; g = 0; b = MAX_BRIGHTNESS; }
        } else if (left_click) {
          r = MAX_BRIGHTNESS; g = 0; b = 0;
        } else {
          r = 0; g = 0; b = MAX_BRIGHTNESS;
        }
      }
    }

    if (r > MAX_BRIGHTNESS) r = MAX_BRIGHTNESS;
    if (g > MAX_BRIGHTNESS) g = MAX_BRIGHTNESS;
    if (b > MAX_BRIGHTNESS) b = MAX_BRIGHTNESS;

  } else {
    int colorIndex = 0;
    if (currentState == STATE_MENU_ROOT) {
      colorIndex = menuCursor;
    } else if (currentState == STATE_MENU_RUN) {
      colorIndex = 0;
    } else if (currentState == STATE_MENU_MOUSE) {
      colorIndex = 1;
    } else if (currentState == STATE_MENU_SERIAL || 
               currentState == STATE_MENU_SERIAL_BAUD ||
               currentState == STATE_MENU_SERIAL_PARITY ||
               currentState == STATE_MENU_SERIAL_STOP) {
      colorIndex = 2;
    } else if (currentState == STATE_MENU_I2C ||
               currentState == STATE_MENU_I2C_MODE ||
               currentState == STATE_MENU_I2C_SPEED ||
               currentState == STATE_MENU_I2C_ADDR) {
      colorIndex = 3;
    } else if (currentState == STATE_MENU_APP || 
               currentState == STATE_APP_I2C_SNIFFER ||
               currentState == STATE_APP_I2C_SNIFFER_VIEW ||
               currentState == STATE_APP_SERIAL_MONITOR ||
               currentState == STATE_APP_SERIAL_MONITOR_VIEW) {
      colorIndex = 4;
    } else if (currentState == STATE_MENU_SAVE_CONFIRM) {
      colorIndex = 5;
    }

    switch (colorIndex) {
      case 0: r = 0;  g = 100; b = 0;   break;
      case 1: r = 50; g = 0;   b = 50;  break;
      case 2: r = 50; g = 50;  b = 0;   break;
      case 3: r = 0;  g = 50;  b = 50;  break;
      case 4: r = 0;  g = 0;   b = 100; break;
      case 5: r = 50; g = 50;  b = 50;  break;
    }
  }

  pixel.setPixelColor(0, pixel.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));

  static uint32_t last_led_show = 0;
  if (millis() - last_led_show >= 30) {
    pixel.show();
    last_led_show = millis();
  }
}

void loop() {
  static uint8_t last_buttons = 0;

  MouseSnapshot mouse = take_mouse_snapshot();
  int16_t raw_dx = mouse.dx;
  int16_t raw_dy = mouse.dy;
  uint8_t current_buttons = mouse.buttons;

  static int16_t rem_dx = 0;
  static int16_t rem_dy = 0;

  int16_t current_dx = raw_dx;
  int16_t current_dy = raw_dy;

  if (g_mouse_scale > 1) {
    int32_t total_dx = raw_dx + rem_dx;
    int32_t total_dy = raw_dy + rem_dy;
    current_dx = total_dx / g_mouse_scale;
    current_dy = total_dy / g_mouse_scale;
    rem_dx = total_dx % g_mouse_scale;
    rem_dy = total_dy % g_mouse_scale;
  } else {
    rem_dx = 0;
    rem_dy = 0;
  }

  if (current_dx != 0 || current_dy != 0 || current_buttons != last_buttons) {
    int16_t dx = current_dx;
    int16_t dy = current_dy;
    uint8_t buttons = current_buttons;

    if ((buttons & 0x04) && !(last_buttons & 0x04)) {
      g_mouse_scale++;
      if (g_mouse_scale > 5) g_mouse_scale = 1;
      save_config();
      uint32_t tone_freqs[] = {0, 523, 587, 659, 698, 784};
      playToneBTL(tone_freqs[g_mouse_scale], 100);
    }
    
    last_buttons = buttons;

    if (dx || dy) {
      g_led_dx = constrain(dx, -128, 127);
      g_led_dy = constrain(dy, -128, 127);
      g_last_move_time = millis();

      g_vis_x += dx / 2;
      g_vis_y += dy / 2;
      if (g_vis_x < 8) g_vis_x = 8;
      if (g_vis_x > 120) g_vis_x = 120;
      if (g_vis_y < 8) g_vis_y = 8;
      if (g_vis_y > 56) g_vis_y = 56;
    }
    g_led_buttons = buttons;

    if (buttons & 0x03) {
      g_ripple_start = millis();
      g_ripple_btn = buttons & 0x03;
    }

    if (g_run_active) {
      Serial2.print("M,");
      Serial2.print(dx);
      Serial2.print(",");
      Serial2.print(dy);
      Serial2.print(",");
      Serial2.println(buttons);
      g_last_tx_time = millis();

      if (g_i2c_mode == 0) {
        Wire.beginTransmission(g_i2c_addr);
        Wire.write(buttons);
        Wire.write((uint8_t)(dx & 0xFF));
        Wire.write((uint8_t)((dx >> 8) & 0xFF));
        Wire.write((uint8_t)(dy & 0xFF));
        Wire.write((uint8_t)((dy >> 8) & 0xFF));
        Wire.endTransmission();
        g_last_i2c_tx_time = millis();
      } else {
        noInterrupts();
        g_i2c_reg_buttons = buttons;
        g_i2c_reg_dx = saturating_add_i16(g_i2c_reg_dx, dx);
        g_i2c_reg_dy = saturating_add_i16(g_i2c_reg_dy, dy);
        interrupts();
      }
    }
  }

  update_led();

  static uint32_t last_hist_time = 0;
  if (millis() - last_hist_time > 30) {
    for (int i = VIS_HISTORY_LEN - 1; i > 0; i--) {
      g_vis_hist_x[i] = g_vis_hist_x[i-1];
      g_vis_hist_y[i] = g_vis_hist_y[i-1];
    }
    g_vis_hist_x[0] = g_vis_x;
    g_vis_hist_y[0] = g_vis_y;
    last_hist_time = millis();
  }

  static uint32_t last_ui_time = 0;
  if (millis() - last_ui_time > 40) {
    last_ui_time = millis();

    buttonSetPressed = false;
    buttonEscPressed = false;

    switch (currentState) {
      case STATE_MAIN: {
        readInputs(menuCursor, 1);
        display.clearDisplay();
        display.setTextWrap(false);

        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        display.setCursor(4, 18);
        display.print(F("MouseTamer"));
        display.setFont();

        display.setTextSize(1);
        display.setCursor(0, 32);
        display.println(F("Ver 0.96"));
        
        display.setCursor(0, 46);
        display.print(F("Mouse: "));
        display.println(mouse_is_mounted() ? F("OK") : F("None"));

        display.setCursor(0, 56);
        display.print(F("SET:Menu"));

        int mx = 104;
        int my = 33;
        display.drawRoundRect(mx, my, 18, 24, 4, SSD1306_WHITE);
        display.drawLine(mx + 9, my, mx + 9, my + 8, SSD1306_WHITE);
        if (g_led_buttons & 0x01) {
          display.fillRoundRect(mx + 2, my + 2, 6, 7, 2, SSD1306_WHITE);
        }
        if (g_led_buttons & 0x02) {
          display.fillRoundRect(mx + 10, my + 2, 6, 7, 2, SSD1306_WHITE);
        }
        
        if (millis() - g_last_move_time < MOVE_HOLD_MS) {
          display.fillCircle(mx + 9, my - 4, 2, SSD1306_WHITE);
          
          int cx = mx + 9;
          int cy = my + 16;
          int stepX = (g_led_dx > 0) ? 1 : ((g_led_dx < 0) ? -1 : 0);
          int stepY = (g_led_dy > 0) ? 1 : ((g_led_dy < 0) ? -1 : 0);

          if (stepX != 0 || stepY != 0) {
            int r = 4;
            int sx = cx - (stepX * r);
            int sy = cy - (stepY * r);
            int ex = cx + (stepX * r);
            int ey = cy + (stepY * r);
            
            display.drawLine(sx, sy, ex, ey, SSD1306_WHITE);
            
            if (stepX != 0 && stepY != 0) {
              display.drawLine(ex - stepX*2, ey, ex, ey, SSD1306_WHITE);
              display.drawLine(ex, ey - stepY*2, ex, ey, SSD1306_WHITE);
            } else if (stepX != 0) {
              display.drawLine(ex - stepX*2, ey - 2, ex, ey, SSD1306_WHITE);
              display.drawLine(ex - stepX*2, ey + 2, ex, ey, SSD1306_WHITE);
            } else {
              display.drawLine(ex - 2, ey - stepY*2, ex, ey, SSD1306_WHITE);
              display.drawLine(ex + 2, ey - stepY*2, ex, ey, SSD1306_WHITE);
            }
          }
        }
        
        if (buttonSetPressed) {
          currentState = STATE_MENU_ROOT;
          menuCursor = 0;
        }
        break;
      }

      case STATE_VISUAL: {
        readButtons();
        display.clearDisplay();
        display.setTextWrap(false);

        int cx = g_vis_x;
        int cy = g_vis_y;

        for (int i = 0; i < VIS_HISTORY_LEN - 1; i++) {
          int x1 = g_vis_hist_x[i];
          int y1 = g_vis_hist_y[i];
          int x2 = g_vis_hist_x[i+1];
          int y2 = g_vis_hist_y[i+1];
          
          int r1 = 4 - (i * 4 / VIS_HISTORY_LEN);
          int r2 = 4 - ((i + 1) * 4 / VIS_HISTORY_LEN);

          int steps = max(abs(x1 - x2), abs(y1 - y2));
          if (steps == 0) {
            if (r1 > 0) display.fillCircle(x1, y1, r1, SSD1306_WHITE);
            else display.drawPixel(x1, y1, SSD1306_WHITE);
          } else {
            for (int j = 0; j <= steps; j += 2) {
              int px = x1 + (x2 - x1) * j / steps;
              int py = y1 + (y2 - y1) * j / steps;
              int pr = r1 + (r2 - r1) * j / steps;
              if (pr > 0) display.fillCircle(px, py, pr, SSD1306_WHITE);
              else display.drawPixel(px, py, SSD1306_WHITE);
            }
          }
        }

        if (g_ripple_start > 0) {
          uint32_t age = millis() - g_ripple_start;
          if (age < 600) {
            int rad = age / 15;
            display.drawCircle(cx, cy, rad, SSD1306_WHITE);
            if (rad > 5) display.drawCircle(cx, cy, rad - 5, SSD1306_WHITE);
            if (rad > 10) display.drawCircle(cx, cy, rad - 10, SSD1306_WHITE);
          } else {
            g_ripple_start = 0;
          }
        }

        display.fillCircle(cx, cy, 4, SSD1306_WHITE);
        display.drawCircle(cx, cy, 8, SSD1306_WHITE);
        if (g_led_buttons & 0x01) display.fillCircle(cx - 6, cy - 6, 3, SSD1306_WHITE);
        if (g_led_buttons & 0x02) display.fillCircle(cx + 6, cy - 6, 3, SSD1306_WHITE);

        display.setTextSize(1);
        display.setCursor(0, 56);
        display.print(F("ESC:Exit  Visual Mode"));

        if (buttonEscPressed) {
          currentState = STATE_MENU_RUN;
          menuCursor = 0;
        }
        break;
      }

      case STATE_MENU_ROOT: {
        const char* items[] = {"1. Run", "2. Mouse Setting", "3. Serial Setting", "4. I2C Setting", "5. Application", "6. Save Settings"};
        readInputs(menuCursor, 6);
        drawMenu("MAIN MENU", items, 6, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) currentState = STATE_MENU_RUN;
          else if (menuCursor == 1) currentState = STATE_MENU_MOUSE;
          else if (menuCursor == 2) currentState = STATE_MENU_SERIAL;
          else if (menuCursor == 3) currentState = STATE_MENU_I2C;
          else if (menuCursor == 4) currentState = STATE_MENU_APP;
          else if (menuCursor == 5) currentState = STATE_MENU_SAVE_CONFIRM;
          menuCursor = 0;
        }
        if (buttonEscPressed) currentState = STATE_MAIN;
        break;
      }

      case STATE_MENU_RUN: {
        char runItem1[20];
        char runItem3[20];
        sprintf(runItem1, "1. Start%s", g_run_active ? " [*]" : "");
        sprintf(runItem3, "3. Auto [%s]", g_auto_start ? "ON" : "OFF");
        
        const char* items[] = {runItem1, "2. Stop", runItem3, "4. Visual Mode", "5. Back"};
        readInputs(menuCursor, 5);
        drawMenu("RUN SETTING", items, 5, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) g_run_active = true;
          else if (menuCursor == 1) g_run_active = false;
          else if (menuCursor == 2) g_auto_start = !g_auto_start;
          else if (menuCursor == 3) {
            currentState = STATE_VISUAL;
            g_vis_x = 64;
            g_vis_y = 32;
            for(int i = 0; i < VIS_HISTORY_LEN; i++) {
              g_vis_hist_x[i] = 64;
              g_vis_hist_y[i] = 32;
            }
          }
          else if (menuCursor == 4) currentState = STATE_MENU_ROOT;
        }
        if (buttonEscPressed) currentState = STATE_MENU_ROOT;
        break;
      }

      case STATE_MENU_MOUSE: {
        const char* items[] = {"1. Boot Mouse (3B)", "2. Extended (5B)", "3. Other", "4. Back"};
        readInputs(menuCursor, 4);
        drawMenu("MOUSE SETTING", items, 4, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) {
            set_mouse_mode(0);
            currentState = STATE_MENU_ROOT;
          } else if (menuCursor == 1) {
            set_mouse_mode(1);
            currentState = STATE_MENU_ROOT;
          } else if (menuCursor == 3) {
            currentState = STATE_MENU_ROOT;
          }
        }
        if (buttonEscPressed) currentState = STATE_MENU_ROOT;
        break;
      }

      case STATE_MENU_SERIAL: {
        char baudItem[25];
        char parityItem[25];
        char stopItem[25];
        
        const char* p_str = "None";
        if (g_parity == 1) p_str = "Even";
        else if (g_parity == 2) p_str = "Odd";
        
        sprintf(baudItem, "1. Baudrate [%lu]", g_baudrate);
        sprintf(parityItem, "2. Parity [%s]", p_str);
        sprintf(stopItem, "3. Stop Bit [%d]", g_stop_bit);
        
        const char* items[] = {baudItem, parityItem, stopItem, "4. Back"};
        readInputs(menuCursor, 4);
        drawMenu("SERIAL SETTING", items, 4, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) {
            currentState = STATE_MENU_SERIAL_BAUD;
            for (int i = 0; i < num_baudrates; i++) {
              if (baudrate_list[i] == g_baudrate) {
                baudrateCursor = i;
                break;
              }
            }
          } else if (menuCursor == 1) {
            currentState = STATE_MENU_SERIAL_PARITY;
            parityCursor = g_parity;
          } else if (menuCursor == 2) {
            currentState = STATE_MENU_SERIAL_STOP;
            stopBitCursor = (g_stop_bit == 1) ? 0 : 1;
          } else if (menuCursor == 3) {
             currentState = STATE_MENU_ROOT;
          }
        }
        if (buttonEscPressed) currentState = STATE_MENU_ROOT;
        break;
      }

      case STATE_MENU_SERIAL_BAUD: {
        readInputs(baudrateCursor, num_baudrates);
        
        const char* str_items[num_baudrates];
        char buffer[num_baudrates][12];
        for (int i = 0; i < num_baudrates; i++) {
          sprintf(buffer[i], "%lu", baudrate_list[i]);
          str_items[i] = buffer[i];
        }
        
        drawMenu("BAUDRATE", str_items, num_baudrates, baudrateCursor);
        
        if (buttonSetPressed) {
          g_baudrate = baudrate_list[baudrateCursor];
          updateSerialConfig();
          currentState = STATE_MENU_SERIAL;
        }
        if (buttonEscPressed) currentState = STATE_MENU_SERIAL;
        break;
      }

      case STATE_MENU_SERIAL_PARITY: {
        const char* items[] = {"None", "Even", "Odd"};
        readInputs(parityCursor, 3);
        drawMenu("PARITY", items, 3, parityCursor);
        
        if (buttonSetPressed) {
          g_parity = parityCursor;
          updateSerialConfig();
          currentState = STATE_MENU_SERIAL;
        }
        if (buttonEscPressed) currentState = STATE_MENU_SERIAL;
        break;
      }
      
      case STATE_MENU_SERIAL_STOP: {
        const char* items[] = {"1 bit", "2 bit"};
        readInputs(stopBitCursor, 2);
        drawMenu("STOP BIT", items, 2, stopBitCursor);
        
        if (buttonSetPressed) {
          g_stop_bit = (stopBitCursor == 0) ? 1 : 2;
          updateSerialConfig();
          currentState = STATE_MENU_SERIAL;
        }
        if (buttonEscPressed) currentState = STATE_MENU_SERIAL;
        break;
      }

      case STATE_MENU_I2C: {
        char mdItem[24], spItem[24], adItem[24];
        sprintf(mdItem, "1. Mode [%s]", g_i2c_mode == 0 ? "Master" : "Slave");
        
        const char* spStr = (g_i2c_speed == 1000000) ? "1M" : ((g_i2c_speed == 400000) ? "400k" : "100k");
        sprintf(spItem, "2. Speed [%s]", spStr);
        sprintf(adItem, "3. Address [0x%02X]", g_i2c_addr);
        
        const char* items[] = {mdItem, spItem, adItem, "4. Back"};
        readInputs(menuCursor, 4);
        drawMenu("I2C SETTING", items, 4, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) {
            currentState = STATE_MENU_I2C_MODE;
            i2cModeCursor = g_i2c_mode;
          } else if (menuCursor == 1) {
            currentState = STATE_MENU_I2C_SPEED;
            if (g_i2c_speed == 1000000) i2cSpeedCursor = 2;
            else if (g_i2c_speed == 400000) i2cSpeedCursor = 1;
            else i2cSpeedCursor = 0;
          } else if (menuCursor == 2) {
            currentState = STATE_MENU_I2C_ADDR;
          } else if (menuCursor == 3) {
            currentState = STATE_MENU_ROOT;
          }
        }
        if (buttonEscPressed) currentState = STATE_MENU_ROOT;
        break;
      }

      case STATE_MENU_I2C_MODE: {
        const char* items[] = {"Master", "Slave"};
        readInputs(i2cModeCursor, 2);
        drawMenu("I2C MODE", items, 2, i2cModeCursor);
        
        if (buttonSetPressed) {
          g_i2c_mode = i2cModeCursor;
          updateI2CConfig();
          currentState = STATE_MENU_I2C;
        }
        if (buttonEscPressed) currentState = STATE_MENU_I2C;
        break;
      }

      case STATE_MENU_I2C_SPEED: {
        const char* items[] = {"100kHz", "400kHz", "1MHz"};
        readInputs(i2cSpeedCursor, 3);
        drawMenu("I2C SPEED", items, 3, i2cSpeedCursor);
        
        if (buttonSetPressed) {
          if (i2cSpeedCursor == 0) g_i2c_speed = 100000;
          else if (i2cSpeedCursor == 1) g_i2c_speed = 400000;
          else if (i2cSpeedCursor == 2) g_i2c_speed = 1000000;
          updateI2CConfig();
          currentState = STATE_MENU_I2C;
        }
        if (buttonEscPressed) currentState = STATE_MENU_I2C;
        break;
      }

      case STATE_MENU_I2C_ADDR: {
        noInterrupts();
        int delta = encoderDelta;
        encoderDelta = 0;
        interrupts();

        if (delta != 0) {
          int temp = g_i2c_addr + delta;
          if (temp > 0x77) temp = 0x77;
          if (temp < 0x08) temp = 0x08;
          g_i2c_addr = temp;
        }
        readButtons();
        
        display.clearDisplay();
        display.setTextWrap(false);
        display.setCursor(0, 0);
        display.println(F("I2C ADDRESS"));
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
        
        char buf[24];
        display.setTextSize(2);
        display.setCursor(32, 16);
        sprintf(buf, "0x%02X", g_i2c_addr);
        display.println(buf);
        display.setTextSize(1);
        
        display.setCursor(10, 36);
        sprintf(buf, "7-bit : 0x%02X", g_i2c_addr);
        display.println(buf);
        
        display.setCursor(10, 46);
        sprintf(buf, "8-bit(W): 0x%02X", g_i2c_addr << 1);
        display.println(buf);
        
        display.setCursor(10, 56);
        sprintf(buf, "8-bit(R): 0x%02X", (g_i2c_addr << 1) | 1);
        display.println(buf);
        
        if (buttonSetPressed || buttonEscPressed) {
          updateI2CConfig();
          currentState = STATE_MENU_I2C;
        }
        break;
      }

      case STATE_MENU_APP: {
        const char* items[] = {"1. Serial Monitor", "2. I2C Sniffer", "3. Whack-A-Mole", "4. Back"};
        readInputs(menuCursor, 4);
        drawMenu("APPLICATION", items, 4, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) {
            currentState = STATE_APP_SERIAL_MONITOR;
            menuCursor = 0;
          } else if (menuCursor == 1) {
            currentState = STATE_APP_I2C_SNIFFER;
          } else if (menuCursor == 2) {
            currentState = STATE_APP_WHACK_A_MOLE;
            wam_state = 0;
            moles[0].x = 20; moles[0].y = 16;
            moles[1].x = 52; moles[1].y = 14;
            moles[2].x = 84; moles[2].y = 16;
            moles[3].x = 8;  moles[3].y = 36;
            moles[4].x = 52; moles[4].y = 38;
            moles[5].x = 96; moles[5].y = 36;
          } else if (menuCursor == 3) {
            currentState = STATE_MENU_ROOT;
          }
        }
        if (buttonEscPressed) currentState = STATE_MENU_ROOT;
        break;
      }

      case STATE_APP_SERIAL_MONITOR: {
        const char* items[] = {"1. Start (Wait RX)", "2. Demo Loopback", "3. Back"};
        readInputs(menuCursor, 3);
        drawMenu("SERIAL MONITOR", items, 3, menuCursor);

        if (buttonSetPressed) {
          if (menuCursor == 0) {
            record_serial();
            if (uart_byte_count > 0) {
              currentState = STATE_APP_SERIAL_MONITOR_VIEW;
              serial_view_offset = 0;
            }
          } else if (menuCursor == 1) {
            record_serial_demo();
            if (uart_byte_count > 0) {
              currentState = STATE_APP_SERIAL_MONITOR_VIEW;
              serial_view_offset = 0;
            }
          } else if (menuCursor == 2) {
            currentState = STATE_MENU_APP;
            menuCursor = 0;
          }
        }
        if (buttonEscPressed) {
          currentState = STATE_MENU_APP;
          menuCursor = 0;
        }
        break;
      }

      case STATE_APP_SERIAL_MONITOR_VIEW: {
        noInterrupts();
        int delta = encoderDelta;
        encoderDelta = 0;
        interrupts();

        const int pixels_per_bit = 5;
        const int frame_pixels = pixels_per_bit * uart_frame_bits();
        const int draw_x0 = 16;
        const int draw_x1 = 127;
        const int PX_PER_CLICK = 3;
        const uint32_t US_PER_PIXEL = 50;
        const int GAP_PX_MIN = 2;
        const int GAP_PX_MAX = 20;

        static int32_t frame_x_pos[UART_MAX_BYTES];
        int32_t timeline_width = 0;
        if (uart_byte_count > 0) {
          frame_x_pos[0] = 0;
          for (uint16_t i = 1; i < uart_byte_count; i++) {
            uint32_t dt = uart_bytes[i].t - uart_bytes[i-1].t;
            int32_t gap_px = dt / US_PER_PIXEL;
            if (gap_px < GAP_PX_MIN) gap_px = GAP_PX_MIN;
            if (gap_px > GAP_PX_MAX) gap_px = GAP_PX_MAX;
            frame_x_pos[i] = frame_x_pos[i-1] + frame_pixels + gap_px;
          }
          timeline_width = frame_x_pos[uart_byte_count - 1] + frame_pixels;
        }

        if (delta != 0) {
          int32_t new_off = serial_view_offset + (int32_t)delta * PX_PER_CLICK;
          int32_t max_off = timeline_width - (draw_x1 - draw_x0);
          if (max_off < 0) max_off = 0;
          if (new_off < 0) new_off = 0;
          if (new_off > max_off) new_off = max_off;
          serial_view_offset = new_off;
        }
        readButtons();

        display.clearDisplay();
        display.setTextWrap(false);

        display.setCursor(0, 2);
        display.print(F("RX"));

        int32_t screen_center = serial_view_offset + (draw_x1 - draw_x0) / 2;
        int focus_idx = -1;
        int32_t min_center_dist = 0x7FFFFFFF;

        for (uint16_t i = 0; i < uart_byte_count; i++) {
          int32_t screen_x = draw_x0 + (frame_x_pos[i] - serial_view_offset);

          int32_t prev_end_x = draw_x0;
          if (i > 0) {
             prev_end_x = draw_x0 + (frame_x_pos[i-1] + frame_pixels - serial_view_offset);
          }
          draw_clipped_line((int16_t)prev_end_x, 5, (int16_t)screen_x, 5, SSD1306_WHITE, draw_x0, draw_x1);

          if (i == uart_byte_count - 1) {
             int32_t last_end_x = screen_x + frame_pixels;
             draw_clipped_line((int16_t)last_end_x, 5, draw_x1, 5, SSD1306_WHITE, draw_x0, draw_x1);
          }

          if (screen_x + frame_pixels < draw_x0 || screen_x > draw_x1) continue;

          draw_uart_frame((int16_t)screen_x, uart_bytes[i].val, 2, pixels_per_bit, draw_x0, draw_x1);

          if (screen_x >= draw_x0 && screen_x + frame_pixels <= draw_x1) {
            display.setCursor((int16_t)(screen_x + frame_pixels / 2 - 4), 13);
            if (uart_bytes[i].val < 0x10) display.print(F("0"));
            display.print(uart_bytes[i].val, HEX);
          }
          
          int16_t marker_y0 = 12;
          int16_t marker_y1 = 21;
          draw_clipped_line((int16_t)screen_x, marker_y0, (int16_t)screen_x, marker_y1, SSD1306_WHITE, draw_x0, draw_x1);
          int16_t stop_x = screen_x + pixels_per_bit * (1 + 8 + (g_parity == 0 ? 0 : 1));
          draw_clipped_line((int16_t)stop_x, marker_y0, (int16_t)stop_x, marker_y1, SSD1306_WHITE, draw_x0, draw_x1);

          int32_t center_dist = abs((int32_t)(frame_x_pos[i] + frame_pixels / 2) - screen_center);
          if (center_dist < min_center_dist) {
            min_center_dist = center_dist;
            focus_idx = i;
          }
        }

        display.drawLine(0, 23, 128, 23, SSD1306_WHITE);

        display.setCursor(0, 25);
        display.print(F("Gap:"));
        if (focus_idx >= 0 && (focus_idx + 1) < uart_byte_count) {
          uint32_t dt_us = uart_bytes[focus_idx + 1].t - uart_bytes[focus_idx].t;
          uint32_t ms = dt_us / 1000;
          uint32_t ms_frac = (dt_us % 1000) / 100;
          display.print(ms);
          display.print(F("."));
          display.print(ms_frac);
          display.print(F("ms"));
        } else {
          display.print(F("--"));
        }
        display.setCursor(80, 25);
        display.print(F("ESC:Back"));

        display.setCursor(80, 35);
        display.print(F("N:"));
        display.print(uart_byte_count);

        if (focus_idx >= 0) {
          int start_idx = focus_idx - 1;
          if (start_idx < 0) start_idx = 0;
          for (int row = 0; row < 3 && (start_idx + row) < uart_byte_count; row++) {
            int idx = start_idx + row;
            uint8_t line_y = 35 + row * 9;
            if (idx == focus_idx) {
              display.fillRect(0, line_y - 1, 60, 9, SSD1306_WHITE);
              display.setTextColor(SSD1306_BLACK);
            } else {
              display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(2, line_y);
            uint8_t val = uart_bytes[idx].val;
            if (val < 0x10) display.print(F("0"));
            display.print(val, HEX);
            display.print(F(" | "));
            if (val >= 32 && val <= 126) display.print((char)val);
            else display.print(F("."));
          }
          display.setTextColor(SSD1306_WHITE);
        }

        if (buttonEscPressed) {
          currentState = STATE_APP_SERIAL_MONITOR;
        }
        break;
      }

      case STATE_APP_I2C_SNIFFER: {
        readButtons();
        display.clearDisplay();
        display.setTextWrap(false);
        display.setCursor(0, 0);
        display.println(F("I2C SNIFFER"));
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);
        display.setCursor(10, 24);
        display.print(F("Press SET to Watch"));
        display.setCursor(10, 36);
        display.print(F("ESC: Back"));
        
        if (buttonSetPressed) {
          record_i2c();
          if (sniffer_sample_count > PRE_SAMPLES + 1) {
            decode_i2c();
            currentState = STATE_APP_I2C_SNIFFER_VIEW;
            sniffer_view_offset_us = 0;
          }
        }
        if (buttonEscPressed) {
          currentState = STATE_MENU_APP;
          menuCursor = 1;
        }
        break;
      }

      case STATE_APP_I2C_SNIFFER_VIEW: {
        noInterrupts();
        int delta = encoderDelta;
        encoderDelta = 0;
        interrupts();

        const int draw_x0 = 24;
        const int draw_x1 = 127;
        const uint32_t capture_start = sniffer_time_buf[0];
        const uint32_t capture_span = sniffer_capture_end_time - capture_start;
        const uint32_t visible_us = (draw_x1 - draw_x0 + 1) * SNIFFER_US_PER_PIXEL;

        if (delta != 0) {
          uint32_t max_off = (capture_span > visible_us) ? (capture_span - visible_us) : 0;
          int32_t new_off = (int32_t)sniffer_view_offset_us +
                            (int32_t)delta * (int32_t)(10 * SNIFFER_US_PER_PIXEL);
          if (new_off < 0) new_off = 0;
          if ((uint32_t)new_off > max_off) new_off = (int32_t)max_off;
          sniffer_view_offset_us = (uint32_t)new_off;
        }
        readButtons();

        display.clearDisplay();
        display.setTextWrap(false);

        for(uint8_t sig = 0; sig < 2; sig++) {
          uint8_t p_y = (sig == 0) ? 0 : 12; 
          uint8_t bit_mask = (sig == 0) ? 0x02 : 0x01; 
          display.setCursor(0, p_y + 2);
          display.print((sig == 0) ? F("SCL") : F("SDA"));
          
          for(uint16_t idx = 0; idx < sniffer_sample_count; idx++) {
            uint32_t t0 = sniffer_time_buf[idx] - capture_start;
            uint32_t t1 = (idx + 1 < sniffer_sample_count)
                            ? (sniffer_time_buf[idx + 1] - capture_start)
                            : capture_span;
            if (t1 < sniffer_view_offset_us || t0 > sniffer_view_offset_us + visible_us) continue;
            int32_t screen_x0 = draw_x0 + ((int32_t)t0 - (int32_t)sniffer_view_offset_us) / SNIFFER_US_PER_PIXEL;
            int32_t screen_x1 = draw_x0 + ((int32_t)t1 - (int32_t)sniffer_view_offset_us) / SNIFFER_US_PER_PIXEL;
            bool edge_in_view = t0 >= sniffer_view_offset_us && t0 <= sniffer_view_offset_us + visible_us;
            if (screen_x0 < draw_x0) screen_x0 = draw_x0;
            if (screen_x1 > draw_x1) screen_x1 = draw_x1;
            bool level = (sniffer_state_buf[idx] & bit_mask) != 0;
            uint8_t line_y = level ? p_y : p_y + 8;
            bool edge = (idx > 0) && ((sniffer_state_buf[idx] & bit_mask) != (sniffer_state_buf[idx-1] & bit_mask));
            if (edge && edge_in_view) {
              draw_clipped_line((int16_t)screen_x0, p_y, (int16_t)screen_x0, p_y + 8, SSD1306_WHITE, draw_x0, draw_x1);
            }
            draw_clipped_line((int16_t)screen_x0, line_y, (int16_t)screen_x1, line_y, SSD1306_WHITE, draw_x0, draw_x1);
          }
        }

        for (uint16_t ev = 0; ev < sym_event_count; ev++) {
          uint32_t event_time = sniffer_time_buf[sym_events[ev].index] - capture_start;
          int32_t screen_x = draw_x0 + ((int32_t)event_time - (int32_t)sniffer_view_offset_us) / SNIFFER_US_PER_PIXEL;
          
          if (sym_events[ev].type == 2) {
            screen_x -= 12;
          } else {
            screen_x -= 3;
          }

          if (screen_x >= draw_x0 && screen_x <= draw_x1) {
            display.setCursor((int16_t)screen_x, 24);
            if (sym_events[ev].type == 0 || sym_events[ev].type == 1) {
              display.print(sym_events[ev].text);
            } else {
              display.print(F("0x"));
              display.print(sym_events[ev].text[0]);
              display.print(sym_events[ev].text[1]);
            }
          }
        }

        uint32_t center_time = sniffer_view_offset_us + visible_us / 2;
        
        int nearest_ev = -1;
        uint32_t min_dist = UINT32_MAX;
        for (uint16_t ev = 0; ev < sym_event_count; ev++) {
          uint32_t event_time = sniffer_time_buf[sym_events[ev].index] - capture_start;
          uint32_t dist = (event_time > center_time) ? (event_time - center_time) : (center_time - event_time);
          if (dist < min_dist) {
            min_dist = dist;
            nearest_ev = ev;
          }
        }

        int current_pkt = 0;
        for (uint16_t ev = 0; ev < sym_event_count; ev++) {
          uint32_t event_time = sniffer_time_buf[sym_events[ev].index] - capture_start;
          if (sym_events[ev].type == 0 && event_time <= center_time) {
            current_pkt++;
          }
        }
        if (current_pkt == 0 && total_packets > 0) current_pkt = 1;

        if (nearest_ev >= 0) {
          int start_ev = nearest_ev - 1;
          if (start_ev < 0) start_ev = 0;
          for (int i = 0; i < 3 && (start_ev + i) < sym_event_count; i++) {
            int ev_idx = start_ev + i;
            uint8_t yPos = 36 + (i * 9);
            if (ev_idx == nearest_ev) {
               display.fillRect(0, yPos - 1, 71, 9, SSD1306_WHITE);
               display.setTextColor(SSD1306_BLACK);
            } else {
               display.setTextColor(SSD1306_WHITE);
            }
            display.setCursor(0, yPos);
            display.print(F("PKT:"));
            
            if (sym_events[ev_idx].type == 2) {
               display.print(F("0x"));
               display.print(sym_events[ev_idx].text[0]);
               display.print(sym_events[ev_idx].text[1]);
               display.print(sym_events[ev_idx].text[2] == 'A' ? F(" ACK") : F(" NAK"));
            } else {
               display.print(sym_events[ev_idx].text);
            }
          }
          display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(72, 36);
        display.print(F("Spd:"));
        if (i2c_freq_khz > 0) {
          display.print(i2c_freq_khz);
          display.print(F("k"));
        } else {
          display.print(F("---"));
        }
        
        display.setCursor(72, 46);
        display.print(F("Pkt:"));
        display.print(current_pkt);
        display.print(F("/"));
        display.print(total_packets);
        
        display.setCursor(72, 56);
        display.print(F("t:"));
        display.print(center_time / 1000);
        display.print(F("/"));
        display.print(capture_span / 1000);
        display.print(F("ms"));

        if (buttonEscPressed) {
          currentState = STATE_APP_I2C_SNIFFER;
        }
        break;
      }

      case STATE_APP_WHACK_A_MOLE: {
        readButtons();
        display.clearDisplay();
        
        if (wam_state == 0) {
          for(int i=0; i<6; i++) {
            moles[i].state = 0;
            moles[i].timer = 0;
          }
          display.setCursor(16, 20);
          display.print(F("WHACK-A-MOLE"));
          display.setCursor(16, 40);
          display.print(F("Click to Start"));
          
          if ((g_led_buttons & 0x01) && !wam_hit) {
             wam_state = 1;
             wam_score = 0;
             wam_game_start = millis();
             playToneBTL(1000, 100);
          }
        } else if (wam_state == 1) {
          uint32_t elapsed_game = millis() - wam_game_start;
          uint32_t remain = (elapsed_game >= 30000) ? 0 : (30000 - elapsed_game);
          if (elapsed_game >= 30000) {
            wam_state = 2;
          } else {
            if (random(0, 100) < 5) {
              int r = random(0, 6);
              if (moles[r].state == 0) {
                moles[r].state = 1;
                moles[r].timer = millis() + 150;
              }
            }

            for(int i=0; i<6; i++) {
              if (moles[i].state == 1 && deadline_reached(millis(), moles[i].timer)) {
                moles[i].state = 2;
                moles[i].timer = millis() + random(600, 1200);
              } else if (moles[i].state == 2 && deadline_reached(millis(), moles[i].timer)) {
                moles[i].state = 3;
                moles[i].timer = millis() + 150;
              } else if (moles[i].state == 3 && deadline_reached(millis(), moles[i].timer)) {
                moles[i].state = 0;
              } else if (moles[i].state == 4 && deadline_reached(millis(), moles[i].timer)) {
                moles[i].state = 0;
              }

              const unsigned char* bmp = bmp_hole;
              if (moles[i].state == 1 || moles[i].state == 3) bmp = bmp_mole_half;
              else if (moles[i].state == 2) bmp = bmp_mole_full;
              else if (moles[i].state == 4) bmp = bmp_mole_hit;
              
              display.drawBitmap(moles[i].x, moles[i].y, bmp, 24, 24, SSD1306_WHITE);
            }

            if ((g_led_buttons & 0x01) && !wam_hit) {
               wam_hammer_anim_timer = millis() + 100;
               bool hit_success = false;
               int16_t hx = g_vis_x;
               int16_t hy = g_vis_y;
               
               for(int i=5; i>=0; i--) {
                 if (moles[i].state == 1 || moles[i].state == 2 || moles[i].state == 3) {
                   if (hx >= moles[i].x && hx <= moles[i].x + 24 && hy >= moles[i].y && hy <= moles[i].y + 24) {
                     moles[i].state = 4;
                     moles[i].timer = millis() + 300;
                     wam_score += 10;
                     hit_success = true;
                     playToneBTL(2000, 50);
                     break;
                   }
                 }
               }
               if (!hit_success) {
                   playToneBTL(200, 30);
               }
            }
          }
          
          display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
          display.setCursor(0, 0);
          display.print(F("T:"));
          display.print(remain / 1000);
          display.setCursor(44, 0);
          display.print(F("HI:"));
          display.print(g_high_score);
          display.setCursor(88, 0);
          display.print(F("Sc:"));
          display.print(wam_score);
          
        } else if (wam_state == 2) {
          if (wam_score > g_high_score) {
            g_high_score = wam_score;
            save_config();
            wam_state = 3;
            wam_mole_timer = millis();
            wam_score_step = 0;
          } else {
            wam_state = 4;
            playToneBTL(500, 500);
          }
        } else if (wam_state == 3) {
          display.setCursor(24, 20);
          display.print(F("NEW RECORD!"));
          display.setCursor(30, 35);
          display.print(F("Score: "));
          display.print(wam_score);

          uint32_t elapsed = millis() - wam_mole_timer;
          if (wam_score_step == 0 && elapsed > 0) { playToneBTL(1046, 100); wam_score_step++; }
          if (wam_score_step == 1 && elapsed > 150) { playToneBTL(1046, 100); wam_score_step++; }
          if (wam_score_step == 2 && elapsed > 300) { playToneBTL(1046, 100); wam_score_step++; }
          if (wam_score_step == 3 && elapsed > 450) { playToneBTL(1318, 400); wam_score_step++; }

          if ((g_led_buttons & 0x01) && !wam_hit && elapsed > 1000) {
             wam_state = 0;
          }
        } else if (wam_state == 4) {
          display.setCursor(30, 20);
          display.print(F("FINISH!"));
          display.setCursor(30, 35);
          display.print(F("Score: "));
          display.print(wam_score);
          
          if ((g_led_buttons & 0x01) && !wam_hit) {
             wam_state = 0;
          }
        }
        
        wam_hit = (g_led_buttons & 0x01);
        
        const unsigned char* h_bmp = bmp_hammer_up;
        if (!deadline_reached(millis(), wam_hammer_anim_timer)) {
          h_bmp = bmp_hammer_down;
        }
        display.drawBitmap(g_vis_x - 8, g_vis_y - 8, h_bmp, 16, 16, SSD1306_WHITE);
        
        if (buttonEscPressed) {
          currentState = STATE_MENU_APP;
          menuCursor = 2;
        }
        break;
      }

      case STATE_MENU_SAVE_CONFIRM: {
        char i1[24], i2[24], i3[24], i4[24], i5[24], i6[24], i7[24], i8[24];
        sprintf(i1, "Auto: %s", g_auto_start ? "ON" : "OFF");
        sprintf(i2, "Mouse: %s", current_mouse_mode() == 0 ? "Boot 3B" : "Ext. 5B");
        sprintf(i3, "Baud: %lu", g_baudrate);
        
        const char* p_str = "None";
        if (g_parity == 1) p_str = "Even";
        else if (g_parity == 2) p_str = "Odd";
        sprintf(i4, "Pari: %s", p_str);
        
        sprintf(i5, "Stop: %d", g_stop_bit);
        sprintf(i6, "I2C Md: %s", g_i2c_mode == 0 ? "Master" : "Slave");
        
        const char* spd_str = (g_i2c_speed == 1000000) ? "1M" : ((g_i2c_speed == 400000) ? "400k" : "100k");
        sprintf(i7, "I2C Sp: %s", spd_str);
        sprintf(i8, "I2C Ad: 0x%02X", g_i2c_addr);
        
        const char* items[] = {i1, i2, i3, i4, i5, i6, i7, i8, "-> EXECUTE SAVE", "-> CANCEL"};
        readInputs(menuCursor, 10);
        drawMenu("SAVE SETTINGS", items, 10, menuCursor);
        
        if (buttonSetPressed) {
          if (menuCursor == 8) {
            save_config();
            
            display.clearDisplay();
            display.setTextWrap(false);
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(16, 24);
            display.println(F("Saved OK!"));
            display.display();
            
            pixel.setPixelColor(0, pixel.Color(100, 100, 100));
            pixel.show();
            
            delay(1000); 
            
            display.setTextSize(1);
            currentState = STATE_MENU_ROOT;
            menuCursor = 0;
          } else if (menuCursor == 9) {
            currentState = STATE_MENU_ROOT;
            menuCursor = 0;
          }
        }
        if (buttonEscPressed) {
           currentState = STATE_MENU_ROOT;
           menuCursor = 0;
        }
        break;
      }
    }

    if (g_run_active) {
      if (millis() - g_last_tx_time < 50 || millis() - g_last_i2c_tx_time < 50) {
        display.fillCircle(122, 4, 3, SSD1306_WHITE);
      } else {
        display.drawCircle(122, 4, 3, SSD1306_WHITE);
      }
    }

    display.display();
  }
}
/* Waveshare_RP2040_Zero_Mouse_Host (APP Ver0.14 - Jumper Mode)
 * Ver0.14 : GP1ジャンパーで有線/5ボタンモード切替
 *
 * 配線:
 *   GP1 を GND に繋ぐ → 5ボタンモード（紫点灯で確認）
 *   GP1 を開放       → 有線モード（白点灯で確認）
 */
#include <Arduino.h>
#include "Adafruit_TinyUSB.h"
#include <Adafruit_NeoPixel.h>

#define WS2812_PIN        16
#define WS2812_COUNT      1
#define MAX_BRIGHTNESS    127
#define MOVE_HOLD_MS      50
#define BLINK_INTERVAL_MS 100
#define TX_INTERVAL_MS    10
#define MODE_PIN          1     // GP1

Adafruit_USBH_Host USBHost;
Adafruit_NeoPixel pixel(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

volatile int16_t  g_acc_dx = 0;
volatile int16_t  g_acc_dy = 0;
volatile uint8_t  g_current_buttons = 0;
volatile bool     g_data_updated = false;

uint32_t g_last_tx_time = 0;
uint32_t g_last_move_time = 0;
int8_t   g_led_dx = 0;
int8_t   g_led_dy = 0;
uint8_t  g_led_buttons = 0;

uint8_t  g_dpi_scale = 1;
uint8_t  g_mouse_mode = 0;   // 0=有線, 1=5ボタン

void setup() {
  pinMode(MODE_PIN, INPUT_PULLUP);   // GP1

  Serial1.begin(115200);
  USBHost.begin(0);

  pixel.begin();
  pixel.setBrightness(255);

  // 起動フラッシュ
  pixel.setPixelColor(0, pixel.Color(MAX_BRIGHTNESS, 0, 0));
  pixel.show(); delay(200);
  pixel.setPixelColor(0, pixel.Color(0, MAX_BRIGHTNESS, 0));
  pixel.show(); delay(200);
  pixel.setPixelColor(0, pixel.Color(0, 0, MAX_BRIGHTNESS));
  pixel.show(); delay(200);

  // 現在のモードをLEDで表示
  g_mouse_mode = (digitalRead(MODE_PIN) == LOW) ? 1 : 0;
  if (g_mouse_mode == 0) {
    // 有線モード = 白
    pixel.setPixelColor(0, pixel.Color(MAX_BRIGHTNESS, MAX_BRIGHTNESS, MAX_BRIGHTNESS));
  } else {
    // 5ボタンモード = 紫
    pixel.setPixelColor(0, pixel.Color(MAX_BRIGHTNESS, 0, MAX_BRIGHTNESS));
  }
  pixel.show();
  delay(800);
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));
  pixel.show();
}

void loop() {
  // ジャンパー状態を定期的に読み取り（途中で変えても反映）
  g_mouse_mode = (digitalRead(MODE_PIN) == LOW) ? 1 : 0;

  USBHost.task();

  uint32_t now = millis();
  if (now - g_last_tx_time >= TX_INTERVAL_MS) {
    g_last_tx_time = now;
    process_and_send_data();
  }
  update_led();
}

void process_and_send_data() {
  noInterrupts();
  int16_t dx = g_acc_dx;
  int16_t dy = g_acc_dy;
  uint8_t buttons = g_current_buttons;
  bool updated = g_data_updated;

  g_acc_dx = 0;
  g_acc_dy = 0;
  g_data_updated = false;
  interrupts();

  if (!updated) return;

  // 中ボタンでDPI切替（両モード共通）
  static bool last_mid = false;
  bool mid = (buttons & 0x04) ? true : false;
  if (!last_mid && mid) {
    if (g_dpi_scale == 1)      g_dpi_scale = 2;
    else if (g_dpi_scale == 2) g_dpi_scale = 4;
    else if (g_dpi_scale == 4) g_dpi_scale = 8;
    else                       g_dpi_scale = 1;
  }
  last_mid = mid;

  static int16_t x_rem = 0, y_rem = 0;
  int16_t x_calc = dx + x_rem;
  int16_t y_calc = dy + y_rem;
  int16_t out_x = x_calc / g_dpi_scale;
  int16_t out_y = y_calc / g_dpi_scale;
  x_rem = x_calc % g_dpi_scale;
  y_rem = y_calc % g_dpi_scale;

  if (out_x > 127) out_x = 127; else if (out_x < -128) out_x = -128;
  if (out_y > 127) out_y = 127; else if (out_y < -128) out_y = -128;

  uint8_t left_click = (buttons & 0x01) ? 1 : 0;

  g_led_buttons = buttons;
  if (dx != 0 || dy != 0) {
    if (dx > 127) dx = 127; else if (dx < -128) dx = -128;
    if (dy > 127) dy = 127; else if (dy < -128) dy = -128;
    g_led_dx = (int8_t)dx;
    g_led_dy = (int8_t)dy;
    g_last_move_time = millis();
  }

  Serial1.print("M,");
  Serial1.print(out_x);
  Serial1.print(",");
  Serial1.print(out_y);
  Serial1.print(",");
  Serial1.println(left_click);
}

void update_led() {
  bool left_click  = (g_led_buttons & 0x01);
  bool right_click = (g_led_buttons & 0x02);

  uint16_t r = 0, g = 0, b = 0;

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

  pixel.setPixelColor(0, pixel.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));
  pixel.show();
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  uint8_t protocol = tuh_hid_interface_protocol(dev_addr, instance);
  if (protocol == HID_ITF_PROTOCOL_MOUSE || protocol == HID_ITF_PROTOCOL_NONE) {
    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    tuh_hid_receive_report(dev_addr, instance);
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  uint8_t buttons = 0;
  int16_t x = 0;
  int16_t y = 0;

  if (g_mouse_mode == 0) {
    // 有線モード
    if (len >= 3) {
      buttons = report[0] & 0x07;
      x = (int8_t)report[1];
      y = (int8_t)report[2];
    }
  } else {
    // 5ボタンモード
    if (len >= 5) {
      uint8_t offset = 1;
      buttons = report[offset] & 0x07;
      x = (int16_t)(report[offset + 1] | (report[offset + 2] << 8));
      y = (int8_t)report[offset + 3];
    }
  }

  g_acc_dx += x;
  g_acc_dy += y;
  g_current_buttons = buttons;
  g_data_updated = true;

  tuh_hid_receive_report(dev_addr, instance);
}
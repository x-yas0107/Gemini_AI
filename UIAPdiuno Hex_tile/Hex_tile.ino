/*
 * Version: 0.19
 * Change History:
 * 0.11 - Added hex frame.
 * 0.12 - Paradigm shift to Vector graphics.
 * 0.13 - Optimized for 24x24.
 * 0.14 - Implemented Bezier curves and perfected single-tile shape.
 * 0.15 - Re-introduced full-screen random tiling.
 * 0.16 - Implemented dynamic Bezier control point calculation.
 * 0.17 - Upgraded to Cubic Bezier curves.
 * 0.18 - Reverted to a single uniform tile topology.
 * 0.19 - Implemented advanced RNG seeding using ADC floating noise and human-induced button press duration (micros()). Added adjacency anti-collision logic to prevent identical rotations from clumping, ensuring high-variance organic maze generation.
 */

#include <Arduino.h>
#include <Wire.h>

#define OLED_ADDR 0x3C
#define SWITCH_PIN PC4

static uint8_t oled_buffer[128];
static uint8_t current_page = 0;

static uint8_t gridDirs[6][9];

const int vx[6] = { 0, 10, 10,  0, -10, -10}; 
const int vy[6] = {-12, -6,  6, 12,   6,  -6}; 
const int dx[6] = { 5, 10,  5, -5, -10,  -5}; 
const int dy[6] = {-9,  0,  9,  9,   0,  -9}; 

void sendCommand(uint8_t command) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(command);
  Wire.endTransmission();
}

void initOLED() {
  sendCommand(0xAE);
  sendCommand(0xD5); sendCommand(0x80);
  sendCommand(0xA8); sendCommand(0x3F);
  sendCommand(0xD3); sendCommand(0x00);
  sendCommand(0x40);
  sendCommand(0x8D); sendCommand(0x14);
  sendCommand(0x20); sendCommand(0x00);
  sendCommand(0xA1);
  sendCommand(0xC8);
  sendCommand(0xDA); sendCommand(0x12);
  sendCommand(0x81); sendCommand(0xCF);
  sendCommand(0xD9); sendCommand(0xF1);
  sendCommand(0xDB); sendCommand(0x40);
  sendCommand(0xA4);
  sendCommand(0xA6);
  sendCommand(0xAF);
}

void clearOLED() {
  for(int i = 0; i < 128; i++) oled_buffer[i] = 0;
  for (uint8_t page = 0; page < 8; page++) {
    sendCommand(0xB0 + page);
    sendCommand(0x00);
    sendCommand(0x10);
    for (uint8_t i = 0; i < 128; i += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40);
      for (uint8_t j = 0; j < 16; j++) Wire.write(oled_buffer[i + j]);
      Wire.endTransmission();
    }
  }
}

void drawPixel(int x, int y, uint8_t color) {
  if (x >= 0 && x < 128 && y >= 0 && y < 64) {
    if ((y / 8) == current_page) {
      if (color) {
        oled_buffer[x] |= (1 << (y % 8));
      } else {
        oled_buffer[x] &= ~(1 << (y % 8));
      }
    }
  }
}

void drawLine(int x0, int y0, int x1, int y1, uint8_t color) {
  int delta_x = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int delta_y = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = delta_x + delta_y, e2;
  
  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 >= delta_y) { err += delta_y; x0 += sx; }
    if (e2 <= delta_x) { err += delta_x; y0 += sy; }
  }
}

void drawThickPixel(int x, int y, int radius, uint8_t color) {
  for(int i = -radius; i <= radius; i++) {
    for(int j = -radius; j <= radius; j++) {
      drawPixel(x + i, y + j, color);
    }
  }
}

void drawCubicBezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int radius, uint8_t color) {
  for (int32_t t = 0; t <= 100; t += 4) { 
    int32_t u = 100 - t;
    int32_t tt = t * t;
    int32_t uu = u * u;
    int32_t uuu = uu * u;
    int32_t ttt = tt * t;
    
    int px = (uuu * x0 + 3 * uu * t * x1 + 3 * u * tt * x2 + ttt * x3) / 1000000;
    int py = (uuu * y0 + 3 * uu * t * y1 + 3 * u * tt * y2 + ttt * y3) / 1000000;
    drawThickPixel(px, py, radius, color);
  }
}

void drawRibbon(int edgeA, int edgeB, int cx, int cy, bool isForeground) {
  int x0 = cx + dx[edgeA];
  int y0 = cy + dy[edgeA];
  int x3 = cx + dx[edgeB];
  int y3 = cy + dy[edgeB];
  
  int diff = abs(edgeA - edgeB);
  if (diff > 3) diff = 6 - diff; 
  
  int factor = (diff == 1) ? 60 : 40; 
  
  int x1 = x0 - (dx[edgeA] * factor) / 100;
  int y1 = y0 - (dy[edgeA] * factor) / 100;
  int x2 = x3 - (dx[edgeB] * factor) / 100;
  int y2 = y3 - (dy[edgeB] * factor) / 100;

  if (isForeground) {
    drawCubicBezier(x0, y0, x1, y1, x2, y2, x3, y3, 2, 0); 
  }
  drawCubicBezier(x0, y0, x1, y1, x2, y2, x3, y3, 1, 1);
}

void generateGrid() {
  for (int r = 0; r < 6; r++) {
    for (int c = 0; c < 9; c++) {
      uint8_t rot;
      bool collision;
      int attempts = 0;
      do {
        rot = random(0, 6);
        collision = false;
        if (c > 0 && gridDirs[r][c - 1] == rot) collision = true;
        if (r > 0 && gridDirs[r - 1][c] == rot) collision = true;
        attempts++;
      } while (collision && attempts < 3);
      
      gridDirs[r][c] = rot;
    }
  }
}

void renderVectorHex() {
  for (current_page = 0; current_page < 8; current_page++) {
    for(int i = 0; i < 128; i++) oled_buffer[i] = 0;
    
    int page_min = current_page * 8;
    int page_max = page_min + 7;

    for (int row = -1; row < 5; row++) {
      for (int col = -1; col < 8; col++) {
        int cx = col * 20 + (row % 2 != 0 ? 10 : 0);
        int cy = row * 18;
        
        if (cy + 12 >= page_min && cy - 12 <= page_max) {
          for (int i = 0; i < 6; i++) {
            drawLine(cx + vx[i], cy + vy[i], cx + vx[(i+1)%6], cy + vy[(i+1)%6], 1);
          }
        }
      }
    }

    for (int row = -1; row < 5; row++) {
      for (int col = -1; col < 8; col++) {
        int cx = col * 20 + (row % 2 != 0 ? 10 : 0);
        int cy = row * 18;
        
        if (cy + 12 >= page_min && cy - 12 <= page_max) {
          int rot = gridDirs[row + 1][col + 1];
          
          drawRibbon((5+rot)%6, (0+rot)%6, cx, cy, false);
          drawRibbon((1+rot)%6, (3+rot)%6, cx, cy, false);
          drawRibbon((2+rot)%6, (4+rot)%6, cx, cy, true);
        }
      }
    }

    sendCommand(0xB0 + current_page);
    sendCommand(0x00);
    sendCommand(0x10);
    for (uint8_t i = 0; i < 128; i += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40); 
      for (uint8_t j = 0; j < 16; j++) Wire.write(oled_buffer[i + j]);
      Wire.endTransmission();
    }
  }
}

void setup() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  
  delay(100); 
  Wire.begin();
  
  initOLED();
  clearOLED();
  
  randomSeed(analogRead(0) ^ micros());
  generateGrid();
  renderVectorHex(); 
}

void loop() {
  if (digitalRead(SWITCH_PIN) == LOW) {
    uint32_t pressTime = micros();
    
    delay(200); 
    while(digitalRead(SWITCH_PIN) == LOW) delay(10);
    
    randomSeed(analogRead(0) ^ pressTime ^ micros());
    
    generateGrid();
    renderVectorHex();
  }
}
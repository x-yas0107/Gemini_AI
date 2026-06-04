/*
 * Version: 0.27
 * Change History:
 * 0.24 - Reverted to base 31x15 grid. Implemented Hunt-and-Kill.
 * 0.25 - Hardened boundaries.
 * 0.26 - Implemented "Enclosed Box" logic. Labels are fully walled off into impenetrable pillars with exactly 1 connection hole.
 * 0.27 - Fixed rendering bug causing dotted lines on the right and bottom borders. Separated pillar (intersection) rendering into an independent full-grid pass to ensure all outermost corners are drawn securely before walls are connected.
 */

#include <Arduino.h>
#include <Wire.h>

#define OLED_ADDR 0x3C
#define SWITCH_PIN PC4

static uint8_t cells[31][15]; 
static uint8_t page_buffer[128];
static uint8_t current_page = 0;

const uint8_t init_cmds[] = {
  0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
  0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
  0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
};

const uint8_t in_bmp[11] = {0x00, 0x22, 0x3E, 0x22, 0x00, 0x00, 0x3E, 0x04, 0x08, 0x3E, 0x00};
const uint8_t out_bmp[17] = {0x00, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x1E, 0x20, 0x20, 0x1E, 0x00, 0x02, 0x02, 0x3E, 0x02, 0x02, 0x00};

void initOLED() {
  for(uint8_t i = 0; i < sizeof(init_cmds); i++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(init_cmds[i]);
    Wire.endTransmission();
  }
}

void generateMaze() {
  for(int x = 0; x < 31; x++) {
    for(int y = 0; y < 15; y++) cells[x][y] = 0;
  }

  // Reserve IN Box area (X=0..2, Y=0..1). 0x81 = Reserved + Visited
  for(int x = 0; x <= 2; x++) {
    for(int y = 0; y <= 1; y++) cells[x][y] = 0x81;
  }
  
  // Reserve OUT Box area (X=26..30, Y=13..14).
  for(int x = 26; x <= 30; x++) {
    for(int y = 13; y <= 14; y++) cells[x][y] = 0x81;
  }

  // Start maze generation from a guaranteed safe cell
  int cx = 3, cy = 0;
  cells[cx][cy] |= 0x01; 
  
  while(true) {
    // Kill Phase
    while(true) {
      int dirs[4] = {0, 1, 2, 3};
      for(int i = 0; i < 4; i++) {
        int r = random(0, 4);
        int t = dirs[i]; dirs[i] = dirs[r]; dirs[r] = t;
      }
      
      bool moved = false;
      for(int i = 0; i < 4; i++) {
        int nx = cx, ny = cy;
        if(dirs[i] == 0) ny--;      
        else if(dirs[i] == 1) nx++; 
        else if(dirs[i] == 2) ny++; 
        else if(dirs[i] == 3) nx--; 
        
        if(nx >= 0 && nx <= 30 && ny >= 0 && ny <= 14) {
          if(!(cells[nx][ny] & 0x01)) { // Unvisited
            if(dirs[i] == 0) cells[nx][ny] |= 0x04; 
            else if(dirs[i] == 1) cells[cx][cy] |= 0x02; 
            else if(dirs[i] == 2) cells[cx][cy] |= 0x04; 
            else if(dirs[i] == 3) cells[nx][ny] |= 0x02; 
            
            cells[nx][ny] |= 0x01; 
            cx = nx;
            cy = ny;
            moved = true;
            break;
          }
        }
      }
      if(!moved) break;
    }
    
    // Hunt Phase
    bool found = false;
    for(int y = 0; y <= 14; y++) {
      for(int x = 0; x <= 30; x++) {
        if(!(cells[x][y] & 0x01)) { 
          int dirs[4] = {0, 1, 2, 3};
          for(int i = 0; i < 4; i++) {
            int r = random(0, 4);
            int t = dirs[i]; dirs[i] = dirs[r]; dirs[r] = t;
          }
          
          for(int i = 0; i < 4; i++) {
            int nx = x, ny = y;
            if(dirs[i] == 0) ny--;
            else if(dirs[i] == 1) nx++;
            else if(dirs[i] == 2) ny++;
            else if(dirs[i] == 3) nx--;
            
            if(nx >= 0 && nx <= 30 && ny >= 0 && ny <= 14) {
              // Connect to a visited cell, strictly excluding reserved box regions
              if((cells[nx][ny] & 0x01) && !(cells[nx][ny] & 0x80)) { 
                if(dirs[i] == 0) cells[nx][ny] |= 0x04; 
                else if(dirs[i] == 1) cells[x][y] |= 0x02; 
                else if(dirs[i] == 2) cells[x][y] |= 0x04; 
                else if(dirs[i] == 3) cells[nx][ny] |= 0x02; 
                
                cells[x][y] |= 0x01;
                cx = x;
                cy = y;
                found = true;
                break;
              }
            }
          }
          if(found) break;
        }
      }
      if(found) break;
    }
    if(!found) break; 
  }
}

void draw_p(int x, int y, uint8_t color) {
  if (x >= 0 && x < 128) {
    if (y >= current_page * 8 && y < current_page * 8 + 8) {
      if (color) page_buffer[x] |= (1 << (y % 8));
      else       page_buffer[x] &= ~(1 << (y % 8));
    }
  }
}

void renderScreen() {
  for (current_page = 0; current_page < 8; current_page++) {
    for(int i = 0; i < 128; i++) page_buffer[i] = 0;
    
    // 1. Draw ALL corner pillars first (including outermost X=31 and Y=15 boundaries)
    for (int x = 0; x <= 31; x++) {
      for (int y = 0; y <= 15; y++) {
        draw_p(x * 4 + 2, y * 4 + 2, 1);
      }
    }
    
    // 2. Draw all connecting walls
    for (int x = 0; x <= 30; x++) {
      for (int y = 0; y <= 14; y++) {
        int px = x * 4 + 2;
        int py = y * 4 + 2;
        
        // Absolute Top wall
        if (y == 0) {
          draw_p(px+1, py, 1); draw_p(px+2, py, 1); draw_p(px+3, py, 1);
        }
        // Absolute Left wall
        if (x == 0) {
          draw_p(px, py+1, 1); draw_p(px, py+2, 1); draw_p(px, py+3, 1);
        }
        
        // Right wall
        if (!(cells[x][y] & 0x02)) {
          draw_p(px+4, py+1, 1); draw_p(px+4, py+2, 1); draw_p(px+4, py+3, 1);
        }
        // Bottom wall
        if (!(cells[x][y] & 0x04)) {
          draw_p(px+1, py+4, 1); draw_p(px+2, py+4, 1); draw_p(px+3, py+4, 1);
        }
      }
    }
    
    // 3. Post-Processing: Hollow out IN Box and punch connection hole
    for(int bx = 3; bx <= 13; bx++) {
      for(int by = 3; by <= 9; by++) draw_p(bx, by, 0); // Hollow interior
    }
    draw_p(14, 7, 0); draw_p(14, 8, 0); draw_p(14, 9, 0); // Exact connection hole to Cell(3,1)
    
    for (int dx = 0; dx < 11; dx++) {
      for (int dy = 0; dy < 7; dy++) {
        if (in_bmp[dx] & (1 << dy)) draw_p(3 + dx, 3 + dy, 1);
      }
    }
    
    // 4. Post-Processing: Hollow out OUT Box and punch connection hole
    for(int bx = 107; bx <= 125; bx++) {
      for(int by = 55; by <= 61; by++) draw_p(bx, by, 0); // Hollow interior
    }
    draw_p(106, 55, 0); draw_p(106, 56, 0); draw_p(106, 57, 0); // Exact connection hole from Cell(25,13)
    
    for (int dx = 0; dx < 17; dx++) {
      for (int dy = 0; dy < 7; dy++) {
        if (out_bmp[dx] & (1 << dy)) draw_p(108 + dx, 55 + dy, 1);
      }
    }
    
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(0xB0 + current_page);
    Wire.endTransmission();
    
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
    
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(0x10);
    Wire.endTransmission();
    
    for (uint8_t i = 0; i < 128; i += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40);
      for (uint8_t j = 0; j < 16; j++) {
        Wire.write(page_buffer[i + j]);
      }
      Wire.endTransmission();
    }
  }
}

void setup() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  delay(100); 
  Wire.begin();
  initOLED();
  
  randomSeed(analogRead(0) ^ micros());
  generateMaze();
  renderScreen(); 
}

void loop() {
  if (digitalRead(SWITCH_PIN) == LOW) {
    uint32_t pressTime = micros();
    delay(200); 
    while(digitalRead(SWITCH_PIN) == LOW) delay(10);
    
    randomSeed(analogRead(0) ^ pressTime ^ micros());
    generateMaze();
    renderScreen();
  }
}
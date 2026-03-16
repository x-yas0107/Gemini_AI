/*
 * ======================================================================================
 * Project: UIAPduino_Symphonic_Writer
 * Version: 3.09 (Verbose Playback with Address)
 * Architect: Gemini & yas
 * Date: 2026/03/16
 *
 * [Change History]
 * v3.05: Added UART interrupt check inside playNote for non-blocking performance.
 * v3.06: Fixed recording timing issue (moved lastEventTime update before playNote).
 * v3.07: Fixed 'M' mode toggle and restored playback logs sync.
 * v3.08: Added duration, attack, and decay details to playback logs.
 * v3.09: Added ROM address to playback logs. Added W mode prompt.
 *
 * [Pin Map]
 * - PC0: Audio PWM Output (TIM2 CH3)
 * - PC2: 93C66 CS / PC3: 93C66 SK / PC4: 93C66 DI / PC7: 93C66 DO
 * - PC5: Play Button (Active Low)
 * - PD5: UART TX / PD6: UART RX (115200bps)
 * ======================================================================================
 */

#include <Arduino.h>

// --- 93C66 Microwire Macros ---
#define ROM_CS_HIGH() (GPIOC->OUTDR |= (1 << 2))
#define ROM_CS_LOW()  (GPIOC->OUTDR &= ~(1 << 2))
#define ROM_SK_HIGH() (GPIOC->OUTDR |= (1 << 3))
#define ROM_SK_LOW()  (GPIOC->OUTDR &= ~(1 << 3))
#define ROM_DI_HIGH() (GPIOC->OUTDR |= (1 << 4))
#define ROM_DI_LOW()  (GPIOC->OUTDR &= ~(1 << 4))
#define ROM_DO_READ() (GPIOC->INDR & (1 << 7))

#define MAX_REC_NOTES 84 

struct Note {
  uint16_t freq;
  uint16_t duration;
  uint8_t  attack;
  uint8_t  decay_div;
};

// --- Global Variables ---
char sharedBuf[64];
bool isBatchMode = false;
bool isSilentMode = false;
bool isKeyboardMode = false;
bool isRecording = false;
uint16_t batchAddr = 0;

uint16_t recBuf[MAX_REC_NOTES * 3];
uint8_t recNoteCount = 0;
uint32_t lastEventTime = 0;

int last_freq = -1;
uint16_t current_duty = 0;
uint32_t global_decay_step = 0;

// --- Function Prototypes ---
void initHardware();
void setupHardwareUart();
bool getCharSafe(uint8_t &out);
void initROM();
void setFrequency(uint16_t freq);
void setDuty(uint16_t duty);
void playNote(const Note& note, int next_freq);
uint16_t rom_read_word(uint8_t address);
void rom_write_word(uint8_t address, uint16_t data);
void rom_send_bits(uint16_t data, uint8_t len);
void playFromROM();
void showMenu();
void handleSerial();
void handleKeyboard(char c);
void processCommand(char* cmd);
void clearAllROM();
void saveRecording();

// --- Setup & Main Loop ---

void setup() {
  initHardware();
  initROM();
  Serial.begin(115200);
  setupHardwareUart();
  delay(1000);
  showMenu();
}

void loop() {
  handleSerial();
  
  if (!(GPIOC->INDR & (1 << 5))) {
    delay(50); 
    if (!(GPIOC->INDR & (1 << 5))) {
      playFromROM();
      while (!(GPIOC->INDR & (1 << 5))); 
    }
  }
}

// --- Hardware Initialization ---

void initHardware() {
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
  RCC->APB1PCENR |= RCC_APB1Periph_TIM2;
  
  GPIOC->CFGLR &= ~(0xF << (0 * 4));
  GPIOC->CFGLR |= (0xB << (0 * 4)); 
  
  GPIOC->CFGLR &= ~(0xF << (5 * 4));
  GPIOC->CFGLR |= (0x8 << (5 * 4)); 
  GPIOC->OUTDR |= (1 << 5);

  TIM2->PSC = 48 - 1; 
  TIM2->CTLR1 = TIM_ARPE;
  TIM2->CHCTLR2 &= ~TIM_OC3M; 
  TIM2->CHCTLR2 |= TIM_OC3M_1 | TIM_OC3M_2;
  TIM2->CCER |= TIM_CC3E; 
  TIM2->CTLR1 |= TIM_CEN;
}

void setupHardwareUart() {
  RCC->APB2PCENR |= (RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1);
  GPIOD->CFGLR &= ~((0xF << (5 * 4)) | (0xF << (6 * 4)));
  GPIOD->CFGLR |=  ((0xB << (5 * 4)) | (0x8 << (6 * 4)));
  GPIOD->OUTDR |= (1 << 6);
  AFIO->PCFR1 &= ~(1 << 2);
  USART1->CTLR1 &= ~USART_CTLR1_UE; 
  USART1->BRR = 0x1A1; 
  USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE | USART_CTLR1_RE;
}

bool getCharSafe(uint8_t &out) {
  uint32_t status = USART1->STATR;
  if (status & (USART_STATR_ORE | USART_STATR_FE | USART_STATR_NE)) {
    volatile uint32_t dummy = USART1->DATAR;
    return false;
  }
  if (status & USART_STATR_RXNE) {
    out = (uint8_t)(USART1->DATAR & 0xFF);
    return true;
  }
  return false;
}

// --- Audio Engine (NEE) ---

void playNote(const Note& note, int next_freq) {
  if (note.freq == 0) {
    setDuty(0);
    delay(note.duration);
    last_freq = -1;
    return;
  }
  
  setFrequency(note.freq);
  setDuty(15); 
  delay(25);
  
  uint8_t atk = note.attack > 0 ? note.attack : 48;
  uint8_t dec = note.decay_div > 0 ? note.decay_div : 3;
  
  current_duty = atk; 
  global_decay_step = 0;
  
  int target_min = (note.freq == next_freq && note.freq != -1) ? 15 : 5;
  uint32_t step_us = (note.duration > 25) ? ((uint32_t)(note.duration - 25) * 1000 / 100) : 0;
  
  for (int i = 0; i < 100; i++) {
    if (USART1->STATR & USART_STATR_RXNE) {
      return; 
    }

    setDuty(current_duty);
    
    if (current_duty > target_min) {
      if (current_duty > 30) {
        current_duty--;
      } else {
        global_decay_step++;
        if (global_decay_step % dec == 0) {
          current_duty--;
        }
      }
    }

    if (step_us > 0) {
      delay(step_us / 1000);
      delayMicroseconds(step_us % 1000);
    }
  }
  last_freq = note.freq;
}

void setFrequency(uint16_t freq) {
  if (freq == 0) return;
  TIM2->ATRLR = 1000000 / freq;
  TIM2->CNT = 0;
}

void setDuty(uint16_t duty) {
  if (duty == 0) {
    TIM2->CH3CVR = 0;
  } else {
    TIM2->CH3CVR = (uint32_t)TIM2->ATRLR * duty / 100;
  }
}

// --- Keyboard & Recording Logic ---

void handleKeyboard(char c) {
  uint16_t freq = 65535; 
  
  if (c == 'r' || c == 'R') {
    if (!isRecording) {
      isRecording = true;
      recNoteCount = 0;
      lastEventTime = millis();
      Serial.print("\n[REC START] ");
    } else {
      saveRecording();
      isRecording = false;
    }
    return;
  }

  if (c == 'q' || c == 'Q' || c == 0x1B) {
    if (isRecording) {
      saveRecording();
      isRecording = false;
    }
    setDuty(0);
    isKeyboardMode = false;
    Serial.println("\nKeyboard Mode Exit.");
    showMenu();
    return;
  }

  switch(c) {
    case 'a': freq = 523; break;  case 'A': freq = 1046; break;
    case 'w': freq = 554; break;  case 'W': freq = 1109; break;
    case 's': freq = 587; break;  case 'S': freq = 1175; break;
    case 'e': freq = 622; break;  case 'E': freq = 1245; break;
    case 'd': freq = 659; break;  case 'D': freq = 1319; break;
    case 'f': freq = 698; break;  case 'F': freq = 1397; break;
    case 't': freq = 740; break;  case 'T': freq = 1480; break;
    case 'g': freq = 784; break;  case 'G': freq = 1568; break;
    case 'y': freq = 831; break;  case 'Y': freq = 1661; break;
    case 'h': freq = 880; break;  case 'H': freq = 1760; break;
    case 'u': freq = 932; break;  case 'U': freq = 1865; break;
    case 'j': freq = 988; break;  case 'J': freq = 1976; break;
    case 'k': freq = 1046; break; case 'K': freq = 2093; break;
    case ' ': freq = 0;   break;
    default: return; 
  }

  if (freq != 65535) {
    uint32_t now = millis();
    if (isRecording && recNoteCount < MAX_REC_NOTES) {
      uint32_t duration = now - lastEventTime;
      if (recNoteCount > 0) {
        recBuf[(recNoteCount - 1) * 3 + 1] = (duration > 65000) ? 65000 : (uint16_t)duration;
      }
      recBuf[recNoteCount * 3] = freq;
      recBuf[recNoteCount * 3 + 1] = 500; 
      recBuf[recNoteCount * 3 + 2] = (48 << 8) | 3; 
      recNoteCount++;
    }
    
    lastEventTime = now;
    
    if (freq > 0) {
      Note liveNote = { freq, 1000, 48, 3 };
      playNote(liveNote, -1);
      setDuty(0);
      if (!isSilentMode) {
        Serial.print("\rKey: "); Serial.print(c); Serial.print(" Piano: "); Serial.print(freq); 
        if(isRecording) Serial.print(" [REC:"); Serial.print(recNoteCount); Serial.print("]");
      }
    } else {
      setDuty(0);
      if (!isSilentMode) {
        Serial.print("\rMute        ");
      }
    }
  }
}

void saveRecording() {
  if (recNoteCount > 0) {
    uint32_t duration = millis() - lastEventTime;
    recBuf[(recNoteCount - 1) * 3 + 1] = (duration > 65000) ? 65000 : (uint16_t)duration;
  }
  
  Serial.println("\n[SYSTEM] Writing to 93C66...");
  rom_write_word(0x00, 0x55AA);
  rom_write_word(0x01, recNoteCount);
  for (int i = 0; i < recNoteCount; i++) {
    uint8_t addr = 0x02 + (i * 3);
    rom_write_word(addr,     recBuf[i * 3]);
    rom_write_word(addr + 1, recBuf[i * 3 + 1]);
    rom_write_word(addr + 2, recBuf[i * 3 + 2]);
    if (i % 5 == 0) Serial.print(".");
  }
  Serial.println(" DONE!");
}

// --- ROM Operations (93C66) ---

void initROM() {
  GPIOC->CFGLR &= ~((0xF << (2*4)) | (0xF << (3*4)) | (0xF << (4*4)) | (0xF << (7*4)));
  GPIOC->CFGLR |=  ((0x3 << (2*4)) | (0x3 << (3*4)) | (0x3 << (4*4)) | (0x4 << (7*4)));
  ROM_CS_LOW(); 
  ROM_SK_LOW();
}

void rom_send_bits(uint16_t data, uint8_t len) {
  for (int i = len - 1; i >= 0; i--) {
    if ((data >> i) & 1) { ROM_DI_HIGH(); } else { ROM_DI_LOW(); }
    ROM_SK_HIGH(); delayMicroseconds(1); ROM_SK_LOW(); delayMicroseconds(1);
  }
}

uint16_t rom_read_word(uint8_t address) {
  uint16_t data = 0;
  ROM_CS_HIGH(); 
  rom_send_bits(0x06, 3); 
  rom_send_bits(address, 8);
  for (int i = 0; i < 16; i++) {
    ROM_SK_HIGH(); delayMicroseconds(1);
    data = (data << 1) | (ROM_DO_READ() ? 1 : 0);
    ROM_SK_LOW(); delayMicroseconds(1);
  }
  ROM_CS_LOW(); 
  return data;
}

void rom_write_word(uint8_t address, uint16_t data) {
  ROM_CS_HIGH(); rom_send_bits(0x04, 3); rom_send_bits(0xC0, 8); ROM_CS_LOW(); delayMicroseconds(2);
  ROM_CS_HIGH(); rom_send_bits(0x05, 3); rom_send_bits(address, 8); rom_send_bits(data, 16); ROM_CS_LOW();
  delay(15);
  ROM_CS_HIGH(); rom_send_bits(0x04, 3); rom_send_bits(0x00, 8); ROM_CS_LOW();
}

// --- Serial Interface & Menu ---

void showMenu() {
  Serial.println("\n--- UIAPduino ROM Manager V3.09 ---");
  Serial.print("Mode: "); 
  Serial.println(isSilentMode ? "SILENT" : "NORMAL");
  Serial.println("[R]          : List All Data");
  Serial.println("[R addr]     : Read Word from Address");
  Serial.println("[W addr val] : Write Word to Address");
  Serial.println("[W]          : Stream Batch Mode");
  Serial.println("[P]          : Play from ROM");
  Serial.println("[T addr]     : Test Note at Address");
  Serial.println("[K]          : Keyboard & Recorder");
  Serial.println("[M]          : Toggle Display Mode");
  Serial.println("[C]          : Format ROM (Clear)");
  Serial.print("> ");
}

void handleSerial() {
  uint8_t c;
  static uint8_t idx = 0;
  if (getCharSafe(c)) {
    if (isKeyboardMode) { handleKeyboard((char)c); return; }
    Serial.print((char)c);
    if (c == '\r' || c == '\n') {
      if (idx > 0) {
        sharedBuf[idx] = '\0';
        Serial.println();
        processCommand(sharedBuf);
        idx = 0;
        if (!isBatchMode && !isKeyboardMode) Serial.print("\n> ");
      }
    } else if (idx < 63) { sharedBuf[idx++] = (char)c; }
  }
}

void processCommand(char* cmd) {
  if (isBatchMode) {
    if (strcmp(cmd, "END") == 0) { isBatchMode = false; showMenu(); return; }
    char* ptr = strtok(cmd, ", ");
    while(ptr != NULL) { rom_write_word(batchAddr++, atoi(ptr)); ptr = strtok(NULL, ", "); }
    return;
  }

  char action = toupper(cmd[0]);
  
  if (action == 'K') {
    isKeyboardMode = true;
    Serial.println("\n--- Keyboard & Recorder (Fast Response) ---");
    Serial.println("Lower: a-k | Higher: Shift+A-K | Rec: R | Exit: Q");
  }
  else if (action == 'R') {
    char* arg = strchr(cmd, ' ');
    if (arg != NULL) {
      uint8_t address = atoi(arg + 1);
      Serial.print("Address["); Serial.print(address); Serial.print("]: "); 
      Serial.println(rom_read_word(address));
    } else {
      Serial.println("--- Full ROM Dump ---");
      for(int i=0; i<256; i++) {
        Serial.print(rom_read_word(i)); Serial.print(" ");
        if((i + 1) % 16 == 0) Serial.println();
      }
    }
  } 
  else if (action == 'W') {
    char* s1 = strchr(cmd, ' ');
    if (s1 != NULL) {
      char* s2 = strchr(s1 + 1, ' ');
      if (s2 != NULL) {
        uint8_t addr = atoi(s1 + 1);
        uint16_t val = atoi(s2 + 1);
        rom_write_word(addr, val);
        Serial.print("Written "); Serial.print(val); Serial.print(" to "); Serial.println(addr);
      } else { goto batch_init; }
    } else {
    batch_init:
      isBatchMode = true;
      batchAddr = 0;
      Serial.println("\n--- Stream Batch Write Mode ---");
      Serial.println("Enter values separated by commas. Type 'END' to exit.");
    }
  } 
  else if (action == 'P') { playFromROM(); } 
  else if (action == 'T' && strlen(cmd) > 2) {
    uint8_t a = atoi(cmd + 2);
    Note n = { rom_read_word(a), rom_read_word(a+1), (uint8_t)(rom_read_word(a+2)>>8), (uint8_t)(rom_read_word(a+2)&0xFF) };
    playNote(n, -1);
    setDuty(0);
  }
  else if (action == 'M') {
    isSilentMode = !isSilentMode;
    showMenu();
  }
  else if (action == 'C') { clearAllROM(); }
  else { showMenu(); }
}

void clearAllROM() {
  Serial.print("Formatting ROM...");
  for(int i=0; i<256; i++) { rom_write_word(i, 0xFFFF); if(i % 32 == 0) Serial.print("."); }
  Serial.println(" Done.");
}

void playFromROM() {
  uint16_t sig = rom_read_word(0x00);
  if (sig != 0x55AA) { Serial.println("No data."); return; }
  uint16_t count = rom_read_word(0x01);
  if (!isSilentMode) {
    Serial.print("Playing "); Serial.print(count); Serial.println(" notes...");
  }
  for (uint16_t i = 0; i < count; i++) {
    uint8_t b = 0x02 + (i * 3);
    uint16_t f = rom_read_word(b);
    Note n = { f, rom_read_word(b+1), (uint8_t)(rom_read_word(b+2)>>8), (uint8_t)(rom_read_word(b+2)&0xFF) };
    if (!isSilentMode) {
      Serial.print("Note["); Serial.print(i); Serial.print("] Addr:"); Serial.print(b); 
      Serial.print(" "); Serial.print(f); Serial.print("Hz (Dur:"); Serial.print(n.duration);
      Serial.print(" Atk:"); Serial.print(n.attack);
      Serial.print(" Dec:"); Serial.print(n.decay_div); Serial.println(")");
    }
    playNote(n, (i + 1 < count) ? rom_read_word(b + 3) : -1);
  }
  setDuty(0);
}
/*
 * CH32V006 Tiny BASIC v2.0 (World Edition - Fixed)
 * Features:
 * - GPIO Control: OUT pin, val / IN(pin)
 * - ADC Support:  ADC(pin)  [Supports pins 5,6,10,11,12,14,19,20]
 * - PWM Support:  PWM pin, duty [Supports pins 1,10,11,20]
 * - System:       PRINT, GOTO, IF, WAIT, CLS, LIST, RUN, NEW
 * - Engine:       Integer Math, 48MHz Operation
 * * Pinout (TSSOP20):
 * - TX: Pin 2 (PD5) / RX: Pin 3 (PD6)
 * - Reserved: Pin 4 (RST), Pin 7 (GND), Pin 9 (VDD), Pin 18 (SWIO)
 */

#define USE_STDPERIPH_DRIVER
#include "debug.h"
#include "ch32v00x.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

#define PROG_SIZE 4096
#define RX_BUF_SIZE 64
#define VAR_COUNT 26

unsigned char program[PROG_SIZE];
int variables[VAR_COUNT];
unsigned char *txtpos;
jmp_buf error_jmp;

// --- Hardware Abstraction ---

// GPIO Helper
void Pin_Set(int pin, int val) {
    GPIO_TypeDef* port = GPIOD;
    uint16_t pin_def = 0;
    
    // Simple Mapping for CH32V006 TSSOP20
    switch(pin) {
        case 1: port=GPIOD; pin_def=GPIO_Pin_4; break;
        case 5: port=GPIOA; pin_def=GPIO_Pin_1; break;
        case 6: port=GPIOA; pin_def=GPIO_Pin_2; break;
        case 8: port=GPIOD; pin_def=GPIO_Pin_0; break;
        case 10: port=GPIOC; pin_def=GPIO_Pin_0; break;
        case 11: port=GPIOC; pin_def=GPIO_Pin_1; break;
        case 12: port=GPIOC; pin_def=GPIO_Pin_2; break;
        case 13: port=GPIOC; pin_def=GPIO_Pin_3; break;
        case 14: port=GPIOC; pin_def=GPIO_Pin_4; break;
        case 15: port=GPIOC; pin_def=GPIO_Pin_5; break;
        case 16: port=GPIOC; pin_def=GPIO_Pin_6; break;
        case 17: port=GPIOC; pin_def=GPIO_Pin_7; break;
        case 19: port=GPIOD; pin_def=GPIO_Pin_2; break;
        case 20: port=GPIOD; pin_def=GPIO_Pin_3; break;
        default: return; // Invalid or Reserved Pin
    }

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = pin_def;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // Set as Output
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(port, &GPIO_InitStructure);
    GPIO_WriteBit(port, pin_def, val ? Bit_SET : Bit_RESET);
}

int Pin_Read(int pin) {
    GPIO_TypeDef* port = GPIOD;
    uint16_t pin_def = 0;
    
    switch(pin) {
        case 1: port=GPIOD; pin_def=GPIO_Pin_4; break;
        case 5: port=GPIOA; pin_def=GPIO_Pin_1; break;
        case 6: port=GPIOA; pin_def=GPIO_Pin_2; break;
        case 8: port=GPIOD; pin_def=GPIO_Pin_0; break;
        case 10: port=GPIOC; pin_def=GPIO_Pin_0; break;
        case 11: port=GPIOC; pin_def=GPIO_Pin_1; break;
        case 12: port=GPIOC; pin_def=GPIO_Pin_2; break;
        case 13: port=GPIOC; pin_def=GPIO_Pin_3; break;
        case 14: port=GPIOC; pin_def=GPIO_Pin_4; break;
        case 15: port=GPIOC; pin_def=GPIO_Pin_5; break;
        case 16: port=GPIOC; pin_def=GPIO_Pin_6; break;
        case 17: port=GPIOC; pin_def=GPIO_Pin_7; break;
        case 19: port=GPIOD; pin_def=GPIO_Pin_2; break;
        case 20: port=GPIOD; pin_def=GPIO_Pin_3; break;
        default: return 0;
    }
    
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = pin_def;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // Set as Input Pull-Up
    GPIO_Init(port, &GPIO_InitStructure);
    return GPIO_ReadInputDataBit(port, pin_def);
}

// PWM Helper (Simple Setup)
void PWM_Set(int pin, int duty) {
    // Duty: 0-255
    // Supported: Pin 1(T2C1), Pin 10(T1C3), Pin 11(T1C4), Pin 20(T2C2)
    
    if (duty < 0) duty = 0;
    if (duty > 255) duty = 255;
    
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = duty;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    if (pin == 1) { // PD4 -> TIM2 CH1
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
        GPIO_Init(GPIOD, &GPIO_InitStructure);
        TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    } else if (pin == 20) { // PD3 -> TIM2 CH2
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
        GPIO_Init(GPIOD, &GPIO_InitStructure);
        TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    } else if (pin == 10) { // PC0 -> TIM1 CH3
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
        GPIO_Init(GPIOC, &GPIO_InitStructure);
        TIM_OC3Init(TIM1, &TIM_OCInitStructure);
    } else if (pin == 11) { // PC1 -> TIM1 CH4
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
        GPIO_Init(GPIOC, &GPIO_InitStructure);
        TIM_OC4Init(TIM1, &TIM_OCInitStructure);
    }
}

int ADC_Read_Pin(int pin) {
    uint8_t ch = 0;
    // Map Pin to ADC Channel
    switch(pin) {
        case 6: ch = ADC_Channel_0; break; // PA2
        case 5: ch = ADC_Channel_1; break; // PA1
        case 14: ch = ADC_Channel_2; break; // PC4
        case 19: ch = ADC_Channel_3; break; // PD2
        case 20: ch = ADC_Channel_4; break; // PD3
        case 12: ch = ADC_Channel_6; break; // PC2
        case 1:  ch = ADC_Channel_7; break; // PD4
        case 11: ch = ADC_Channel_8; break; // PC1
        case 10: ch = ADC_Channel_9; break; // PC0
        default: return 0; // Not ADC pin
    }
    
    // Re-init GPIO as Analog
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    
    if(pin==5 || pin==6) {
        GPIO_InitStructure.GPIO_Pin = (pin==5)?GPIO_Pin_1:GPIO_Pin_2;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
    } else if(pin==1 || pin==19 || pin==20) {
        GPIO_InitStructure.GPIO_Pin = (pin==1)?GPIO_Pin_4 : (pin==19)?GPIO_Pin_2 : GPIO_Pin_3;
        GPIO_Init(GPIOD, &GPIO_InitStructure);
    } else {
        GPIO_InitStructure.GPIO_Pin = (1<<(pin-10)); // PC0..
        GPIO_Init(GPIOC, &GPIO_InitStructure);
    }

    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_CyclesMode7);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

void Hardware_Init(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    
    // Enable Clocks (APB2)
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOD | RCC_PB2Periph_GPIOA | RCC_PB2Periph_GPIOC | RCC_PB2Periph_USART1 | RCC_PB2Periph_ADC1 | RCC_PB2Periph_TIM1, ENABLE);
    
    // Enable Clocks (APB1) - ¡ïFIXED HERE¡ï
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);

    // UART Init (PD5 TX, PD6 RX)
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure = {0};
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);

    // ADC Init
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);
    ADC_InitTypeDef ADC_InitStructure = {0};
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_Cmd(ADC1, ENABLE);
    
    // Timer Init for PWM (TIM1 & TIM2)
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_TimeBaseStructure.TIM_Period = 255; // 8-bit PWM
    TIM_TimeBaseStructure.TIM_Prescaler = 48000000 / (256 * 1000) - 1; // Approx 1kHz
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_Cmd(TIM1, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE); // Main Output Enable for TIM1
}

int Serial_Available(void) {
    return (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET);
}

char Serial_ReadChar(void) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
    return (char)USART_ReceiveData(USART1);
}

// --- Interpreter Core ---
void error(char *msg) {
    printf("\r\nError: %s\r\n", msg);
    longjmp(error_jmp, 1);
}

void check_break(void) {
    if (Serial_Available()) {
        char c = (char)USART_ReceiveData(USART1);
        if (c == 3) error("Break");
    }
}

char peek(void) { return *txtpos; }
void skip(void) { if (*txtpos) txtpos++; }
void ignore_blanks(void) { while (*txtpos <= ' ' && *txtpos != 0) txtpos++; }

void match(char c) {
    ignore_blanks();
    if (*txtpos == c) skip();
    else error("SynErr");
}

int expression(void);

int number(void) {
    int val = 0;
    ignore_blanks();
    if (!isdigit(*txtpos)) error("Num?");
    while (isdigit(*txtpos)) {
        val = val * 10 + (*txtpos - '0');
        txtpos++;
    }
    return val;
}

int factor(void) {
    int val;
    ignore_blanks();
    if (*txtpos == '-') {
        skip();
        return -factor();
    }
    else if (*txtpos == '(') {
        skip();
        val = expression();
        match(')');
    } 
    else if (strncmp((char*)txtpos, "ADC", 3) == 0) {
        txtpos += 3;
        match('(');
        val = expression(); // Pin number
        match(')');
        val = ADC_Read_Pin(val);
    }
    else if (strncmp((char*)txtpos, "IN", 2) == 0) {
        txtpos += 2;
        match('(');
        val = expression(); // Pin number
        match(')');
        val = Pin_Read(val);
    }
    else if (isalpha(*txtpos)) {
        int index = toupper(*txtpos) - 'A';
        skip();
        val = variables[index];
    } 
    else {
        val = number();
    }
    return val;
}

int term(void) {
    int val = factor();
    ignore_blanks();
    while (*txtpos == '*' || *txtpos == '/') {
        if (*txtpos == '*') { skip(); val *= factor(); }
        else if (*txtpos == '/') { skip(); int d = factor(); if (d==0) error("Div0"); val /= d; }
        ignore_blanks();
    }
    return val;
}

int expression(void) {
    int val = term();
    ignore_blanks();
    while (*txtpos == '+' || *txtpos == '-') {
        if (*txtpos == '+') { skip(); val += term(); }
        else if (*txtpos == '-') { skip(); val -= term(); }
        ignore_blanks();
    }
    return val;
}

int condition(void) {
    int val = expression();
    ignore_blanks();
    if (strncmp((char*)txtpos, "<=", 2) == 0) { txtpos+=2; return val <= expression(); }
    if (strncmp((char*)txtpos, ">=", 2) == 0) { txtpos+=2; return val >= expression(); }
    if (strncmp((char*)txtpos, "<>", 2) == 0) { txtpos+=2; return val != expression(); }
    if (*txtpos == '<') { skip(); return val < expression(); }
    if (*txtpos == '>') { skip(); return val > expression(); }
    if (*txtpos == '=') { skip(); return val == expression(); }
    return val;
}

unsigned char *find_line(int line_num) {
    unsigned char *p = program;
    while (*p) {
        int current_line = p[0] | (p[1] << 8);
        if (current_line == line_num) return p;
        if (current_line > line_num) return NULL;
        p += 2;
        while (*p++);
    }
    return NULL;
}

void basic_line_insert(int line_num, char *line_str) {
    unsigned char *p = program;
    unsigned char *next_p;
    while (*p) {
        int current_line = p[0] | (p[1] << 8);
        if (current_line >= line_num) break;
        p += 2;
        while (*p++);
    }
    if (*p) {
        int current_line = p[0] | (p[1] << 8);
        if (current_line == line_num) {
            next_p = p + 2;
            while (*next_p++);
            int len_rest = PROG_SIZE - (next_p - program);
            memmove(p, next_p, len_rest);
        }
    }
    int len = strlen(line_str);
    if (len > 0) {
        unsigned char *end = program;
        while(*end) { end += 2; while(*end++); }
        if ((end - program) + len + 3 >= PROG_SIZE) { printf("Full\r\n"); return; }
        memmove(p + len + 3, p, (end - p) + 1);
        p[0] = line_num & 0xFF;
        p[1] = (line_num >> 8) & 0xFF;
        strcpy((char*)p + 2, line_str);
    }
}

void execute_statement(void) {
    ignore_blanks();
    if (strncmp((char*)txtpos, "PRINT", 5) == 0) {
        txtpos += 5;
        int newline = 1;
        while(1) {
            ignore_blanks();
            if (*txtpos == '"') {
                skip();
                while (*txtpos != '"' && *txtpos) printf("%c", *txtpos++);
                if (*txtpos == '"') skip();
                newline = 1;
            } else if (*txtpos == ';' || *txtpos == ',') {
                skip(); 
                newline = 0; 
                continue; 
            } else if (*txtpos == 0 || *txtpos == ':') {
                break;
            } else {
                printf("%d", expression());
                newline = 1;
            }
        }
        if (newline) printf("\r\n");
    }
    else if (strncmp((char*)txtpos, "OUT", 3) == 0) {
        txtpos += 3;
        int pin = expression();
        ignore_blanks();
        if (*txtpos == ',') skip();
        int val = expression();
        Pin_Set(pin, val);
    }
    else if (strncmp((char*)txtpos, "PWM", 3) == 0) {
        txtpos += 3;
        int pin = expression();
        ignore_blanks();
        if (*txtpos == ',') skip();
        int val = expression();
        PWM_Set(pin, val);
    }
    else if (strncmp((char*)txtpos, "WAIT", 4) == 0) {
        txtpos += 4;
        int ms = expression();
        for(int i=0; i<ms; i++) {
            Delay_Ms(1);
            check_break();
        }
    }
    else if (strncmp((char*)txtpos, "CLS", 3) == 0) {
        txtpos += 3;
        printf("\x1b[2J\x1b[H");
    }
    else if (strncmp((char*)txtpos, "GOTO", 4) == 0) {
        txtpos += 4;
        int line_num = expression();
        unsigned char *p = find_line(line_num);
        if (p) { txtpos = p + 2; return; }
        else error("Line?");
    }
    else if (strncmp((char*)txtpos, "IF", 2) == 0) {
        txtpos += 2;
        int cond = condition();
        ignore_blanks();
        if (strncmp((char*)txtpos, "THEN", 4) == 0) txtpos += 4;
        if (cond) execute_statement();
        else while (*txtpos) txtpos++;
    }
    else if (strncmp((char*)txtpos, "LIST", 4) == 0) {
        unsigned char *p = program;
        while (*p) {
            printf("%d %s\r\n", p[0] | (p[1] << 8), p + 2);
            p += 2; while (*p++);
            check_break();
        }
    }
    else if (strncmp((char*)txtpos, "RUN", 3) == 0) { }
    else if (strncmp((char*)txtpos, "NEW", 3) == 0) {
        memset(program, 0, PROG_SIZE);
        printf("OK\r\n");
    }
    else if (isalpha(*txtpos)) {
        int index = toupper(*txtpos) - 'A';
        txtpos++;
        ignore_blanks();
        if (*txtpos == '=') { skip(); variables[index] = expression(); }
        else error("SynErr");
    }
}

void run_program(void) {
    unsigned char *p = program;
    while (*p) {
        unsigned char *next_line = p + 2;
        while (*next_line++);
        txtpos = p + 2;
        if (setjmp(error_jmp) == 0) execute_statement();
        else return;
        check_break();
        if (txtpos >= program && txtpos < program + PROG_SIZE) {
             if (*txtpos == 0 && next_line > p) p = next_line;
             else p = txtpos - 2;
        } else break;
    }
}

int main(void)
{
    char input_buf[RX_BUF_SIZE];
    int buf_idx = 0;

    Hardware_Init();
    printf("\r\nCH32V006 Tiny BASIC v2.0 (World Edition)\r\n> ");

    while(1)
    {
        char c = Serial_ReadChar();
        if (c >= ' ' && c <= '~') printf("%c", c);
        
        if (c == '\r') {
            printf("\r\n");
            input_buf[buf_idx] = 0;
            if (buf_idx > 0) {
                char *ptr = input_buf;
                int line_num = 0;
                if (isdigit(*ptr)) {
                    while(isdigit(*ptr)) { line_num = line_num * 10 + (*ptr - '0'); ptr++; }
                    ignore_blanks();
                    basic_line_insert(line_num, ptr + (*ptr == ' ' ? 1 : 0));
                } else {
                    for(int i=0; i<strlen(input_buf); i++) input_buf[i] = toupper(input_buf[i]);
                    if (strcmp(input_buf, "RUN") == 0) run_program();
                    else if (strcmp(input_buf, "LIST") == 0) { txtpos=(unsigned char*)"LIST"; execute_statement(); }
                    else if (strcmp(input_buf, "NEW") == 0) { memset(program,0,PROG_SIZE); printf("OK\r\n"); }
                    else if (strcmp(input_buf, "CLS") == 0) { printf("\x1b[2J\x1b[H"); }
                    else { txtpos = (unsigned char*)input_buf; if (setjmp(error_jmp) == 0) { execute_statement(); printf("OK\r\n"); } }
                }
            }
            buf_idx = 0;
            printf("> ");
        } 
        else if (c == 8 || c == 127) {
            if (buf_idx > 0) { buf_idx--; printf("\b \b"); }
        }
        else if (buf_idx < RX_BUF_SIZE - 1 && c >= ' ') {
            input_buf[buf_idx++] = c;
        }
        else if (c == 3) {
            printf("^C\r\n> ");
            buf_idx = 0;
        }
    }
}
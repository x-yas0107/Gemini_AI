/*********************************************************************
 * File Name    : main.c
 * Project      : CH32V006 Curve Tracer
 * Version      : 0.18
 * Description  : カーブトレーサー (高周波PWM ＋ 空読み1回 ＋ 可変平均化)
 *********************************************************************/

#include "debug.h"

#define SHUNT_RESISTOR_OHM  100
#define VDD_MV              5000
#define ADC_RESOLUTION      4095
#define MAX_DATA_POINTS     510

/* ▼ ここで平均化の回数を自由に変更できます（おすすめは 4 または 16） ▼ */
#define NUM_SAMPLES         16
/* ▲ ========================================================== ▲ */

typedef struct {
    uint16_t v_ce_mv;
    int32_t  i_c_ua;
    uint8_t  ib_pwm;
    uint16_t v_dd_mv;
} TraceData;

TraceData curve_buffer[MAX_DATA_POINTS];
uint16_t data_count = 0;

void UART_RX_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOD | RCC_PB2Periph_USART1, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    USART1->CTLR1 |= USART_Mode_Rx;
}

uint8_t Read_UART_Cmd(void)
{
    if(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
    {
        return (uint8_t)USART_ReceiveData(USART1);
    }
    return 0;
}

void Show_Menu(void)
{
    printf("\r\n=== CH32V006 Curve Tracer ===\r\n");
    printf("[s] : Start Analysis\r\n");
    printf("[r] : Read Data\r\n");
    printf("=============================\r\n");
}

void ADC_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_GPIOC, ENABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
}

/* --- ADC取得関数（空読み1回＋本番1回） --- */
uint16_t Get_ADC_Val(uint8_t ch)
{
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_CyclesMode7);
    
    /* 1回目：空読み（前のチャンネルの残像を捨てる） */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    
    /* 2回目：本番の読み込み */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    uint16_t val = ADC_GetConversionValue(ADC1);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    
    return val;
}

void PWM_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOD | RCC_PB2Periph_TIM1, ENABLE);
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Period = 255; 
    TIM_TimeBaseStructure.TIM_Prescaler = 0; 
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_Cmd(TIM2, ENABLE);
}

void Set_Vce_PWM(uint8_t duty)
{
    TIM_SetCompare1(TIM1, duty);
}

void Set_Ib_PWM(uint8_t duty)
{
    TIM_SetCompare2(TIM2, duty);
}

int main(void)
{
    uint16_t ib_step_val, vce_pwm_val;
    uint32_t sum_vsweep, sum_vce, sum_vdd;
    uint16_t adc_vsweep_raw, adc_vce_raw, adc_vdd_raw;
    int v_sweep_mv, v_ce_mv, i_c_ua, v_dd_mv;
    uint8_t cmd, i;

    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    UART_RX_Init();

    ADC_Config();
    PWM_Config();

    Set_Vce_PWM(0);
    Set_Ib_PWM(0);
    Delay_Ms(100);

    Show_Menu();

    while(1)
    {
        cmd = Read_UART_Cmd();

        if(cmd == 's')
        {
            printf("\r\nAnalyzing...\r\n");
            data_count = 0;

            for(ib_step_val = 25; ib_step_val <= 250; ib_step_val += 25)
            {
                Set_Ib_PWM(ib_step_val); 
                Delay_Ms(50); 

                for(vce_pwm_val = 0; vce_pwm_val <= 250; vce_pwm_val += 5)
                {
                    Set_Vce_PWM(vce_pwm_val);
                    Delay_Ms(15); 

                    /* 設定した回数分サンプリングして平均化 */
                    sum_vsweep = 0;
                    sum_vce = 0;
                    sum_vdd = 0;

                    for(i = 0; i < NUM_SAMPLES; i++)
                    {
                        sum_vsweep += Get_ADC_Val(ADC_Channel_1);
                        sum_vce    += Get_ADC_Val(ADC_Channel_0);
                        sum_vdd    += Get_ADC_Val(ADC_Channel_2);
                    }

                    adc_vsweep_raw = sum_vsweep / NUM_SAMPLES;
                    adc_vce_raw    = sum_vce / NUM_SAMPLES;
                    adc_vdd_raw    = sum_vdd / NUM_SAMPLES;

                    /* 電圧計算 */
                    v_sweep_mv = (adc_vsweep_raw * VDD_MV) / ADC_RESOLUTION;
                    v_ce_mv    = (adc_vce_raw * VDD_MV) / ADC_RESOLUTION;
                    v_dd_mv    = (adc_vdd_raw * VDD_MV) / ADC_RESOLUTION;
                    
                    /* 電流計算 */
                    i_c_ua = ((v_sweep_mv - v_ce_mv) * 1000) / SHUNT_RESISTOR_OHM;

                    if(data_count < MAX_DATA_POINTS)
                    {
                        curve_buffer[data_count].v_ce_mv = v_ce_mv;
                        curve_buffer[data_count].i_c_ua = i_c_ua;
                        curve_buffer[data_count].ib_pwm = ib_step_val;
                        curve_buffer[data_count].v_dd_mv = v_dd_mv;
                        data_count++;
                    }
                }
            }

            Set_Vce_PWM(0);
            Set_Ib_PWM(0);
            printf("Analysis Complete\r\n");
            Show_Menu();
        }
        else if(cmd == 'r')
        {
            printf("\r\n");
            if(data_count == 0)
            {
                printf("No Data\r\n");
            }
            else
            {
                uint16_t j;
                for(j = 0; j < data_count; j++)
                {
                    printf("%d, %ld, %d, %d\r\n", 
                           curve_buffer[j].v_ce_mv, 
                           curve_buffer[j].i_c_ua, 
                           curve_buffer[j].ib_pwm,
                           curve_buffer[j].v_dd_mv);
                }
                printf("Transfer Complete\r\n");
            }
            Show_Menu();
        }
    }
}
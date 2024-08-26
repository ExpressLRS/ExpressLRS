#include "PWM.h"

#if defined(M0139) && defined(TARGET_RX)
#include <Arduino.h>
#include "logging.h"
#include "variant_M0139.h"
#include "stm32f1xx_hal.h"
extern bool servoInitialized;

/* Private variables */
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

/* Private function prototypes */
static void GPIO_Init(void);
static void TIM1_Init(void);
static void TIM3_Init(void);

#define MAX_PWM_CHANNELS    4

static uint16_t refreshInterval[MAX_PWM_CHANNELS] = {0};
static int8_t pwm_gpio[MAX_PWM_CHANNELS] = {-1};
#define PIN_DISCONNECTED -1

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
static void PWM_Error_Handler()
{
  /* User can add his own implementation to report the HAL error return state */
  DBGLN("PWM ERROR!");
  __disable_irq();
//   while (1)
//   {
//     /*Write both GREEN and RED LED HIGH to indicate error*/
//     digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
//     digitalWrite(GPIO_PIN_LED, HIGH);
//   }
}

pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency)
{
    for(int channel=0 ; channel<MAX_PWM_CHANNELS ; channel++)
    {
        if (refreshInterval[channel] == 0)
        {
            pwm_gpio[channel] = pin;
            refreshInterval[channel] = 1000000U / frequency;
            switch(pin){
                case Ch1:
                    DBGLN("Starting PWM on: %u", pin);
                    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
                    __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, 0);  // ALL LEDs OFF
                    break;
                
                case Ch2:
                    DBGLN("Starting PWM on: %u", pin);
                    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
                    __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, 0);  // ALL LEDs OFF
                    break;

                case Ch3:
                    DBGLN("Starting PWM on: %u", pin);
                    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
                    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, 0);  // ALL LEDs OFF
                    break;
                
                case Ch4:
                    DBGLN("Starting PWM on: %u", pin);
                    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
                    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, 0);  // ALL LEDs OFF
                    break;

                default:
                    DBGLN("Trying to start Undefined pin: %u", pin);
                    break;
            }

            return channel;
        }
    }
    return -1;
}

void PWMController::release(pwm_channel_t channel)
{
    int8_t pin = pwm_gpio[channel];
    if(pin == -1)
    {
        return;
    }
    switch(pin){
        case Ch1:
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, 0);
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
            digitalWrite(pin, LOW);
            break;
        
        case Ch2:
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, 0);
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_4);
            digitalWrite(pin, LOW);
            break;

        case Ch3:
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, 0);
            HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);     
            break;

        case Ch4:
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, 0);
            HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);    
            break;

        default:
            break;
    }

    refreshInterval[channel] = 0;
    pwm_gpio[channel] = -1;
}

void PWMController::setDuty(pwm_channel_t channel, uint16_t duty)
{
    const uint8_t pin = pwm_gpio[channel];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    uint16_t mappedValue = 0;
    switch(pin){
        case Ch1:
            mappedValue = map(duty, 0, 100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, mappedValue);
            break;
        
        case Ch2:
            mappedValue = map(duty, 0, 100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, mappedValue);
            break;

        case Ch3:
            mappedValue = map(duty, 0, 100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, mappedValue);
            break;

        case Ch4:
            mappedValue = map(duty, 0, 100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, mappedValue);
            break;

        default:
            break;
    }

}

void PWMController::setMicroseconds(pwm_channel_t channel, uint16_t microseconds)
{
    const uint8_t pin = pwm_gpio[channel];
    if (pin == PIN_DISCONNECTED)
    {
        return;
    }
    uint16_t mappedValue = 0;
    switch(pin){
        case Ch1:
            mappedValue = map(microseconds, 0, 2100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_3, mappedValue);
            break;
        
        case Ch2:
            mappedValue = map(microseconds, 0, 2100, 0, htim3.Init.Period);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, mappedValue);
            break;

        case Ch3:
            mappedValue = map(microseconds, 0, 2100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, mappedValue);
            break;
        
        case Ch4:
            mappedValue = map(microseconds, 0, 2100, 0, htim1.Init.Period);
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, mappedValue);
            break;

        default:
            break;
    }

}

void PWMController::initialize()
{
    DBGLN("Initializing PWMs BEGIN");
    GPIO_Init();
    TIM1_Init();
    TIM3_Init();
    DBGLN("Initializing PWMs DONE");   
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim->Instance==TIM1)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PA8     ------> TIM1_CH1
    PA11    ------> TIM1_CH4
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
  else if(htim->Instance==TIM3)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**TIM3 GPIO Configuration
    PB0     ------> TIM3_CH3
    PB1     ------> TIM3_CH4
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

/**
  * @brief TIM1 Initialization Function.
  *         ARR = 2068 (Max 2100 us period, 32 clock cycle delay so we use 2068)
  *         900  us HIGH -> 886  us (ARR * DUTY/100)... DUTY is ~42.84% (900/2100)
  *         1400 us HIGH -> 1379 us (ARR * DUTY/100)... DUTY is ~66.66% (1400/2100)
  *         2100 us HIGH -> 2068 us (ARR * DUTY/100)... DUTY is 100% (2100/2100)
  *
  * @param None
  * @retval None
  */
static void TIM1_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 72;        // 72MHz APB2 CLK -> 1MHz APB2 CLK
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2068;         // 2100 us at 1 MHz 
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function. 
  *         ARR = 2068 (Max 2100 us period, some delay so we use 2068)
  *         900  us HIGH -> 886  us (ARR * DUTY/100)... DUTY is ~42.84% (900/2100)
  *         1400 us HIGH -> 1379 us (ARR * DUTY/100)... DUTY is ~66.66% (1400/2100)
  *         2100 us HIGH -> 2068 us (ARR * DUTY/100)... DUTY is 100% (2100/2100)
  * 
  * @param None
  * @retval None
  */
static void TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 72;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 2068;         // 2100 us at 1 MHz 
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    PWM_Error_Handler();
  }
if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    PWM_Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim3);

}

#endif
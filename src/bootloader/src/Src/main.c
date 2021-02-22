/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>

/* Private includes ----------------------------------------------------------*/
#include "main.h"
#include "led.h"
#include "uart.h"
#include "flash.h"
#include "xmodem.h"
#include "stk500.h"
#include "frsky.h"
#if !XMODEM && !STK500 && !FRSKY
#error "Upload protocol not defined!"
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

#if defined(PIN_BUTTON)
struct gpio_pin gpio_btn;
#endif
#if defined(PIN_LED_RED)
struct gpio_pin gpio_led_red;
void *led_red_port;
uint32_t led_red_pin;
#endif
#if defined(PIN_LED_GREEN)
struct gpio_pin gpio_led_green;
void *led_green_port;
uint32_t led_green_pin;
#endif
#if defined(DUPLEX_PIN)
static int32_t duplex_pin = IO_CREATE(DUPLEX_PIN);
#else
static int32_t duplex_pin = -1;
#endif // DUPLEX_PIN

#ifndef BUTTON_INVERTED
#define BUTTON_INVERTED   1
#endif // BUTTON_INVERTED
#define BTN_READ() (GPIO_Read(gpio_btn) ^ BUTTON_INVERTED)

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* Private user code ---------------------------------------------------------*/

void gpio_port_pin_get(uint32_t io, void ** port, uint32_t * pin)
{
  *pin = IO_GET_PIN(io);
  io = IO_GET_PORT(io);
  *port = (void*)((uintptr_t)GPIOA_BASE + (io * 0x0400UL));
}

void gpio_port_clock(uint32_t port)
{
  // Enable gpio clock
  switch (port) {
    case GPIOA_BASE:
      __HAL_RCC_GPIOA_CLK_ENABLE();
      break;
    case GPIOB_BASE:
      __HAL_RCC_GPIOB_CLK_ENABLE();
      break;
#ifdef __HAL_RCC_GPIOC_CLK_ENABLE
    case GPIOC_BASE:
      __HAL_RCC_GPIOC_CLK_ENABLE();
      break;
#endif
#ifdef __HAL_RCC_GPIOD_CLK_ENABLE
    case GPIOD_BASE:
      __HAL_RCC_GPIOD_CLK_ENABLE();
      break;
#endif
#ifdef __HAL_RCC_GPIOE_CLK_ENABLE
    case GPIOE_BASE:
      __HAL_RCC_GPIOE_CLK_ENABLE();
      break;
#endif
#ifdef __HAL_RCC_GPIOF_CLK_ENABLE
    case GPIOF_BASE:
      __HAL_RCC_GPIOF_CLK_ENABLE();
      break;
#endif
    default:
      break;
    }
}

void GPIO_SetupPin(GPIO_TypeDef *regs, uint32_t pos, uint32_t mode, int pullup)
{
  gpio_port_clock((uint32_t)regs);

#if defined(STM32F1)
  // Configure GPIO
  uint32_t shift = (pos % 8) * 4, msk = 0xf << shift, cfg;

  if (mode == GPIO_INPUT) {
    cfg = pullup ? 0x8 : 0x4;
  } else if (mode == GPIO_OUTPUT) {
    cfg = 0x1; // push-pull, 0b00 | max speed 2 MHz, 0b01
  } else if (mode == (GPIO_OUTPUT | GPIO_OPEN_DRAIN)) {
    cfg = 0x5; // Open-drain, 0b01 | max speed 2 MHz, 0b01
  } else if (mode == GPIO_ANALOG) {
    cfg = 0x0;
  } else {
    // Alternate function
    if (mode & GPIO_OPEN_DRAIN)
      // output open-drain mode, 10MHz
      cfg = 0xd;
      // output open-drain mode, 50MHz
      //cfg = 0xF;
    else if (pullup > 0)
      // input pins use GPIO_INPUT mode on the stm32f1
      cfg = 0x8;
    else
      // output push-pull mode, 10MHz
      cfg = 0x9;
      // output push-pull mode, 50MHz
      //cfg = 0xB;
  }
  if (pos & 0x8)
    regs->CRH = (regs->CRH & ~msk) | (cfg << shift);
  else
    regs->CRL = (regs->CRL & ~msk) | (cfg << shift);

  if (pullup > 0)
    regs->BSRR = 1 << pos;
  else if (pullup < 0)
    regs->BSRR = 1 << (pos + 16);
#else
  uint32_t const bit_pos = 0x1 << pos;
  if (mode == GPIO_INPUT) {
    LL_GPIO_SetPinMode(regs, bit_pos, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(regs, bit_pos, (0 < pullup ? LL_GPIO_PULL_UP : LL_GPIO_PULL_DOWN));
  } else if (mode == GPIO_OUTPUT) {
    LL_GPIO_SetPinMode(regs, bit_pos, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(regs, bit_pos, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(regs, bit_pos, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_ResetOutputPin(regs, bit_pos);
  } else {
    mode = (mode >> 4) & 0xff;
    LL_GPIO_SetPinMode(regs, bit_pos, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinPull(regs, bit_pos, (0 < pullup ? LL_GPIO_PULL_UP : LL_GPIO_PULL_DOWN));
    if (pos & 0x8) {
      LL_GPIO_SetAFPin_8_15(regs, bit_pos, mode);
    } else {
      LL_GPIO_SetAFPin_0_7(regs, bit_pos, mode);
    }
  }
#endif
}

struct gpio_pin
GPIO_Setup(uint32_t io, uint32_t mode, int pullup)
{
  uint32_t pin = IO_GET_PIN(io);
  io = IO_GET_PORT(io);
  GPIO_TypeDef *port = (void*)((uintptr_t)GPIOA_BASE + (io * 0x0400UL));

  GPIO_SetupPin(port, pin, mode, pullup);

  return (struct gpio_pin){.reg = port, .bit = (1 << pin)};
}

void led_state_set(uint32_t state)
{
  uint32_t val;
  switch (state) {
  case LED_BOOTING:
    val = 0x00ffff;
    break;
  case LED_FLASHING:
    val = 0x0000ff;
    break;
  case LED_FLASHING_ALT:
    val = 0x00ff00;
    break;
  case LED_STARTING:
    val = 0xffff00;
    break;
  case LED_OFF:
  default:
    val = 0x0;
  };

#if defined(PIN_LED_RED)
  GPIO_Write(gpio_led_red, (!!(uint8_t)val));
#endif
#if defined(PIN_LED_GREEN)
  GPIO_Write(gpio_led_green, !!(uint8_t)(val >> 8));
#endif
  ws2812_set_color_u32(val);
}

#if XMODEM

static void print_boot_header(void)
{
  /* Send welcome message on startup. */
  uart_transmit_str("\n\r========= v");
#if defined(BOOTLOADER_VERSION)
  uart_transmit_str(BUILD_VERSION(BOOTLOADER_VERSION));
#endif
  uart_transmit_str(" ============\n\r");
  uart_transmit_str("  Bootloader for ExpressLRS\n\r");
  uart_transmit_str("=============================\n\r");
#if defined(MCU_TYPE)
  uart_transmit_str(BUILD_MCU_TYPE(MCU_TYPE));
  uart_transmit_str("\n\r");
#endif
}

static int8_t boot_code_xmodem(int32_t rx_pin, int32_t tx_pin)
{
  uint8_t BLrequested = 0;
  uint8_t header[6] = {0, 0, 0, 0, 0, 0};

  uart_init(UART_BAUD, rx_pin, tx_pin, duplex_pin, UART_INV);
  flash_dump();

  print_boot_header();
  /* If the button is pressed, then jump to the user application,
   * otherwise stay in the bootloader. */
  uart_transmit_str("Send 'bbbb' or hold down button\n\r");

#if 0 // UART ECHO DEBUG
  uint8_t _led_tmp = 0;
  while(1) {
    if (uart_receive_timeout(header, 1u, 1000) == UART_OK) {
      uart_transmit_bytes(header, 1);
    } else {
      uart_transmit_ch('F');
    }
    led_state_set(_led_tmp ? LED_FLASHING : LED_FLASHING_ALT);
    _led_tmp ^= 1;
  }
#endif

  /* Wait input from UART */
  uart_clear();
  if (uart_receive(header, 5u) == UART_OK) {
    /* Search for magic strings */
    BLrequested = (strnstr((char *)header, "bbbb", sizeof(header)) != NULL) ? 1 : 0;
  }

#if defined(PIN_BUTTON)
  // Wait button press to access bootloader
  if (!BLrequested && BTN_READ()) {
    HAL_Delay(200); // wait debounce
    if (BTN_READ()) {
      // Button still pressed
      BLrequested = 2;
    }
  }
#endif /* PIN_BUTTON */

  if (!BLrequested) {
    return -1;
  }

#if 0
  /* Wait command from uploader script if button was preassed */
  else if (BLrequested == 2) {
    BLrequested = 0;
    uint32_t ticks = HAL_GetTick();
    uint8_t ledState = 0;
    while (1) {
      if (BLrequested < 6) {
        if (1000 <= (HAL_GetTick() - ticks)) {
          led_state_set(ledState ? LED_FLASHING : LED_FLASHING_ALT);
          ledState ^= 1;
          ticks = HAL_GetTick();
        }

        if (uart_receive_timeout(header, 1, 1000U) != UART_OK) {
          continue;
        }
        uint8_t ch = header[0];

        switch (BLrequested) {
          case 0:
            if (ch == 0xEC) BLrequested++;
            else BLrequested = 0;
            break;
          case 1:
            if (ch == 0x04) BLrequested++;
            else BLrequested = 0;
            break;
          case 2:
            if (ch == 0x32) BLrequested++;
            else BLrequested = 0;
            break;
          case 3:
            if (ch == 0x62) BLrequested++;
            else BLrequested = 0;
            break;
          case 4:
            if (ch == 0x6c) BLrequested++;
            else BLrequested = 0;
            break;
          case 5:
            if (ch == 0x0A) BLrequested++;
            else BLrequested = 0;
            break;

        }
      } else {
        /* Boot cmd => wait 'bbbb' */
        print_boot_header();
        if (uart_receive_timeout(header, 5, 2000U) != UART_OK) {
          BLrequested = 0;
          continue;
        }
        if (strstr((char *)header, "bbbb")) {
          /* Script ready for upload... */
          break;
        }
      }
    }
  }
#endif

  /* Infinite loop */
  while (1) {
    uart_clear();
    xmodem_receive();
    /* We only exit the xmodem protocol, if there are any errors.
     * In that case, notify the user and start over. */
    //uart_transmit_str("\n\rFailed... Please try again.\n\r");
  }

  return 0;
}
#endif /* XMODEM */


#ifndef BOOT_WAIT
#define BOOT_WAIT 300 // ms
#endif

static uint32_t boot_start_time;

int8_t boot_wait_timer_end(void)
{
  return (BOOT_WAIT < (HAL_GetTick() - boot_start_time));
}

int8_t boot_code_stk(uint32_t baud, int32_t rx_pin, int32_t tx_pin, int32_t duplexpin, uint8_t inverted)
{
  int8_t ret = 0;

  uart_init(baud, rx_pin, tx_pin, duplexpin, inverted);

  boot_start_time = HAL_GetTick();

  while (0 <= ret) {
#if STK500
    ret = stk500_check();
#else
    ret = frsky_check();
#endif
  }
  uart_deinit();

  return ret;
}


/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* MCU Configuration---------------------------------------------*/

  /* Make sure the vectors are set correctly */
  SCB->VTOR = BL_FLASH_START;

  /* Reset of all peripherals, Initializes the Flash interface and the
   * Systick.
   */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();
  __enable_irq();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  led_state_set(LED_BOOTING);

  int32_t rx_pin = -1, tx_pin;
  int8_t ret = 0;

#if XMODEM
#if defined(UART_TX_PIN_2ND)
#if defined(UART_RX_PIN_2ND)
  rx_pin = IO_CREATE(UART_RX_PIN_2ND);
#endif // UART_RX_PIN
  tx_pin = IO_CREATE(UART_TX_PIN_2ND);

  ret = boot_code_stk(UART_BAUD_2ND, rx_pin, tx_pin, duplex_pin, UART_INV_2ND);
  if (ret < 0) // timeout, start xmodem
#endif // UART_TX_PIN
  {
#if defined(UART_RX_PIN)
    rx_pin = IO_CREATE(UART_RX_PIN);
#else
    rx_pin = -1;
#endif // UART_RX_PIN
    tx_pin = IO_CREATE(UART_TX_PIN);
    ret = boot_code_xmodem(rx_pin, tx_pin);
  }

#else /* !XMODEM */

#if defined(UART_RX_PIN)
  rx_pin = IO_CREATE(UART_RX_PIN);
#endif // UART_RX_PIN
  tx_pin = IO_CREATE(UART_TX_PIN);

  /* Give a bit time for OTX to bootup */
  if ((BL_FLASH_START & 0xffff) == 0x0)
    HAL_Delay(500);

  ret = boot_code_stk(UART_BAUD, rx_pin, tx_pin, duplex_pin, UART_INV);
#if defined(UART_TX_PIN_2ND)
  if (ret < 0) {
#if defined(UART_RX_PIN_2ND)
    rx_pin = IO_CREATE(UART_RX_PIN_2ND);
#else
    rx_pin = -1;
#endif // UART_RX_PIN
    tx_pin = IO_CREATE(UART_TX_PIN_2ND);
    ret = boot_code_stk(UART_BAUD_2ND, rx_pin, tx_pin, -1, UART_INV_2ND);
  }
#endif
#endif/* XMODEM */

  if (ret < 0) {
    flash_jump_to_app();
  }

  // Reset system if something went wrong
  NVIC_SystemReset();
}


/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  memset(&RCC_OscInitStruct, 0, sizeof(RCC_OscInitTypeDef));
  memset(&RCC_ClkInitStruct, 0, sizeof(RCC_ClkInitTypeDef));

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

#if defined(STM32L0xx) || defined(STM32L4xx)
  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Enable HSI Oscillator and activate PLL with HSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
#if defined(STM32L4xx)
  RCC_OscInitStruct.PLL.PLLM = 1;  // 16MHz
  RCC_OscInitStruct.PLL.PLLN = 10; // 10 * 16MHz
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2; // 160MHz / 2
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7; // 160MHz / 7
#if defined(STM32L432xx)
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4; // 160MHz / 4
#else // !STM32L432xx
  /* STM32L433 */
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2; // 160MHz / 2
#endif // STM32L432xx

#else // !STM32L4xx
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV2;
#endif // STM32L4xx
  RCC_OscInitStruct.HSICalibrationValue = 0x10;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
#if defined(STM32L4xx)
  uint32_t flash_latency = FLASH_LATENCY_4;
#else // !STM32L4xx
  uint32_t flash_latency = FLASH_LATENCY_1;
#endif // STM32L4xx
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency) != HAL_OK)
    Error_Handler();

  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    Error_Handler();

  HAL_ResumeTick();

#elif defined(STM32L1xx)
#warning "Clock setup is missing! Should use HSI!"

#elif defined(STM32F1) || defined(STM32F3xx)

  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
#if defined(STM32F3xx)
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI; // 8MHz / DIV2 = 4MHz
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16; // 16 * 4MHz = 64MHz
#else
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2; // 8MHz / DIV2 = 4MHz
  //RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12; // 12 * 4MHz = 48MHz
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16; // 16 * 4MHz = 64MHz
#endif
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }

#if defined(STM32F3xx)
  RCC_PeriphCLKInitTypeDef PeriphClkInit;
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
#if defined(RCC_USART1CLKSOURCE_PCLK2)
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
#else
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
#endif
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
#else
  __HAL_RCC_AFIO_CLK_ENABLE();
  /** NOJTAG: JTAG-DP Disabled and SW-DP Enabled */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();
  //__HAL_AFIO_REMAP_SWJ_DISABLE();
#endif
#endif

  SystemCoreClockUpdate();
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
#if defined(PIN_BUTTON)
  gpio_btn = GPIO_Setup(IO_CREATE(PIN_BUTTON), GPIO_INPUT, (BUTTON_INVERTED ? 1 : -1));
#endif // PIN_BUTTON

#if defined(PIN_LED_GREEN)
  gpio_led_green = GPIO_Setup(IO_CREATE(PIN_LED_GREEN), GPIO_OUTPUT, -1);
#endif // PIN_LED_GREEN

#if defined(PIN_LED_RED)
  gpio_led_red = GPIO_Setup(IO_CREATE(PIN_LED_RED), GPIO_OUTPUT, -1);
#endif // PIN_LED_RED

  ws2812_init();
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state
   */
  while (1)
    ;
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line
     number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line)
   */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

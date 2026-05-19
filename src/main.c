/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "uart_ring_buffer.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Глобальні змінні для асинхронного обміну */
uint8_t rx_byte;
uint8_t tx_byte;
volatile bool tx_ready = true;

/* Екземпляри кільцевих буферів (RX та TX) */
RingBuffer rx_buffer;
RingBuffer tx_buffer;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Ініціалізуємо кільцеві буфери */
  rb_init(&rx_buffer);
  rb_init(&tx_buffer);

  /* Зводимо "курок" переривання: очікуємо 1 байт даних */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Перевіряємо наявність нових необроблених даних у кільцевому буфері */
    uint8_t process_byte;
    if (rb_pop(&rx_buffer, &process_byte))
    {
      /* 2. Логіка керування периферією 
      тут може бути будь-яка логіка */
      if (process_byte == '1')
      {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_13);
      }
      else if (process_byte == '2')
      {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
      }
      else if (process_byte == '3')
      {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_15);
      }

      /* 1. Асинхронне ЕХО: додаємо байт у чергу TX буфера */
      
      /* СТВОРЕННЯ ЗВОРОТНОГО ТИСКУ (Backpressure): 
         Якщо буфер повний, активно чекаємо (Spinlock), поки апаратний UART 
         через переривання не відправить хоча б один байт і не звільнить місце. */
      while (rb_is_full(&tx_buffer)) 
      {
        /* Процесор заблокований тут, але NVIC продовжує обробляти переривання! */
      }
      
      rb_push(&tx_buffer, process_byte);

      /* Якщо передавач зараз "спить", даємо йому стартовий поштовх (Kick-start) */
      if (tx_ready && !rb_is_empty(&tx_buffer))
      {
        tx_ready = false;                           /* Переводимо передавач у зайнятий стан */
        if (rb_pop(&tx_buffer, &tx_byte)) {         /* Беремо найстаріший байт з хвоста */
          HAL_UART_Transmit_IT(&huart1, &tx_byte, 1); /* Запускаємо першу передачу */
        }
      }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * @brief  Функція зворотного виклику (Callback), яка автоматично
 *         викликається HAL при успішному прийомі заданої кількості байт.
 * @param  huart: покажчик на структуру конфігурації UART
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  /* Перевіряємо, що переривання прийшло саме від USART1 */
  if (huart->Instance == USART1)
  {
      /* Додаємо байт у буфер. У разі переповнення rb_push оновлює overflow_count */
      rb_push(&rx_buffer, rx_byte);

    /* 3. Re-arming (перезарядження): вмикаємо переривання знову,
          оскільки HAL автоматично його вимкнув після прийому 1 байта */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/**
 * @brief  Callback завершення передачі (Transmission Complete).
 *         Викликається HAL, коли зсувний регістр TX фізично спустошено.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    /* Перевіряємо, чи є ще дані у черзі на відправку */
      if (rb_pop(&tx_buffer, &tx_byte))
    {
      /* Запускаємо апаратну передачу наступного байта */
      HAL_UART_Transmit_IT(&huart1, &tx_byte, 1);
    }
    else
    {
      /* Якщо черга порожня, переводимо передавач у стан очікування (Idle) */
      tx_ready = true;
    }
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
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
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Application Layer.
 *                   This file acts as the Application Layer. It ties together
 *                   the Driver Layer (uart_ring_buffer) and Protocol Layer (packet_protocol).
 *                   It leverages interrupts to move data in and out of the ring
 *                   buffers, and runs the application logic in the main loop,
 *                   processing valid packets provided by the Protocol Layer.
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
#include "dma.h"
#include "crc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#include "uart_ring_buffer.h"
#include "packet_protocol.h"
#include "uart_dma_manager.h"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Ring buffer instances (RX and TX) */
RingBuffer rx_buffer;
RingBuffer tx_buffer;
uint8_t my_payload[128]; /* Buffer for storing unpacked payload after CRC validation */
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
  MX_DMA_Init();
  MX_CRC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Initialize ring buffers */
  rb_init(&rx_buffer);
  rb_init(&tx_buffer);

  /* Initialize the UART DMA Manager */
  if (UART_Manager_Init(&huart1, &rx_buffer, &tx_buffer) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    UART_Manager_Task();
    
    /* 2. Software TX Flow Control (Option 2)
     * Only process incoming packets if we have enough space in the TX buffer 
     * to safely store the maximum possible response. 
     */
   // uint16_t tx_count = rb_get_count(&tx_buffer);
    // uint16_t tx_free = (RING_BUFFER_SIZE - 1) - tx_count ;
   
    uint16_t len = pkt_pop_binary_packet_crc(&rx_buffer, 0xAA, my_payload, sizeof(my_payload) - 1);
    if (len > 0)
    {
        my_payload[len] = '\0'; /* Null terminate in case it is a string */

        /* Process known string commands or default to binary echo */
        if (strcmp((char *)my_payload, "LED_ON") == 0)
        {
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); /* Turn ON LED (active low) */
          pkt_push_string_crc(&tx_buffer, 0xAA, "LED is now ON");
        }
        else if (strcmp((char *)my_payload, "LED_OFF") == 0)
        {
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); /* Turn OFF LED */
          pkt_push_string_crc(&tx_buffer, 0xAA, "LED is now OFF");
        }
        else if (strcmp((char *)my_payload, "TEST_CAPACITY") == 0)
        {
          /* Generate a 110 character string to test tx_buffer capacity */
          char cap_test[120];
          memset(cap_test, 'C', 110);
          cap_test[110] = '\0';
          pkt_push_string_crc(&tx_buffer, 0xAA, cap_test);
        }
        else if (strncmp((char *)my_payload, "Msg#", 4) == 0)
        {
          /* Echo the string back with a prefix */
          pkt_push_string_crc(&tx_buffer, 0xAA, "Echo: ");
          pkt_push_string_crc(&tx_buffer, 0xAA, (char *)my_payload);
        }
        else
        {
          /* --- RAW BINARY ARRAY HANDLING --- */
          /* Echo the raw binary array exactly as received */
          pkt_push_binary_packet_crc(&tx_buffer, 0xAA, my_payload, len);
        }
      }
    }
  }
  
  /* USER CODE END 3 */


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
 * @brief  Function for asynchronous transmission of a data array via UART.
 * @param  data: Pointer to the data array to send.
 * @param  len: Length of the data array.
 * @retval None
 */

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

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
#include "usart.h"
#include "gpio.h"
#include "uart_ring_buffer.h"

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
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Global variables for asynchronous communication */
uint8_t tx_byte;
volatile bool tx_ready = true;

/* Ring buffer instances (RX and TX) */
RingBuffer rx_buffer;
RingBuffer tx_buffer;
uint8_t my_payload[128]; /* Buffer for storing unpacked payload after CRC validation */
uint8_t rx_dma_buf[128]; /* Buffer for DMA reception */
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
  /* Initialize ring buffers */
  rb_init(&rx_buffer);
  rb_init(&tx_buffer);

  /* Start DMA reception with IDLE Line Detection */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_dma_buf, sizeof(rx_dma_buf));

 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Check if there is a valid packet with correct CRC in the ring buffer */
    /* We read it as a binary array first to determine its type */
    uint16_t len = pkt_pop_binary_packet_crc(&rx_buffer, 0xAA, my_payload, sizeof(my_payload) - 1);
    if (len > 0)
    {
      my_payload[len] = '\0'; /* Null terminate in case it is a string */

      /* 1. Determine if the payload is a string or a raw binary array.
         If all characters are printable ASCII (32-126), we treat it as a String.
         Otherwise, we treat it as a raw binary array. */
      bool is_string = true;
      for (uint16_t i = 0; i < len; i++) {
          if (my_payload[i] < 32 || my_payload[i] > 126) {
              is_string = false;
              break;
          }
      }

      if (is_string)
      {
          /* --- DEMONSTRATION OF STRING HANDLING --- */
          if (strcmp((char*)my_payload, "LED_ON") == 0)
          {
              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); /* Turn ON LED (active low) */
              pkt_push_string_crc(&tx_buffer, 0xAA, "LED is now ON");
          }
          else if (strcmp((char*)my_payload, "LED_OFF") == 0)
          {
              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); /* Turn OFF LED */
              pkt_push_string_crc(&tx_buffer, 0xAA, "LED is now OFF");
          }
          else if (strcmp((char*)my_payload, "TEST_CAPACITY") == 0)
          {
              /* Generate a 110 character string to test tx_buffer capacity */
              char cap_test[120];
              memset(cap_test, 'C', 110);
              cap_test[110] = '\0';
              pkt_push_string_crc(&tx_buffer, 0xAA, cap_test);
          }
          else
          {
              /* Echo the string back with a prefix */
              pkt_push_string_crc(&tx_buffer, 0xAA, "Echo: ");
              pkt_push_string_crc(&tx_buffer, 0xAA, (char*)my_payload);
          }
      }
      else
      {
          /* --- DEMONSTRATION OF RAW BINARY ARRAY HANDLING --- */
          /* Echo the raw binary array exactly as received without string processing */
          pkt_push_binary_packet_crc(&tx_buffer, 0xAA, my_payload, len);
      }

      /* Safely trigger the TX interrupt to send the data if the transmitter is idle */
      __disable_irq();
      if (tx_ready && !rb_is_empty(&tx_buffer))
      {
        tx_ready = false;
        if (rb_pop(&tx_buffer, &tx_byte))
        {
          HAL_UART_Transmit_IT(&huart1, &tx_byte, 1);
        }
      }
      __enable_irq();
    }
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
 * @brief  Function for asynchronous transmission of a data array via UART.
 * @param  data: Pointer to the data array to send.
 * @param  len: Length of the data array.
 * @retval None
 */


/**
 * @brief  Rx Event Callback (IDLE + DMA).
 *         Automatically called by HAL when the UART line becomes free (IDLE),
 *         or when the DMA buffer is completely filled.
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART1)
  {
    /* 1. Transfer data to the ring buffer for packet processing in main */
    rb_push_array(&rx_buffer, rx_dma_buf, Size);

    /* 2. Since RX DMA in CubeMX is configured as CIRCULAR, we forcibly stop RX
          and restart it. This guarantees new data is written from the start of rx_dma_buf. */
    HAL_UART_AbortReceive(&huart1);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_dma_buf, sizeof(rx_dma_buf));
  }
}

/**
 * @brief  Tx Transfer completed callback.
 *         Called by HAL when the TX shift register is physically empty.
 * @param  huart: pointer to a UART_HandleTypeDef structure that contains
 *                the configuration information for the specified UART module.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    /* Check if there is more data in the queue to send */
    if (rb_pop(&tx_buffer, &tx_byte))
    {
      /* Start hardware transmission of the next byte */
      HAL_UART_Transmit_IT(&huart1, &tx_byte, 1);
    }
    else
    {
      /* If the queue is empty, put the transmitter in Idle state */
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

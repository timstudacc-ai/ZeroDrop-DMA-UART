/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    crc.c
  * @brief   This file provides code for the configuration
  *          of the CRC instances.
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
#include "crc.h"

/* USER CODE BEGIN 0 */
#include "al_crc.h"
#include <string.h> /* For memcpy */
/* USER CODE END 0 */

CRC_HandleTypeDef hcrc;

/* CRC init function */
void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

void HAL_CRC_MspInit(CRC_HandleTypeDef* crcHandle)
{

  if(crcHandle->Instance==CRC)
  {
  /* USER CODE BEGIN CRC_MspInit 0 */

  /* USER CODE END CRC_MspInit 0 */
    /* CRC clock enable */
    __HAL_RCC_CRC_CLK_ENABLE();
  /* USER CODE BEGIN CRC_MspInit 1 */

  /* USER CODE END CRC_MspInit 1 */
  }
}

void HAL_CRC_MspDeInit(CRC_HandleTypeDef* crcHandle)
{

  if(crcHandle->Instance==CRC)
  {
  /* USER CODE BEGIN CRC_MspDeInit 0 */

  /* USER CODE END CRC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CRC_CLK_DISABLE();
  /* USER CODE BEGIN CRC_MspDeInit 1 */

  /* USER CODE END CRC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

uint8_t AL_CalculateCRC8(const uint8_t *data, uint16_t len)
{
    uint32_t crc32 = 0;
    
    /* Calculate how many full 32-bit words we have */
    uint16_t num_words = len / 4;
    uint16_t remaining_bytes = len % 4;
    
    /* Calculate CRC for all full 32-bit words */
    /* HAL_CRC_Calculate automatically resets the CRC before calculation */
    if (num_words > 0)
    {
        crc32 = HAL_CRC_Calculate(&hcrc, (uint32_t *)data, num_words);
    }
    
    /* Handle remaining bytes by padding to 4 bytes with zeros */
    if (remaining_bytes > 0)
    {
        uint32_t padded_word = 0;
        /* Copy the remaining bytes into the lowest addresses of padded_word */
        memcpy(&padded_word, data + (num_words * 4), remaining_bytes);
        
        if (num_words > 0)
        {
            /* If we already calculated previous words, accumulate */
            crc32 = HAL_CRC_Accumulate(&hcrc, &padded_word, 1);
        }
        else
        {
            /* If the entire payload was less than 4 bytes, calculate it as the first word */
            crc32 = HAL_CRC_Calculate(&hcrc, &padded_word, 1);
        }
    }
    
    /* XOR fold the 32-bit CRC into 8 bits */
    uint8_t crc8 = (uint8_t)(crc32 & 0xFF) ^ 
                   (uint8_t)((crc32 >> 8) & 0xFF) ^ 
                   (uint8_t)((crc32 >> 16) & 0xFF) ^ 
                   (uint8_t)((crc32 >> 24) & 0xFF);
                   
    return crc8;
}

/* USER CODE END 1 */


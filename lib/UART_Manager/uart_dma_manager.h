#ifndef UART_DMA_MANAGER_H
#define UART_DMA_MANAGER_H

#include "usart.h"
#include "uart_ring_buffer.h"

/**
 * @brief Initialize the UART DMA Manager with dependency injection.
 * @param huart Pointer to the UART handle (e.g. &huart1).
 * @param rx_ptr Pointer to the RX ring buffer.
 * @param tx_ptr Pointer to the TX ring buffer.
 * @return HAL_StatusTypeDef HAL_OK on success, otherwise error.
 */
HAL_StatusTypeDef UART_Manager_Init(UART_HandleTypeDef *huart, RingBuffer *rx_ptr, RingBuffer *tx_ptr);

/**
 * @brief Periodic task for UART Manager.
 *        Handles kicking off TX DMA transfers using Ping-Pong buffering.
 *        Must be called continuously in the main loop or RTOS task.
 */
void UART_Manager_Task(void);

/**
 * @brief Enable or disable software manual flow control.
 * @param enable true to enable flow control, false to disable.
 */
void UART_Manager_EnableSoftwareFlowControl(bool enable);

#endif /* UART_DMA_MANAGER_H */

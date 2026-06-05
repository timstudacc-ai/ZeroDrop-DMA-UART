#include "uart_dma_manager.h"
#include "dma.h"
#include <stdbool.h>



/* Encapsulated static state for the UART DMA Manager */
static UART_HandleTypeDef *p_huart = NULL; /* Pointer to the UART hardware handle */
static RingBuffer *p_rx_buf = NULL;        /* High-level software ring buffer for reception */
static RingBuffer *p_tx_buf = NULL;        /* High-level software ring buffer for transmission */

/* Hardware DMA Buffers */
static uint8_t rx_dma_buf[256]; /* Circular DMA buffer for continuous background reception */
static uint8_t tx_buf_A[256];   /* Ping-Pong TX Buffer A */
static uint8_t tx_buf_B[256];   /* Ping-Pong TX Buffer B */

/* State variables for RX Circular Extraction */
static uint16_t rx_old_pos = 0; /* Tracks the last read position in rx_dma_buf */

/* State variables for TX Ping-Pong Operation */
static volatile bool tx_dma_busy = false;  /* Flag indicating if the TX DMA is currently transmitting */
static volatile uint8_t tx_active_buf = 0; /* Indicates which ping-pong buffer is currently held by DMA (0 = A, 1 = B) */

/**
 * @brief  Initializes the UART DMA Manager.
 * @param  huart: Pointer to the UART handle
 * @param  rx_ptr: Pointer to the RX software ring buffer
 * @param  tx_ptr: Pointer to the TX software ring buffer
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef UART_Manager_Init(UART_HandleTypeDef *huart, RingBuffer *rx_ptr, RingBuffer *tx_ptr)
{
    if (huart == NULL || rx_ptr == NULL || tx_ptr == NULL)
    {
        return HAL_ERROR;
    }

    p_huart = huart;
    p_rx_buf = rx_ptr;
    p_tx_buf = tx_ptr;

    rx_old_pos = 0;
    return HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_dma_buf, sizeof(rx_dma_buf));
}

/**
 * @brief  Main Task for UART Manager.
 *         Responsible for safely pulling data from the software TX Ring Buffer 
 *         and triggering the hardware DMA transmission.
 *         Must be called continuously in the main application loop.
 */
void UART_Manager_Task(void)
{
    if (p_huart == NULL || p_rx_buf == NULL || p_tx_buf == NULL)
    {
        return;
    }

    /* 2. DMA TX Kick-off */
    NVIC_DisableIRQ(DMA2_Stream7_IRQn);
    NVIC_DisableIRQ(USART1_IRQn);
    if (!tx_dma_busy && !rb_is_empty(p_tx_buf))
    {
        uint16_t tx_len = rb_pop_array(p_tx_buf, tx_buf_A, sizeof(tx_buf_A));
        if (tx_len > 0)
        {
            tx_dma_busy = true;
            tx_active_buf = 0;
            HAL_UART_Transmit_DMA(p_huart, tx_buf_A, tx_len);
        }
    }
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA2_Stream7_IRQn);
}

/**
 * @brief  RX Event Callback triggered by the HAL library.
 *         This function is called primarily when the physical UART line goes IDLE,
 *         or when the circular DMA buffer reaches exactly half or full capacity.
 * @param  huart: Pointer to the UART handle
 * @param  Size: The absolute position (index) of the DMA write pointer within rx_dma_buf
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (p_huart != NULL && huart->Instance == p_huart->Instance)
    {
        uint16_t new_pos = Size;
        
        if (new_pos != rx_old_pos)
        {
            if (new_pos > rx_old_pos)
            {
                uint16_t len = new_pos - rx_old_pos;
                rb_push_array(p_rx_buf, &rx_dma_buf[rx_old_pos], len);
            }
            else
            {
                /* Wrap-around detected */
                uint16_t len1 = sizeof(rx_dma_buf) - rx_old_pos;
                if (len1 > 0)
                {
                    rb_push_array(p_rx_buf, &rx_dma_buf[rx_old_pos], len1);
                }
                if (new_pos > 0)
                {
                    rb_push_array(p_rx_buf, rx_dma_buf, new_pos);
                }
            }
            
            rx_old_pos = new_pos;
            if (rx_old_pos >= sizeof(rx_dma_buf))
            {
                rx_old_pos = 0;
            }
        }

        /* HACK: Fix the STM32 HAL IDLE bug natively in the interrupt. 
         * If HAL sets state to READY, we manually force it back to BUSY_RX 
         * and re-enable the IDLE interrupt bit so the Circular DMA never drops a byte. */
        if (huart->RxState == HAL_UART_STATE_READY)
        {
            huart->RxState = HAL_UART_STATE_BUSY_RX;
            SET_BIT(huart->Instance->CR1, USART_CR1_IDLEIE);
        }
    }
}

/**
 * @brief  TX Complete Callback triggered when the DMA finishes transmitting a buffer.
 *         Implements Ping-Pong continuous transmission. If more data exists in the 
 *         software TX Ring Buffer, it instantly loads it into the alternate ping-pong 
 *         buffer and restarts the DMA without waiting for the main loop.
 * @param  huart: Pointer to the UART handle
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (p_huart != NULL && huart->Instance == p_huart->Instance)
    {
        if (tx_active_buf == 0)
        {
            uint16_t len = rb_pop_array(p_tx_buf, tx_buf_B, sizeof(tx_buf_B));
            if (len > 0)
            {
                tx_active_buf = 1;
                HAL_UART_Transmit_DMA(p_huart, tx_buf_B, len);
            }
            else
            {
                tx_dma_busy = false;
            }
        }
        else
        {
            uint16_t len = rb_pop_array(p_tx_buf, tx_buf_A, sizeof(tx_buf_A));
            if (len > 0)
            {
                tx_active_buf = 0;
                HAL_UART_Transmit_DMA(p_huart, tx_buf_A, len);
            }
            else
            {
                tx_dma_busy = false;
            }
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (p_huart != NULL && huart->Instance == p_huart->Instance)
    {
       
        HAL_UART_AbortReceive(p_huart);
        rx_old_pos = 0;
        HAL_UARTEx_ReceiveToIdle_DMA(p_huart, rx_dma_buf, sizeof(rx_dma_buf));
    }
}

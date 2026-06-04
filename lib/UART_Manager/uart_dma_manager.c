#include "uart_dma_manager.h"
#include "dma.h"
#include <stdbool.h>

#define ENABLE_HW_FLOW_CONTROL 0 /* Встановіть 1, щоб увімкнути RTS/CTS керування потоком */
#define RX_BUFFER_WATERMARK 32

/* Encapsulated static state for the UART DMA Manager */
static UART_HandleTypeDef *p_huart = NULL;
static RingBuffer *p_rx_buf = NULL;
static RingBuffer *p_tx_buf = NULL;

static uint8_t rx_dma_buf[256];
static uint8_t tx_buf_A[256];
static uint8_t tx_buf_B[256];

static uint16_t rx_old_pos = 0;
static volatile bool tx_dma_busy = false;
static volatile uint8_t tx_active_buf = 0; /* 0 = tx_buf_A active, 1 = tx_buf_B active */

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

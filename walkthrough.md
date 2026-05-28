# 🚀 STM32 High-Performance UART DMA Driver Integration Walkthrough

This walkthrough explains how to port and implement the non-blocking UART DMA Ring Buffer driver from this repository into any other STM32 project.

## Step 1: Copy the Library Files
Copy the `lib/Ring_buffer` directory into your new project's source tree.
- **For PlatformIO:** Place it inside the `lib/` directory.
- **For STM32CubeIDE:** Copy `uart_ring_buffer.h` to `Core/Inc` and `uart_ring_buffer.c` to `Core/Src`.

*(Optional: If you need packetization and CRC, copy `lib/Packey_protocol` as well).*

## Step 2: STM32CubeMX Configuration
To ensure the hardware layer works correctly with the driver, configure the following in STM32CubeMX (or your equivalent configurator):

### UART Settings
- **Mode:** `Asynchronous`
- **Hardware Flow Control:** `RTS/CTS` (if flow control is desired, otherwise `Disable`)
- **NVIC Settings:** Enable the **USART global interrupt**.

### DMA Settings
- **USART_RX:**
  - Request: `USART_RX`
  - Mode: **Circular** *(Crucial for continuous reception)*
  - Increment Address: Memory = **Checked**, Peripheral = **Unchecked**
  - Data Width: Byte
- **USART_TX:**
  - Request: `USART_TX`
  - Mode: **Normal** *(Crucial for Ping-Pong buffering)*
  - Increment Address: Memory = **Checked**, Peripheral = **Unchecked**
  - Data Width: Byte

> [!IMPORTANT]
> **Initialization Order:** The DMA controller must be initialized *before* the UART. CubeMX usually handles this automatically, but verify in `main.c` that `MX_DMA_Init()` is called before `MX_USART1_UART_Init()`.

---

## Step 3: Variables and Initialization

In your new `main.c`, include the header and define the necessary buffers and flags:

```c
/* Includes ------------------------------------------------------------------*/
#include "uart_ring_buffer.h"

/* Private variables ---------------------------------------------------------*/
/* Application Ring Buffers */
RingBuffer rx_buffer;
RingBuffer tx_buffer;

/* DMA RX Buffer */
uint8_t rx_dma_buf[128];

/* DMA TX Ping-Pong Buffers */
uint8_t tx_buf_A[128];               
uint8_t tx_buf_B[128];               
volatile bool tx_dma_busy = false;    
volatile uint8_t tx_active_buf = 0;   
```

In the `main()` function, initialize the ring buffers and start the initial DMA reception with IDLE line detection:

```c
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  rb_init(&rx_buffer);
  rb_init(&tx_buffer);

  /* Start DMA reception with IDLE Line Detection */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_dma_buf, sizeof(rx_dma_buf));
  /* USER CODE END 2 */
```

---

## Step 4: Interrupt Callbacks

Copy the three essential callbacks to the bottom of your `main.c` (inside `/* USER CODE BEGIN 4 */`):

```c
/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART1)
  {
    /* Push data to software ring buffer */
    rb_push_array(&rx_buffer, rx_dma_buf, Size);

    /* Forcibly stop and restart RX to reset Circular DMA index */
    HAL_UART_AbortReceive(huart);
    HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_dma_buf, sizeof(rx_dma_buf));
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (tx_active_buf == 0)
    {
      /* Buffer A finished — drain ring buffer into B and send */
      uint16_t len = rb_pop_array(&tx_buffer, tx_buf_B, sizeof(tx_buf_B));
      if (len > 0)
      {
        tx_active_buf = 1;
        HAL_UART_Transmit_DMA(huart, tx_buf_B, len);
      }
      else
      {
        tx_dma_busy = false; 
      }
    }
    else
    {
      /* Buffer B finished — drain ring buffer into A and send */
      uint16_t len = rb_pop_array(&tx_buffer, tx_buf_A, sizeof(tx_buf_A));
      if (len > 0)
      {
        tx_active_buf = 0;
        HAL_UART_Transmit_DMA(huart, tx_buf_A, len);
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
  if (huart->Instance == USART1)
  {
    /* Recover from hardware errors */
    HAL_UART_AbortReceive(huart);
    HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_dma_buf, sizeof(rx_dma_buf));
  }
}
/* USER CODE END 4 */
```

---

## Step 5: Application Loop & Flow Control

Finally, in your `while (1)` loop or RTOS Task, read from `rx_buffer` and write to `tx_buffer`. If you are using RTS/CTS flow control, include the watermark checks:

```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 1. Hardware RX Flow Control (Watermark Check) */
    uint16_t rx_free = (RING_BUFFER_SIZE - 1) - rb_get_count(&rx_buffer);
    if (rx_free < 32) {
      CLEAR_BIT(huart1.Instance->CR1, USART_CR1_RE); // De-assert RTS
    } else {
      SET_BIT(huart1.Instance->CR1, USART_CR1_RE);   // Assert RTS
    }

    /* 2. Process Incoming Data (Backpressure Check) */
    uint16_t tx_free = (RING_BUFFER_SIZE - 1) - rb_get_count(&tx_buffer);
    if (tx_free >= 120) // Only pop if we have room to send a response
    {
      uint8_t byte;
      if (rb_pop(&rx_buffer, &byte))
      {
        /* Process byte/packet here... */
        
        /* Queue response... */
        rb_push(&tx_buffer, byte); 
      }
    }

    /* 3. DMA TX Kick-off (The Starter Motor) */
    NVIC_DisableIRQ(DMA2_Stream7_IRQn); // IMPORTANT: Use your specific TX DMA IRQ!
    NVIC_DisableIRQ(USART1_IRQn);
    if (!tx_dma_busy && !rb_is_empty(&tx_buffer))
    {
      uint16_t tx_len = rb_pop_array(&tx_buffer, tx_buf_A, sizeof(tx_buf_A));
      if (tx_len > 0)
      {
        tx_dma_busy = true;
        tx_active_buf = 0;
        HAL_UART_Transmit_DMA(&huart1, tx_buf_A, tx_len);
      }
    }
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA2_Stream7_IRQn);
  }
  /* USER CODE END WHILE */
```

You have now successfully integrated the high-performance UART driver!

---

## Step 6: Enabling Hardware Flow Control (Optional)

If your hardware setup requires RTS/CTS flow control to prevent buffer overflows under heavy load, you can dynamically enable it using the provided abstraction in `main.c`.

To enable or disable Hardware Flow Control, simply modify the `ENABLE_HW_FLOW_CONTROL` macro at the top of your `main.c`:

```c
/* Set to 1 to enable RTS/CTS Hardware Flow Control, or 0 to disable */
#define ENABLE_HW_FLOW_CONTROL 1 
```

Behind the scenes, this macro calls `HW_CONTROL_ON(&huart1)` or `HW_CONTROL_OFF(&huart1)`, which dynamically alters the `CR3` hardware register to toggle `RTSE` and `CTSE` bits without needing a full peripheral reinitialization.

---

## Step 7: Validating with the HIL Python Testbench

This repository includes a robust Hardware-in-the-Loop (HIL) testbench (`Test_script/uart_testbench.py`) to validate your firmware under extreme conditions.

### Prerequisites
Make sure you have Python 3.8+ installed along with the `pyserial` package:
```bash
pip install pyserial
```

### Running the Testbench
Connect your STM32 to your PC via a USB-to-Serial adapter and run the script:
```bash
python Test_script/uart_testbench.py
```

### Testbench Features & Architecture
The script operates interactively and provides several configurable options:

1. **Port Selection:** Automatically detects available COM ports and prompts you to select one.
2. **Test Modes:**
   - **Binary:** Sends raw byte arrays and expects an exact echo back. Validates the integrity of the binary payload and CRC.
   - **String:** Sends ASCII commands (`LED_ON`, `LED_OFF`, `TEST_CAPACITY`, etc.) and validates the specific textual responses generated by the application layer.
3. **Chaos Modifiers:** Simulates real-world serial line issues:
   - *Fragmented*: Introduces artificial delays between bytes to test the driver's timeout/IDLE logic.
   - *Burst*: Sends multiple packets back-to-back without waiting.
   - *Noise*: Injects random garbage bytes before and after valid packets to test the protocol layer's parsing and recovery algorithms.
   - *Overflow*: Purposely sends payloads larger than the buffer capacity to test backpressure and flow control.

Once the test completes, it generates a comprehensive stress test report detailing the success rate, timeout errors, CRC mismatches, and overall effective bitrate (throughput).

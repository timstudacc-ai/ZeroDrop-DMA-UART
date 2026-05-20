# 🚀 High-Performance UART DMA Driver & HIL Testbench

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)
[![MCU](https://img.shields.io/badge/STM32-F411CEU6-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f411.html)
[![Framework](https://img.shields.io/badge/Framework-STM32CubeHAL-brightgreen.svg)](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)
[![Testing](https://img.shields.io/badge/Testing-HIL_Python-yellow.svg)](#)

> **Objective:** A production-ready, zero-CPU-overhead UART driver implementation designed for the STM32F411CEU6 microcontroller. This project demonstrates advanced embedded software architecture, leveraging Direct Memory Access (DMA) alongside hardware interrupts to guarantee robust data handling under heavy backpressure, verified via an automated Hardware-in-the-Loop (HIL) stress test.

---

## 1. Production Architecture Overview

The core firmware architecture is built around the synergy of **DMA Circular Mode**, **UART IDLE Line Detection**, and a **Software Ring Buffer (FIFO)**. This approach eliminates the bottlenecks associated with standard blocking calls or byte-by-byte interrupt designs.

### DMA & IDLE Line Synergy
Traditional interrupt-driven UART reception (e.g., `HAL_UART_Receive_IT`) triggers an Interrupt Service Routine (ISR) for every single incoming byte, leading to massive CPU overhead and potential data loss at high baud rates. 

This architecture implements a **Zero-CPU-Overhead Reception Path**:
1. **DMA Streaming:** The DMA controller directly transfers incoming bytes from the UART Data Register (`DR`) to a temporary RAM buffer (`rx_dma_buf`) in the background. The CPU is completely unburdened during active transmission.
2. **IDLE Event Detection:** When the host finishes sending a burst of data, the UART line remains logically high for one frame. The hardware detects this silence and fires an **IDLE Line Interrupt**.
3. **Data Commit:** The `HAL_UARTEx_RxEventCallback` is triggered. The driver calculates exactly how many bytes were received, pushes the block into the application's Ring Buffer, and seamlessly restarts the DMA stream.

> [!TIP]
> **Optimization Benefit:** By combining DMA with IDLE Line detection, the CPU only intervenes *once per data block* rather than *once per byte*. This allows the MCU to process complex payloads asynchronously without blocking mission-critical tasks or dropping frames during heavy traffic.

### Ring Buffer Sizing
The size of the ring buffers can be easily customized by modifying `RING_BUFFER_SIZE` inside `uart_ring_buffer.h`.

> [!CAUTION]
> **Sizing Rule:** To prevent buffer overrun and silent data loss, `RING_BUFFER_SIZE` should be configured to be **at least 2x larger** than the maximum possible size of any single received data payload.

---

## 2. Hardware Configuration Guide (STM32CubeMX)

To replicate this architecture, exact peripheral configuration within STM32CubeMX is mandatory. The following checklist ensures proper hardware linkage for the DMA controller and UART interrupts.

| Peripheral / Feature | Parameter | Required Configuration |
| :--- | :--- | :--- |
| **USART1** | Mode | Asynchronous |
| **USART1 / NVIC** | USART1 global interrupt | **Enabled** |
| **USART1 / DMA** | DMA Request | `USART1_RX` |
| **DMA Settings** | Stream | `DMA2 Stream 2` |
| **DMA Settings** | Mode | **Circular** |
| **DMA Settings** | Increment Address | Peripheral: **Disabled**, Memory: **Enabled** (MINC) |
| **DMA Settings** | Data Width | Peripheral: `Byte`, Memory: `Byte` |

> [!IMPORTANT]
> **Initialization Order Matters:** Ensure that `MX_DMA_Init()` is called **before** `MX_USART1_UART_Init()` in the `main.c` initialization sequence. The UART handle links to the DMA handle during its MSP initialization, requiring the DMA clock to be active.

---

## 3. Hardware-in-the-Loop (HIL) Python Stress-Test Bench

Validating firmware resilience requires more than manual terminal testing. This repository includes an automated Python-based testbench (`uart_testbench.py`) designed to subject the MCU to rigorous crash testing and validate data integrity under continuous load.

### Binary Protocol Container
The testbench utilizes a structured binary protocol to encapsulate data, making it immune to stray bytes or desynchronization:
`[Start Marker: 0xAA] [Length: 1B] [Payload: N bytes] [XOR Checksum: 1B]`

### Testbench Operational Logic
- **Bombardment & Backpressure:** The script rapidly streams hundreds of dynamically generated packets to the STM32, testing the Ring Buffer's elasticity and the DMA's circular wrapping logic.
- **Data Integrity Validation:** Every echoed packet is captured and its `XOR Checksum` is re-calculated. The testbench instantly flags dropped bytes, mismatched lengths, or corrupted frames.
- **Metrics Calculation:** Upon completion, the script generates a comprehensive report calculating *Success Rate*, *Timeouts*, *Throughput (pkt/s)*, and *Effective Bitrate*.

---

## 4. PlatformIO Project Layout

The repository follows a clean, standardized PlatformIO directory structure, separating hardware abstraction, application logic, and testing scripts.

```text
📦 Ring_Buffer_UART_driver
┣ 📂 include
┃ ┣ 📜 dma.h
┃ ┣ 📜 gpio.h
┃ ┣ 📜 main.h
┃ ┣ 📜 packet_protocol.h     # Binary frame & string wrappers
┃ ┣ 📜 uart_ring_buffer.h    # Thread-safe FIFO API
┃ ┗ 📜 usart.h
┣ 📂 src
┃ ┣ 📜 dma.c                 # DMA initialization
┃ ┣ 📜 gpio.c
┃ ┣ 📜 main.c                # App layer & callback handling
┃ ┣ 📜 packet_protocol.c     # Packet parsing & CRC validation
┃ ┣ 📜 stm32f4xx_it.c        # Interrupt vectors
┃ ┣ 📜 uart_ring_buffer.c    # Ring buffer logic
┃ ┗ 📜 usart.c               # UART initialization
┣ 📂 Test_script
┃ ┗ 📜 uart_testbench.py     # Python HIL stress-test bench
┗ 📜 platformio.ini          # Build flags & monitor config
```

> [!NOTE]
> All firmware components are written in modular C, ensuring strict separation of concerns between the Hardware Driver layer, the Protocol Parsing layer, and the Application logic.

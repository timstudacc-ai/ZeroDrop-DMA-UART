# 🚀 ZeroDrop-DMA-UART

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)
[![MCU](https://img.shields.io/badge/STM32-F411CEU6-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f411.html)
[![Framework](https://img.shields.io/badge/Framework-STM32CubeHAL-brightgreen.svg)](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)
[![Testing](https://img.shields.io/badge/Testing-HIL_Python-yellow.svg)](#)

> **Objective:** A production-ready, high-level UART driver implementation designed for the STM32F411CEU6 microcontroller. This project demonstrates an advanced embedded software architecture, leveraging Direct Memory Access (DMA) for **both data transmission (TX) and reception (RX)** to guarantee robust data handling with near-zero CPU overhead. It features double-buffering (Ping-Pong) for TX, a circular buffer with IDLE line detection for RX, and is verified via an automated Hardware-in-the-Loop (HIL) stress test.

---

## 1. Production Architecture Overview

The firmware is designed with a strict separation of concerns, divided into four distinct layers. This modular approach ensures that the application logic is completely decoupled from hardware constraints, eliminating bottlenecks associated with standard blocking calls or byte-by-byte interrupt designs.

```mermaid
flowchart TD
    subgraph Hardware["1. Hardware Layer"]
        direction TB
        RX[Pin UART RX] -->|Physical Signal| USART_RX[USART1 RX]
        USART_RX -->|Byte Assembly| DR_RX[Data Register DR]
        DR_RX -->|DMA Request| DMA_RX[DMA RX Stream]
        DMA_RX -->|Circular Mode| DMABUF[(rx_dma_buf)]
        
        DMA_TX[DMA TX Stream] -->|Normal Mode| DR_TX[Data Register DR]
        DR_TX -->|Byte Shift| USART_TX[USART1 TX]
        USART_TX -->|Physical Signal| TX[Pin UART TX]
    end

    subgraph ISR["2. ISR Layer"]
        direction TB
        IDLE[IDLE Line Event] -->|Interrupt| RXCB[HAL_UARTEx_RxEventCallback]
        DMABUF -.->|Extract Block| RXCB
        RXCB -->|rb_push_array| RINGBUF[(Software Ring Buffer)]
        
        TC[Tx Complete Event] -->|Interrupt| TXCB[HAL_UART_TxCpltCallback]
        TXCB -->|Toggle Active DMA Buffer| DMA_TX
    end

    subgraph Protocol["3. Protocol Layer"]
        direction TB
        RINGBUF -.->|Raw Bytes| POP[pkt_pop_binary_packet_crc]
        POP -->|Sync Start Byte| SYNC{Valid Start & Length?}
        SYNC -- Yes --> CRC{Verify XOR CRC}
        SYNC -- No --> DROP1[Drop Garbage]
        CRC -- Valid --> PAYLOAD[Extract Payload]
        CRC -- Invalid --> DROP2[Drop Corrupted]
    end

    subgraph App["4. Application Layer"]
        direction TB
        PAYLOAD -.->|Clean Data| MAIN[Main Loop / Business Logic]
        MAIN -->|Process Command| EXEC[Execute Action]
        EXEC -->|Generate Response| FORMAT[Format TX Packet]
        FORMAT -->|Fill tx_buf_A or tx_buf_B| TXBUF[(TX Ping-Pong Buffers)]
        TXBUF -.->|Trigger DMA Transmit| TC
    end

    Hardware ==> ISR
    ISR ==> Protocol
    Protocol ==> App
    App ==> ISR
    
    classDef layer fill:#2b2d31,stroke:#a6adc8,stroke-width:2px,color:#cdd6f4
    class Hardware,ISR,Protocol,App layer
```

### 1. Hardware Layer (Low-Level Driver)
At the lowest level, the CPU is completely decoupled from the physical data transfer.
- **Silent DMA Reception:** The DMA Controller operates in the background, silently pulling raw incoming bytes directly from the UART hardware register and streaming them into a temporary circular memory array (`rx_dma_buf`). This happens entirely without waking up the CPU, conserving massive amounts of processing power.
- **RTS/CTS Flow Control:** To prevent faults and silent data loss when the software buffers near overflow, the system utilizes hardware flow control. If the MCU cannot process data fast enough, it de-asserts the RTS pin, commanding the remote device to pause. Conversely, if the remote device's buffer is full, it de-asserts our CTS pin, safely pausing our TX DMA transfers until space clears up.

### 2. ISR & Synchronization Layer
- **IDLE Line Interrupt Trigger:** Because data arrives in variable-length packets, standard byte-counting DMA is insufficient. Instead, the hardware detects when the physical UART line goes quiet (IDLE). This IDLE Line Interrupt instantly wakes the CPU, signaling that a complete packet burst has arrived. The ISR then quickly extracts this block of data and pushes it into a thread-safe software Ring Buffer to trigger parsing.
- **Ping-Pong TX Buffering:** For data transmission, the system uses a dual-buffer (Ping-Pong) approach (`tx_buf_A` and `tx_buf_B`). While the DMA actively transmits data out of Buffer A in the background, the CPU can safely format the next packet and calculate checksums inside Buffer B. This strict separation prevents race conditions and data corruption, ensuring the pipeline remains fully saturated.

### 3. Protocol & Application Layer (High-Level Parser)
Running asynchronously in the main loop, the high-level protocol parser (`packet_protocol.c`) is strictly separated from hardware timings. It blindly consumes raw bytes from the Ring Buffer, searching for the `[Start]` marker. Once found, it extracts the variable-length `[Payload]` and verifies the `[CRC]` checksum. Only pristine, error-free payloads are passed up to the Application Layer for business logic execution, guaranteeing that the application never acts on corrupted telemetry.

> [!TIP]
> **Optimization Benefit:** By combining DMA Ping-Pong for TX with DMA+IDLE Line detection for RX, the CPU only intervenes *per data block* rather than *per byte*. This makes it a high-level driver that can process complex payloads asynchronously without blocking mission-critical tasks or dropping frames under heavy traffic.

### Ring Buffer Sizing
The size of the ring buffers can be easily customized by modifying `RING_BUFFER_SIZE` inside `lib/Ring_buffer/uart_ring_buffer.h`.

> [!CAUTION]
> **Sizing Rule:** To prevent buffer overrun and silent data loss, `RING_BUFFER_SIZE` should be configured to be **at least 2x larger** than the maximum possible size of any single received data payload.

---

## 2. Hardware Configuration Guide

To replicate this architecture, ensure proper hardware linkage for the DMA controller and UART interrupts.

| Peripheral / Feature | Parameter | Required Configuration |
| :--- | :--- | :--- |
| **USARTX** | Mode | Asynchronous |
| **USARTX / NVIC** | USARTX global interrupt | **Enabled** |
| **USARTX / DMA (RX)** | DMA Request | `USARTX_RX` (Mode: **Circular**) |
| **USARTX / DMA (TX)** | DMA Request | `USARTX_TX` (Mode: **Normal**) |
| **DMA Settings** | Increment Address | Peripheral: **Disabled**, Memory: **Enabled** (MINC) |
| **DMA Settings** | Data Width | Peripheral: `Byte`, Memory: `Byte` |
| **Hardware Wiring** | CTS Pin | Connect to remote **RTS** |
| **Hardware Wiring** | RTS Pin | Connect to remote **CTS** |

> [!IMPORTANT]
> **Initialization Order Matters:** Ensure that the DMA controller (and its clock) is initialized **before** the UART initialization. The UART hardware links to the DMA channels during its initialization phase.

---

## 3. Hardware-in-the-Loop (HIL) Python Stress-Test Bench

Validating firmware resilience requires rigorous testing. This repository includes an automated Python-based testbench (`uart_testbench.py`) to validate data integrity under continuous load.

### Binary Protocol Container
The testbench utilizes a structured binary protocol to encapsulate data:
`[Start Marker: 0xAA] [Length: 1B] [Payload: N bytes] [XOR Checksum: 1B]`

### Testbench Operational Logic
- **Bombardment & Backpressure:** Rapidly streams dynamically generated packets to test Ring Buffer elasticity and TX Ping-Pong efficiency.
- **Data Integrity Validation:** Every echoed packet is captured and its `XOR Checksum` is re-calculated, instantly flagging corrupted frames.
- **Metrics Calculation:** Reports *Success Rate*, *Timeouts*, *Throughput (pkt/s)*, and *Effective Bitrate*.

### Benchmark Results

#### Results for 9600 Baud
| Test Case | Mode | Modifiers | Packets | Payload | Success | Error | Timeouts |
|---|---|---|---|---|---|---|---|
| Baseline Binary | binary | None | 100 | 8 | 100.0% | 0.0% | 0 |
| Burst Mode Binary | binary | burst | 2500 | 16 | 100.0% | 0.0% | 0 |
| Fragmented Binary | binary | fragmented | 20 | 16 | 100.0% | 0.0% | 0 |
| Burst + Noise | binary | burst, noise | 1250 | 16 | 94.4% | 0.0% | 70 |
| Baseline String (Small) | string | None | 100 | 8 | 99.0% | 0.0% | 1 |
| Max Payload(120) Binary | binary | None | 100 | 120 | 100.0% | 0.0% | 0 |
| Max Payload(110) String | string | None | 100 | 110 | 100.0% | 0.0% | 0 |
| Burst Mode String | string | burst | 2500 | 16 | 100.0% | 0.0% | 0 |
| Fragmented String | string | fragmented | 20 | 16 | 100.0% | 0.0% | 0 |
| Noise Injection Binary | binary | noise | 100 | 24 | 86.0% | 0.0% | 14 |
| Burst + Fragmented | binary | burst, fragmented | 250 | 16 | 100.0% | 0.0% | 0 |
| Burst + Fragmented + Noise | binary | burst, fragmented, noise | 250 | 16 | 94.0% | 0.0% | 15 |

#### Results for 115200 Baud
| Test Case | Mode | Modifiers | Packets | Payload | Success | Error | Timeouts |
|---|---|---|---|---|---|---|---|
| Baseline Binary | binary | None | 100 | 8 | 100.0% | 0.0% | 0 |
| Burst Mode Binary | binary | burst | 2500 | 16 | 74.3% | 0.1% | 640 |
| Fragmented Binary | binary | fragmented | 20 | 16 | 100.0% | 0.0% | 0 |
| Burst + Noise | binary | burst, noise | 1250 | 16 | 74.8% | 0.0% | 315 |
| Baseline String (Small) | string | None | 100 | 8 | 99.0% | 0.0% | 1 |
| Max Payload(120) Binary | binary | None | 100 | 120 | 74.0% | 0.0% | 26 |
| Max Payload(110) String | string | None | 100 | 110 | 96.0% | 0.0% | 4 |
| Burst Mode String | string | burst | 2500 | 16 | 75.4% | 0.1% | 612 |
| Fragmented String | string | fragmented | 20 | 16 | 100.0% | 0.0% | 0 |
| Noise Injection Binary | binary | noise | 100 | 24 | 99.0% | 0.0% | 1 |
| Burst + Fragmented | binary | burst, fragmented | 250 | 16 | 100.0% | 0.0% | 0 |
| Burst + Fragmented + Noise | binary | burst, fragmented, noise | 250 | 16 | 94.8% | 0.0% | 13 |

#### Results for 921600 Baud
| Test Case | Mode | Modifiers | Packets | Payload | Success | Error | Timeouts |
|---|---|---|---|---|---|---|---|
| Baseline Binary | binary | None | 100 | 8 | 100.0% | 0.0% | 0 |
| Burst Mode Binary | binary | burst | 2500 | 16 | 99.4% | 0.0% | 16 |
| Fragmented Binary | binary | fragmented | 20 | 16 | 100.0% | 0.0% | 0 |
| Burst + Noise | binary | burst, noise | 1250 | 16 | 91.0% | 0.0% | 113 |
| Baseline String (Small) | string | None | 100 | 8 | 99.0% | 0.0% | 1 |
| Max Payload(120) Binary | binary | None | 100 | 120 | 80.0% | 0.0% | 20 |
| Max Payload(110) String | string | None | 100 | 110 | 92.0% | 0.0% | 8 |
| Burst Mode String | string | burst | 2500 | 16 | 99.0% | 0.0% | 26 |
| Fragmented String | string | fragmented | 20 | 16 | 100.0% | 0.0% | 0 |
| Noise Injection Binary | binary | noise | 100 | 24 | 98.0% | 0.0% | 2 |
| Burst + Fragmented | binary | burst, fragmented | 250 | 16 | 100.0% | 0.0% | 0 |
| Burst + Fragmented + Noise | binary | burst, fragmented, noise | 250 | 16 | 91.2% | 0.0% | 22 |

---

> [!NOTE]
> All firmware components are written in modular C, ensuring strict separation of concerns between the Hardware Driver layer, the Protocol Parsing layer, and the Application logic.

---

## 4. API / Integration Example

Using this driver in your own application is straightforward. Below is a clean, 15-line example demonstrating initialization and a non-blocking send/receive call:

```c
#include "uart_dma_manager.h"
#include "packet_protocol.h"

// Global ring buffers
RingBuffer rx_buffer;
RingBuffer tx_buffer;
uint8_t payload[128];

int main(void) {
    HAL_Init();
    
    // 1. Initialization
    rb_init(&rx_buffer);
    rb_init(&tx_buffer);
    UART_Manager_Init(&huart1, &rx_buffer, &tx_buffer);
    UART_Manager_SetHwFlowControl(true);

    while (1) {
        // 2. Non-blocking Task: Kicks off pending TX DMA transfers
        UART_Manager_Task(); 
        
        // 3. Attempt to pop a valid, CRC-verified packet from RX ring buffer
        uint16_t len = pkt_pop_binary_packet_crc(&rx_buffer, 0xAA, payload, sizeof(payload));
        if (len > 0) {
            // Echo the received payload back instantly
            pkt_push_binary_packet_crc(&tx_buffer, 0xAA, payload, len); 
        }
    }
}
```

---

## 6. Build & Flash Instructions

This project is built using the **PlatformIO** ecosystem. 

**Dependencies:** STM32Cube HAL (managed automatically by PlatformIO).

Run the following commands in your terminal to clone, build, and upload the firmware directly to your board:

```bash
# Clone the repository
git clone https://github.com/yourusername/stm32-high-perf-uart-dma-protocol.git
cd stm32-high-perf-uart-dma-protocol

# Compile and upload the firmware (ensure your ST-Link is connected)
pio run --target upload

# Open the serial monitor
pio device monitor
```

---

## 7. Memory Footprint (Resource Usage)

Embedded systems require strict resource management. Below is the memory footprint when compiled in **Release mode** (`-Os`) for the STM32F411CEU6 via PlatformIO:

- **Flash (ROM):** ~12.5 KB (Includes the entire STM32 HAL overhead)
- **SRAM (RAM):** ~2.1 KB (Efficiently allocated for the 256-byte Ring Buffers, Ping-Pong DMA arrays, and Core system variables)

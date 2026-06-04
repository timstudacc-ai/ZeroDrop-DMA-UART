#ifndef UART_RING_BUFFER_H
#define UART_RING_BUFFER_H

/**
 * @file uart_ring_buffer.h
 * @brief Driver Layer (Hardware Abstraction) for UART ring buffer.
 *
 * This layer manages the movement of raw bytes between the hardware UART
 * (interrupts) and RAM. It is agnostic to data framing, packets, or contents.
 * It serves as a data source and sink for the Protocol Layer.
 */

#include <stdint.h>
#include <stdbool.h>
#define RING_BUFFER_SIZE 512
/**
 * @brief Structure representing a circular buffer.
 */
typedef struct
{
    volatile uint8_t buffer[RING_BUFFER_SIZE]; /**< Data storage array */
    volatile uint16_t head;                    /**< Write index (where next byte goes) */
    volatile uint16_t tail;                    /**< Read index (where next byte is read from) */
    volatile uint32_t overflow_count;          /**< Tracks how many bytes were lost due to buffer full */
} RingBuffer;

/**
 * @brief Initializes the ring buffer indices and counters.
 * @param rb Pointer to the RingBuffer instance.
 */
void rb_init(RingBuffer *rb);

/**
 * @brief Pushes a single byte into the ring buffer.
 * @param rb Pointer to the RingBuffer instance.
 * @param data Byte to be written.
 * @return true if successful, false if the buffer is full.
 */
bool rb_push(RingBuffer *rb, uint8_t data);

/**
 * @brief Pops a single byte from the ring buffer.
 * @param rb Pointer to the RingBuffer instance.
 * @param data Pointer to store the read byte.
 * @return true if a byte was read, false if the buffer is empty.
 */
bool rb_pop(RingBuffer *rb, uint8_t *data);

/**
 * @brief Checks if the ring buffer is empty.
 * @param rb Pointer to the RingBuffer instance.
 * @return true if empty, false otherwise.
 */
bool rb_is_empty(RingBuffer *rb);

/**
 * @brief Checks if the ring buffer is full.
 * @param rb Pointer to the RingBuffer instance.
 * @return true if full, false otherwise.
 */
bool rb_is_full(RingBuffer *rb);

/**
 * @brief Pushes an array of bytes into the ring buffer.
 * @param rb Pointer to the RingBuffer instance.
 * @param data Pointer to the source data array.
 * @param len Number of bytes to push.
 * @return Number of bytes actually pushed (may be less than len if buffer fills up).
 */
uint16_t rb_push_array(RingBuffer *rb, const uint8_t *data, uint16_t len);

/**
 * @brief Pops an array of bytes from the ring buffer.
 * @param rb Pointer to the RingBuffer instance.
 * @param data Pointer to the destination array.
 * @param len Number of bytes to pop.
 * @return Number of bytes actually popped (may be less than len if buffer becomes empty).
 */
uint16_t rb_pop_array(RingBuffer *rb, uint8_t *data, uint16_t len);

/**
 * @brief Gets the number of bytes currently stored in the ring buffer.
 * @param rb Pointer to the RingBuffer instance.
 * @return The number of unread bytes.
 */
uint16_t rb_get_count(RingBuffer *rb);

#endif /* UART_RING_BUFFER_H */
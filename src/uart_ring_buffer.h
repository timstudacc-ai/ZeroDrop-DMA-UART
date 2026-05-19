#ifndef UART_RING_BUFFER_H
#define UART_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 64

typedef struct
{
    volatile uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t overflow_count;
} RingBuffer;

void rb_init(RingBuffer *rb);
bool rb_push(RingBuffer *rb, uint8_t data);
bool rb_pop(RingBuffer *rb, uint8_t *data);
bool rb_is_empty(RingBuffer *rb);
bool rb_is_full(RingBuffer *rb);

#endif /* UART_RING_BUFFER_H */
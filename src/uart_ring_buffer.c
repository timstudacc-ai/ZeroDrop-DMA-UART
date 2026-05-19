#include "uart_ring_buffer.h"

void rb_init(RingBuffer *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_count = 0;
}

bool rb_is_empty(RingBuffer *rb)
{
    return rb->head == rb->tail;
}

bool rb_is_full(RingBuffer *rb)
{
    uint16_t next_head = (rb->head + 1) % RING_BUFFER_SIZE;
    return next_head == rb->tail;
}

bool rb_push(RingBuffer *rb, uint8_t data)
{
    uint16_t next_head = (rb->head + 1) % RING_BUFFER_SIZE;
    if (next_head == rb->tail)
    {
        rb->overflow_count++;
        return false;
    }
    rb->buffer[rb->head] = data;
    rb->head = next_head;
    return true;
}

bool rb_pop(RingBuffer *rb, uint8_t *data)
{
    if (rb->head == rb->tail)
    {
        return false;
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return true;
}
/**
 * @file uart_ring_buffer.c
 * @brief Implementation of the UART ring buffer driver layer.
 *
 * This file contains the implementation of the raw byte movement logic.
 * The functions here provide thread-safe-like (ISR-safe) writing and reading 
 * if used correctly (single producer, single consumer). 
 * It interacts directly with interrupts in main.c and feeds the protocol parser.
 */

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

uint16_t rb_push_array(RingBuffer *rb, const uint8_t *data, uint16_t len)
{
    uint16_t pushed = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        if (rb_push(rb, data[i]))
        {
            pushed++;
        }
        else
        {
            break; /* Buffer is full, stop writing */
        }
    }
    return pushed;
}

uint16_t rb_pop_array(RingBuffer *rb, uint8_t *data, uint16_t len)
{
    uint16_t popped = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        if (rb_pop(rb, &data[i]))
        {
            popped++;
        }
        else
        {
            break; /* Buffer is empty, stop reading */
        }
    }
    return popped;
}

uint16_t rb_get_count(RingBuffer *rb)
{
    uint16_t head = rb->head;
    uint16_t tail = rb->tail;
    if (head >= tail)
    {
        return head - tail;
    }
    else
    {
        return RING_BUFFER_SIZE - tail + head;
    }
}
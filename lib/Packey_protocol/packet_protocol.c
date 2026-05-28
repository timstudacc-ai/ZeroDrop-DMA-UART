/**
 * @file packet_protocol.c
 * @brief Implementation of the Protocol Layer for serial communication.
 *
 * This file translates raw byte streams from the RingBuffer into structural data.
 * It knows how to scan for start markers, calculate checksums, and extract complete
 * logical packets to be handed over to the Application Layer (main.c).
 */

#include "packet_protocol.h"
#include "al_crc.h"
#include <string.h> /* For memcpy */
/* =========================================================
 * Binary Protocol with CRC: Start Byte + Length + Data + CRC
 * ========================================================= */

uint16_t pkt_push_binary_packet_crc(RingBuffer *rb, uint8_t start_byte, const uint8_t *data, uint8_t len)
{
    uint16_t count = rb_get_count(rb);
    uint16_t free_space = RING_BUFFER_SIZE - 1 - count;

    /* Check fit: Start(1) + Len(1) + Data(len) + CRC(1) */
    if (free_space < (3 + len))
        return 0;

    rb_push(rb, start_byte);
    rb_push(rb, len);
    uint16_t pushed = rb_push_array(rb, data, len);

    /* Calculate 1-byte folded hardware CRC */
    uint8_t crc = AL_CalculateCRC8(data, len);
    rb_push(rb, crc);

    return pushed + 3;
}

uint16_t pkt_pop_binary_packet_crc(RingBuffer *rb, uint8_t start_byte, uint8_t *out_data, uint8_t max_len)
{
    uint16_t count = rb_get_count(rb);
    if (count < 3)
        return 0; /* Wait for start, len, and CRC */

    bool found = false;
    while (!rb_is_empty(rb))
    {
        if (rb->buffer[rb->tail] == start_byte)
        {
            found = true;
            break;
        }
        else
        {
            uint8_t dummy;
            rb_pop(rb, &dummy);
        }
    }
    if (!found)
        return 0;

    count = rb_get_count(rb);
    if (count < 3)
        return 0;

    uint16_t len_idx = (rb->tail + 1) % RING_BUFFER_SIZE;
    uint8_t payload_len = rb->buffer[len_idx]; 

    /* BOUNDS CHECK: If the requested length makes the packet larger than
     * the physical capacity of the ring buffer, it is impossible to ever receive.
     * This means the start byte was falsely identified or data was dropped.
     */
    if ((3 + payload_len) >= RING_BUFFER_SIZE)
    {
        uint8_t dummy;
        rb_pop(rb, &dummy); /* Discard the corrupted start byte */
        return 0; /* Let the next loop resynchronize */
    }

    if (count < (uint16_t)(3 + payload_len))
        return 0; /* Wait for data and CRC */

    uint8_t dummy;
    rb_pop(rb, &dummy); /* Pop start */
    rb_pop(rb, &dummy); /* Pop len */

    uint16_t read_len = (payload_len <= max_len) ? payload_len : max_len;

    /* Pop the full payload into a temporary stack buffer to calculate CRC */
    uint8_t full_payload[255]; /* max payload_len is a uint8_t (255) */
    for (uint16_t i = 0; i < payload_len; i++)
    {
        rb_pop(rb, &full_payload[i]);
    }
    
    uint8_t calculated_crc = AL_CalculateCRC8(full_payload, payload_len);

    uint8_t received_crc;
    rb_pop(rb, &received_crc);

    if (calculated_crc != received_crc)
        return 0; /* Corrupted packet, discard! */
        
    /* Copy valid data to the output buffer */
    if (read_len > 0)
    {
        memcpy(out_data, full_payload, read_len);
    }
    return read_len;
}

/* =========================================================
 * String Protocol via Binary CRC Container
 * ========================================================= */

uint16_t pkt_push_string_crc(RingBuffer *rb, uint8_t start_byte, const char *str)
{
    uint8_t len = (uint8_t)strlen(str);
    return pkt_push_binary_packet_crc(rb, start_byte, (const uint8_t *)str, len);
}

uint16_t pkt_pop_string_crc(RingBuffer *rb, uint8_t start_byte, char *out_str, uint8_t max_len)
{
    /* Reserve 1 byte for the null-terminator */
    if (max_len == 0) return 0;
    uint8_t read_capacity = max_len - 1;

    uint16_t read_len = pkt_pop_binary_packet_crc(rb, start_byte, (uint8_t *)out_str, read_capacity);
    if (read_len > 0)
    {
        out_str[read_len] = '\0'; /* Securely null-terminate */
    }
    return read_len;
}
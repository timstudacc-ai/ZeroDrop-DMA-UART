#ifndef PACKET_PROTOCOL_H
#define PACKET_PROTOCOL_H

/**
 * @file packet_protocol.h
 * @brief Protocol Layer.
 *
 * This layer is responsible for interpreting the raw bytes provided by the 
 * Driver Layer (uart_ring_buffer) and grouping them into logical data packets.
 * It provides functions to encapsulate (push) and parse (pop) binary packets 
 * with CRC verification.
 * This layer separates the application logic from the raw stream processing.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart_ring_buffer.h"

/**
 * @brief Pushes a binary packet with CRC: [START_BYTE] [LEN] [DATA...] [CRC].
 * @param rb Target ring buffer.
 * @param start_byte Synchronization start byte.
 * @param data Payload array.
 * @param len Payload length.
 * @return Total number of bytes pushed.
 */
uint16_t pkt_push_binary_packet_crc(RingBuffer *rb, uint8_t start_byte, const uint8_t *data, uint8_t len);

/**
 * @brief Pushes a CRC-protected binary packet: [START_BYTE] [LEN] [DATA...] [CRC].
 * @param rb Source ring buffer.
 * @param start_byte Synchronization start byte.
 * @param out_data Destination array for the payload.
 * @param max_len Maximum capacity of the destination array.
 * @return Length of the extracted payload, or 0 if packet is incomplete/corrupted.
 */
uint16_t pkt_pop_binary_packet_crc(RingBuffer *rb, uint8_t start_byte, uint8_t *out_data, uint8_t max_len);

/**
 * @brief Pushes a null-terminated string using the CRC binary protocol.
 * @param rb Target ring buffer.
 * @param start_byte Synchronization start byte.
 * @param str Null-terminated string to send.
 * @return Total number of bytes pushed.
 */
uint16_t pkt_push_string_crc(RingBuffer *rb, uint8_t start_byte, const char *str);

/**
 * @brief Pops a string encapsulated in the CRC binary protocol.
 * @param rb Source ring buffer.
 * @param start_byte Synchronization start byte.
 * @param out_str Destination buffer for the extracted string (will be null-terminated).
 * @param max_len Maximum capacity of the destination array (including null-terminator).
 * @return Length of the extracted string (excluding null-terminator), or 0 on failure.
 */
uint16_t pkt_pop_string_crc(RingBuffer *rb, uint8_t start_byte, char *out_str, uint8_t max_len);

#endif /* PACKET_PROTOCOL_H */
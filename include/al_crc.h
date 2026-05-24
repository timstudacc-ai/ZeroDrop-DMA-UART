#ifndef AL_CRC_H
#define AL_CRC_H

#include <stdint.h>

/**
 * @brief Calculate a 1-byte folded CRC using the hardware CRC peripheral.
 *
 * This function calculates the 32-bit hardware CRC (STM32 built-in, poly 0x4C11DB7)
 * and then XOR folds it down to 1 byte.
 * Byte streams that are not multiples of 4 are padded with zeros internally.
 * 
 * @param data Pointer to the payload buffer.
 * @param len Length of the payload in bytes.
 * @return 8-bit XOR folded CRC.
 */
uint8_t AL_CalculateCRC8(const uint8_t *data, uint16_t len);

#endif /* AL_CRC_H */

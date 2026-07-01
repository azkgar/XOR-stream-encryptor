/**********************************************************************************************
 * @file crypto.c
 * @brief Implementation of bitwise left-rotation for key manipulation.
 * 
 * This file implements the functions declared in crypto.h.
 * 
 * @author Azkary Garcia
 * @date July 1st, 2026
 * @version 1.0.0
*********************************************************************************************/

#define _POSIX_C_SOURCE 200809L
#include "crypto.h"

/**********************************************************************************************
 * @name rotateKeyLeft
 * @brief Rotates the key to the left by 1 bit in-place.
 * @param key Pointer to the key.
 * @param keyLen Length of the key.
*********************************************************************************************/
void rotateKeyLeft(uint8_t *key, size_t keyLen)
{
    // Variable to store the bit that wraps around from the first byte to the last byte
    uint8_t overflowBit;

    // Extract bit that wraps around from the first byte to the last byte
    overflowBit = (key[0] >> 7u) & LSB_MASK;

    // Shift all bytes of key one bit to the left
    for(size_t idx = 0; idx < keyLen - 1; idx++)
    {
        // Variable to store MSB of next byte
        uint8_t nextByteMSB;

        // Get MSB of next byte for shift
        nextByteMSB = (key[idx + 1u] >> 7u) & LSB_MASK;

        // Rotate key by BIT_KEY_ROTATION bits to the left for the current byte
        key[idx] <<= BIT_KEY_ROTATION;

        // Set last bit to the stored one
        key[idx] |= nextByteMSB;
    }

    // Rotate key by BIT_KEY_ROTATION bits to the left for the last byte
    key[keyLen - 1u] <<= BIT_KEY_ROTATION;

    // Set last bit that wraps up from the 1st byte
    key[keyLen - 1u] |= overflowBit;
}

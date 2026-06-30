/**
 * Implements bitwise left-rotation array manipulation and the block XOR logic
 */

#include "include/crypto.h"

 /**********************************************
 * Rotate the key buffer left by 1 bit in-place
 * Key: byte array of length key length
 *********************************************/

void rotateKeyLeft(uint8_t *key, size_t keyLen)
{
    // Variable to store bit that wraps around from the first byte to the last byte
    uint8_t overflowBit;

    // Get bit that wraps around from the first byte to the last byte
    overflowBit = (key[0] >> 7u) & LSB_MASK;

    // Shift all bytes one bit to the left
    for(size_t idx = 0; idx < keyLen - 1; idx++)
    {
        // Variable to store first bit of idx + 1 byte
        uint8_t nextByteFirstBit;

        // Get first byte on next byte for shift
        nextByteFirstBit = (key[idx + 1u] >> 7u) & LSB_MASK;

        // Rotate key by BIT_KEY_ROTATION bits to the left for the current byte
        key[idx] <<= BIT_KEY_ROTATION ;

        // Set last bit to the stored one
        key[idx] |= nextByteFirstBit;
    }

    // Rotate key by BIT_KEY_ROTATION bits to the left for the last byte
    key[keyLen - 1u] <<= BIT_KEY_ROTATION ;

    // Set last bit that wraps up from the 1st byte
    key[keyLen - 1u] |= overflowBit;
}
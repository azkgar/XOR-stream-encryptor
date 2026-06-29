/*
* Rotate they key buffer left by 1 bit in-place.
* Key: byte array of length key length
*/

void rotateKeyLeft(uint8_t *key, size_t key_len)
{
    // Variable to store first bit of first byte
    uint8_t firstBit;

    // Get first bit of first byte
    firstBit = (key[0] >> 7u) & 0x01;

    // Shift all bytes one bit to the left
    for(size_t idx = 0; idx < key_len - 1; idx++)
    {
        // Variable to store first bit of idx + 1 byte
        uint8_t nextByteFirstBit;

        // Get first byte on next byte for shift
        nextByteFirstBit = (key[idx + 1] >> 7u) & 0x01;

        // Shift current byte
        key[idx] <<= 1u;

        // Set last bit to the stored one
        key[idx] |= nextByteFirstBit;
    }

    // Shift last byte
    key[key_len - 1] <<= 1u;

    // Set last bit that wraps up from the 1st byte
    key[key_len - 1] |= firstBit;
}
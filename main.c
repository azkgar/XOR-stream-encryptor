#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**********************************************
 * Rotate the key buffer left by 1 bit in-place
 * Key: byte array of length key length
 *********************************************/

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


/**
 * 
 */
int main(int argc, char *argv[])
{
    // Variable for number of threads
    int threads;
    // Variable for key file
    char *keyFile;
    // Variable for catching the command prompt read result
    int opt;

    // Initialize threads to 1 as default value
    threads = 1;
    // Initialize key file to NULL
    keyFile = NULL;

    while((opt = getopt(argc, argv, "n:k:")) != -1)
    {
        switch (opt)
        {
            case 'n':
                threads = atoi(optarg);
                break;

            case 'k':
                keyFile = optarg;
                break;

            default:
                fprintf(stderr,
                        "Usage: %s -n threads -k keyfile\n",
                        argv[0]);
                return 1;
        }
    }

    // Check if key file is provided
    if(keyFile == NULL)
    {
        fprintf(stderr, 
            "Error: key file is required (-k keyfile)\n");
        return 1;
    }

    // Check if number of threads is valid
    if(threads < 1)
    {
        fprintf(stderr,
            "Error: number of threads must be greater or equal than 1\n");
        return 1;
    }

    return 0;
}
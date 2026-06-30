#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


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
    // Variable for key length
    size_t keyLen;
    // Variable for key buffer
    uint8_t *key;

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

    // Load key from file
    FILE *keyFilePtr = fopen(keyFile, "rb");

    // Check if file opened successfully
    if(keyFilePtr == NULL)
    {
        fprintf(stderr,
            "Error: can't open key file '%s'\n",
            keyFile);
        return 1;
    }

    fseek(keyFilePtr, 0, SEEK_END);

    keyLen = (size_t)ftell(keyFilePtr);
    rewind(keyFilePtr);

    // Allocate memory for key
    key = (uint8_t *)malloc(keyLen * sizeof(uint8_t));

    // Check if memory allocation was successful
    if(key == NULL)
    {
        fprintf(stderr,
            "Error: memory allocation failed for key. Out of memory\n");
        fclose(keyFilePtr);
        return 1;
    }

    // Read key from file
    fread(key, 1, keyLen, keyFilePtr);
    fclose(keyFilePtr);

    fprintf(stderr, "Threads: %d || Key Size: %zu bytes\n", threads, keyLen);

    // Free memory
    free(key);

    return 0;
}
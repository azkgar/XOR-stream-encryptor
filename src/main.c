/**********************************************************************************************
 * @file main.c
 * @brief 
 * 
 * This file 
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/

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

    if(keyLen == 0)
    {
        fprintf(stderr,
            "Error: key file '%s' is empty\n",
            keyFile);
        fclose(keyFilePtr);
        return 1;
    }

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
    size_t bytesRead = fread(key, 1, keyLen, keyFilePtr);
    if (bytesRead != keyLen)
    {
        fprintf(stderr,
            "Error: failed to read key from file '%s'\n",
            keyFile);
        fclose(keyFilePtr);
        free(key);
        return 1;
    }
    fclose(keyFilePtr);

    fprintf(stderr, 
            "Threads: %d || Key Size: %zu bytes\n", 
            threads,
            keyLen);

    WorkQueue queue;
    workQueueInit(&queue, threads);

    pthread_t *threadPool = (pthread_t *)malloc(sizeof(pthread_t) * threads);

    if(threadPool == NULL)
    {
        fprintf(stderr, 
                "Error: failed to allocate memory for thread pool\n");
        exit(1);
    }

    for(int idx = 0; idx < threads; idx++)
    {
        int ret = pthread_create(&threadPool[idx], NULL, workerThread, &queue);
        if(ret != 0)
        {
            fprintf(stderr, 
                    "Error: failed to create thread %d\n", 
                    idx);
            exit(1);
        }
    }

    processStdin(&queue, key, keyLen);

    for(int idx = 0; idx < threads; idx++)
    {
        pthread_join(threadPool[idx], NULL);
    }

    // Free memory
    free(threadPool);
    workQueueDestroy(&queue);
    free(key);

    return 0;
}
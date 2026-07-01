/**********************************************************************************************
 * @file main.c
 * @brief Entry point for the XOR stream encryption utility.
 *
 * This file parses command-line arguments, loads the key file, sets up the work queue and 
 * worker threads, and orchestrates encryption of stdin to stdout.
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "crypto.h"
#include "queue.h"
#include "utils.h"


/**
 * 
 */
int main(int argc, char *argv[])
{
    // Local variable for number of threads
    int threads;
    // Local variable for key file
    char *keyFile;
    // Local variable for catching the command prompt read result
    int opt;
    // Local variable for key length
    size_t keyLen;
    // Local variable for key buffer
    uint8_t *key;
    // Local variable to store key from file
    FILE *keyFilePtr;
    // Local variable to store the amount of bytes read
    size_t bytesRead;

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
    keyFilePtr = fopen(keyFile, "rb");

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
    bytesRead = fread(key, 1, keyLen, keyFilePtr);
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
        workQueueDestroy(&queue);
        free(key);
        return 1;
    }

    for(int idx = 0; idx < threads; idx++)
    {
        int ret = pthread_create(&threadPool[idx], NULL, workerThread, &queue);
        if(ret != 0)
        {
            fprintf(stderr, 
                    "Error: failed to create thread %d\n", 
                    idx);
            pthread_mutex_lock(&queue.lock);
            queue.finished = 1;
            pthread_cond_bradcast(&queue.notEmpty);
            pthread_mutex_unlock(&queue.lock);

            for(int j = 0; j < idx; j++)
            {
                pthread_join(threadPool[j], NULL);
            }
            free(threadPool);
            workQueueDestroy(&queue);
            free(key);
            
            return 1;
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

/* TODO: Fix comments and refactor main so it's more readable */
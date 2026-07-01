/**********************************************************************************************
 * @file main.c
 * @brief Entry point for the XOR stream encryption utility.
 *
 * This file parses command-line arguments, loads the key file, sets up the work queue and 
 * worker threads, and orchestrates encryption of stdin to stdout.
 * 
 * @author Azkary Garcia
 * @date July 1st, 2026
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

// Forward declarations
static int       parseArgs(int argCount, char *argValues[], int *threads, char **keyFile);
static uint8_t  *loadKey(const char *keyFilePath, size_t *keyLen);
static pthread_t *spawnWorkers(WorkQueue *queue, int threads);


/**********************************************************************************************
 * @name main
 * @brief Entry point. Parses arguments, loads the key, spawns worker threads,
 *        drives encryption of stdin to stdout, and cleans up on exit.
 * @param argCount   Number of command line arguments.
 * @param argValues  Array of command line argument strings.
 * @return 0 on success, 1 on any error.
*********************************************************************************************/
int main(int argCount, char *argValues[])
{
    // Local variable to hold the requested number of worker threads
    int threads;
    // Local variable to hold the path to the key file
    char *keyFile;
    // Local variable to hold the encryption key bytes
    uint8_t *key;
    // Local variable to hold the length of the key in bytes
    size_t keyLen;
    // Local variable to hold the shared work queue
    WorkQueue queue;
    // Local variable to hold the pool of worker thread handles
    pthread_t *threadPool;

    // Parse -n and -k arguments, validate thread count and key file path
    if(parseArgs(argCount, argValues, &threads, &keyFile) != 0)
    {
        return 1;
    }

    // Load the key file into memory and get its length
    key = loadKey(keyFile, &keyLen);
    if(key == NULL)
    {
        return 1;
    }

    // Log startup info to stderr
    fprintf(stderr, "Threads: %d || Key Size: %zu bytes\n", threads, keyLen);

    // Initialize the shared work queue with capacity equal to thread count
    workQueueInit(&queue, threads);

    // Spawn all worker threads, get back their handles for joining later
    threadPool = spawnWorkers(&queue, threads);
    if(threadPool == NULL)
    {
        workQueueDestroy(&queue);
        free(key);
        return 1;
    }

    // Read stdin, encrypt each block, write ciphertext to stdout in order
    processStdin(&queue, key, keyLen);

    // Wait for all worker threads to finish processing and exit cleanly
    for(int idx = 0; idx < threads; idx++)
    {
        pthread_join(threadPool[idx], NULL);
    }

    // Release all resources before exit
    free(threadPool);
    workQueueDestroy(&queue);
    free(key);

    return 0;
}

/**********************************************************************************************
 * @name parseArgs
 * @brief Parses command line arguments to extract thread count and key file path.
 * @param argCount   Number of command line arguments.
 * @param argValues  Array of command line argument strings.
 * @param threads    Output pointer populated with the requested thread count.
 * @param keyFile    Output pointer populated with the key file path string.
 * @return 0 on success, 1 on invalid arguments.
*********************************************************************************************/
int parseArgs(int argCount, char *argValues[], int *threads, char **keyFile)
{
    // Local variable to hold each parsed option character from getopt
    int parsedOption;

    // Set default values before parsing
    *threads = 1;
    *keyFile = NULL;

    // Parse command line options: -n expects an integer, -k expects a path
    while((parsedOption = getopt(argCount, argValues, "n:k:")) != -1)
    {
        switch(parsedOption)
        {
            case 'n':
                // Convert the thread count argument from string to integer
                *threads = atoi(optarg);
                break;
            case 'k':
                // Store the key file path
                *keyFile = optarg;
                break;
            default:
                fprintf(stderr,
                        "Usage: %s [-n threads] [-k keyfile]\n",
                        argValues[0]);
                return 1;
        }
    }

    // Validate that a key file was provided
    if(*keyFile == NULL)
    {
        fprintf(stderr, "Error: key file is required (-k keyfile)\n");
        return 1;
    }

    // Validate that thread count is at least 1
    if(*threads < 1)
    {
        fprintf(stderr,
                "Error: number of threads must be greater or equal than 1\n");
        return 1;
    }

    return 0;
}

/**********************************************************************************************
 * @name loadKey
 * @brief Opens the key file, reads its contents into a buffer.
 * @param keyFilePath  Path to the binary key file.
 * @param keyLen       Output pointer populated with the key length in bytes.
 * @return Pointer to buffer containing the key, or NULL on failure.
*********************************************************************************************/
uint8_t *loadKey(const char *keyFilePath, size_t *keyLen)
{
    // Local variable to hold the key file handle
    FILE *keyFileHandle;
    // Local variable to hold the allocated key buffer
    uint8_t *keyBuffer;
    // Local variable to hold the number of bytes read from the key file
    size_t bytesRead;

    // Open the key file in binary read mode
    keyFileHandle = fopen(keyFilePath, "rb");
    if(keyFileHandle == NULL)
    {
        fprintf(stderr, "Error: can't open key file '%s'\n", keyFilePath);
        return NULL;
    }

    // Seek to end to determine file size
    fseek(keyFileHandle, 0, SEEK_END);
    *keyLen = (size_t)ftell(keyFileHandle);
    rewind(keyFileHandle);

    // Reject empty key files
    if(*keyLen == 0)
    {
        fprintf(stderr, "Error: key file '%s' is empty\n", keyFilePath);
        fclose(keyFileHandle);
        return NULL;
    }

    // Allocate buffer to hold the key bytes
    keyBuffer = (uint8_t *)malloc(*keyLen * sizeof(uint8_t));
    if(keyBuffer == NULL)
    {
        fprintf(stderr, "Error: memory allocation failed for key. Out of memory\n");
        fclose(keyFileHandle);
        return NULL;
    }

    // Read the key bytes into the buffer
    bytesRead = fread(keyBuffer, 1, *keyLen, keyFileHandle);
    
    if(bytesRead != *keyLen)
    {
        fprintf(stderr, "Error: failed to read key from file '%s'\n", keyFilePath);
        fclose(keyFileHandle);
        free(keyBuffer);
        return NULL;
    }

    // Close the file
    fclose(keyFileHandle);

    return keyBuffer;
}

/**********************************************************************************************
 * @name spawnWorkers
 * @brief Allocates the thread pool and spawns all worker threads.
 * @param queue    Pointer to the initialized work queue passed to each thread.
 * @param threads  Number of worker threads to create.
 * @return Pointer to pthread_t array, or NULL on failure.
 *         On failure, any already running threads are joined before returning.
*********************************************************************************************/
pthread_t *spawnWorkers(WorkQueue *queue, int threads)
{
    // Local variable to hold the thread pool array
    pthread_t *threadPool;
    // Local variable to hold the return code from pthread_create
    int createResult;
    // Local variable used when joining already started threads on failure
    int joinIdx;

    // Allocate array to hold all thread handles
    threadPool = (pthread_t *)malloc(sizeof(pthread_t) * threads);
    if(threadPool == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for thread pool\n");
        return NULL;
    }

    // Spawn each worker thread, passing the shared queue as its argument
    for(int idx = 0; idx < threads; idx++)
    {
        createResult = pthread_create(&threadPool[idx], NULL, workerThread, queue);
        if(createResult != 0)
        {
            fprintf(stderr, "Error: failed to create thread %d\n", idx);

            // Signal already-running threads to exit cleanly
            pthread_mutex_lock(&queue->lock);
            queue->finished = 1;
            pthread_cond_broadcast(&queue->notEmpty);
            pthread_mutex_unlock(&queue->lock);

            // Join all threads that were successfully created before this failure
            for(joinIdx = 0; joinIdx < idx; joinIdx++)
            {
                pthread_join(threadPool[joinIdx], NULL);
            }

            free(threadPool);
            return NULL;
        }
    }

    return threadPool;
}

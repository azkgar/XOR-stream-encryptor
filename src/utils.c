/**********************************************************************************************
 * @file utils.c
 * @brief Implementation of utility functions for the XOR stream encryptor.
 * 
 * This file implements the functions declared in utils.h.
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/

#include "utils.h"
#include "crypto.h"

/**********************************************************************************************
 * @name workerThread
 * @brief Worker thread function that processes blocks from the work queue.
 * @param arg Pointer to the work queue.
 * @return NULL
*********************************************************************************************/
void *workerThread(void *arg)
{
    // Local variable to hold the work queue pointer
    WorkQueue *queue;
    // Local variable to hold the block pointer
    Block *block;
    
    // Cast the argument to WorkQueue pointer
    queue = (WorkQueue *)arg;

    // Infinite loop to process blocks from the queue
    while(1)
    {
        // Lock the queue mutex to safely access the queue
        pthread_mutex_lock(&queue->lock);

        // Wait until there is a block in the queue or the queue is finished
        while(queue->head == queue->tail && !queue->finished)
        {
            pthread_cond_wait(&queue->notEmpty, &queue->lock);
        }

        // If the queue is finished and empty, exit the loop
        if(queue->head == queue->tail && queue->finished)
        {
            pthread_mutex_unlock(&queue->lock);
            break;
        }

        // Get the next block from the queue
        block = queue->blocks[queue->head];

        // Move the head pointer to the next position
        queue->head = (queue->head + 1) % queue->capacity;

        // Release the queue mutex
        pthread_mutex_unlock(&queue->lock);

        // XOR the block against its prerotated key
        for(size_t idx = 0; idx < block->dataLen; idx++)
        {
            block->output[idx] = block->data[idx] ^ block->key[idx];
        }

        // Lock the block mutex to safely update the block's done status
        pthread_mutex_lock(&block->lock);

        // Mark the block as done
        block->done = 1;

        // Signal that the block is ready for writing
        pthread_cond_signal(&block->ready);
        // Release the block mutex
        pthread_mutex_unlock(&block->lock);
    }

    return NULL;
}

/**********************************************************************************************
 * @name processStdin
 * @brief Processes input from stdin and manages the work queue.
 * @param queue Pointer to the work queue.
 * @param key Pointer to the key.
 * @param keyLen Length of the key.
*********************************************************************************************/
void processStdin(WorkQueue *queue, uint8_t *key, size_t keyLen)
{
    // Local variable to hold the capacity of the queue
    size_t capacity;
    // Local variable to hold the array of pending blocks
    Block **pendingBlocks;
    // Local variable to hold the rotated key
    uint8_t *rotatedKey;
    // Local variable to hold the current block index
    size_t blockIdx;
    // Local variable to hold the index of the next block to write
    size_t nextToWrite;
    // Local variable to hold the block pointer
    Block *block;
    // Local variable to hold the number of bytes read from stdin
    size_t bytesRead;
    // Local variable to hold the next block pointer
    Block *nextBlock; 
    // Local variable to get next block current status
    int done;

    // Get the capacity of the queue
    capacity = queue->capacity;

    // Allocate memory for the array of pending blocks
    pendingBlocks = (Block **)calloc(capacity, sizeof(Block *));

    // Check if memory allocation was successful
    if(pendingBlocks == NULL)
    {
        fprintf(stderr, 
                "Error: failed to allocate memory for pending blocks\n");
        exit(1);
    }

    // Allocate memory for the rotated key
    rotatedKey = (uint8_t *)malloc(keyLen);

    // Check if memory allocation was successful
    if(rotatedKey == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for rotated key\n");
        exit(1);
    }

    // Copy the original key into the rotated key buffer
    memcpy(rotatedKey, key, keyLen);

    // Initialize block index and next-to-write index
    blockIdx = 0;
    nextToWrite = 0;

    // Infinite loop to read blocks from stdin and process them
    while(1)
    {
        // malloc a buffer of keyLen bytes for block data
        block = (Block *)malloc(sizeof(Block));

        // Check if memory allocation was successful
        if(block == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for new block\n");
            exit(1);
        }

        // Allocate memory for block data
        block->data = (uint8_t *)malloc(keyLen);

        // Check if memory allocation was successful
        if(block->data == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for block data\n");
            exit(1);
        }

        // Read up to keyLen bytes from stdin into block data
        bytesRead = fread(block->data, 1, keyLen, stdin);

        // If no bytes were read, free the block and break the loop
        if(bytesRead == 0)
        {
            free(block->data);
            free(block);
            break;
        }

        // Set the block's data length
        block->dataLen = bytesRead;

        // Allocate memory for block key
        block->key = (uint8_t *)malloc(keyLen);

        // Check if memory allocation was successful
        if(block->key == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for block key\n");
            exit(1);
        }

        // Copy rotated key to current block 
        memcpy(block->key, rotatedKey, keyLen);

        // Allocate memory for block output
        block->output = (uint8_t *)malloc(keyLen);

        // Check if memory allocation was successful
        if(block->output == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for block output\n");
            exit(1);
        }

        // Set block's key length, index and done flag
        block->keyLen = keyLen;
        block->blockIdx = blockIdx;
        block->done = 0;

        // Initialize block mutex and condition variable
        pthread_mutex_init(&block->lock, NULL);
        pthread_cond_init(&block->ready, NULL);

        // Store block in pending buffer
        pendingBlocks[blockIdx % capacity] = block;

        // Push block into queue
        workQueuePush(queue, block);

        // Rotate key for next block
        rotateKeyLeft(rotatedKey, keyLen);

        // Move block index to the next block
        blockIdx++;

        // 
        while(pendingBlocks[nextToWrite % capacity] != NULL)
        {
            nextBlock = pendingBlocks[nextToWrite % capacity];
            pthread_mutex_lock(&nextBlock->lock);
            done = nextBlock->done;
            pthread_mutex_unlock(&nextBlock->lock);

            if(done)
            {
                writeBlock(nextBlock);
                pendingBlocks[nextToWrite % capacity] = NULL;
                nextToWrite++;
            }
            else
            {
                break; // The next block is not done yet, exit the loop
            }
        }

    }

    // Blocking drain: write everything still pending
    while (nextToWrite < blockIdx)
    {
        block = pendingBlocks[nextToWrite % capacity];
        writeBlock(block);
        pendingBlocks[nextToWrite % capacity] = NULL;
        nextToWrite++;
    }

    // Free memory for pending blocks and rotated key
    free(pendingBlocks);
    free(rotatedKey);

    // Lock queue mutex for thread safe access
    pthread_mutex_lock(&queue->lock);

    // Set queue to finished
    queue->finished = 1;

    // Broadcast that queue is not empty and unlock queue mutex
    pthread_cond_broadcast(&queue->notEmpty);
    pthread_mutex_unlock(&queue->lock);
}


/*
TODO: think of a better param name than arg for workerThread. Maybe queuePtr or something like that.
TODO: add comments to the blocking drain sections 
*/
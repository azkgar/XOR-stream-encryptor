/**********************************************************************************************
 * @file utils.c
 * @brief Implementation of utility functions for the XOR stream encryptor.
 * 
 * This file implements the functions declared in utils.h.
 * 
 * @author Azkary Garcia
 * @date July 1st, 2026
 * @version 1.0.0
*********************************************************************************************/

#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include "crypto.h"

/**********************************************************************************************
 * @name workerThread
 * @brief Worker thread function that processes blocks from the work queue.
 * @param context Pointer to the work queue.
 * @return NULL
*********************************************************************************************/
void *workerThread(void *context)
{
    // Local variable to hold the work context pointer
    WorkQueue *queue;
    // Local variable to hold the block pointer
    Block *block;
    
    // Cast the context to WorkQueue pointer
    queue = (WorkQueue *)context;

    // Infinite loop to process blocks from the queue
    while(1)
    {
        // Lock the queue mutex to safely access the queue
        pthread_mutex_lock(&queue->lock);

        // Wait until there is a block in the queue or the queue is finished
        while(queue->count == 0 && !queue->finished)
        {
            pthread_cond_wait(&queue->notEmpty, &queue->lock);
        }

        // If the queue is finished and empty, exit the loop
        if(queue->count == 0 && queue->finished)
        {
            pthread_mutex_unlock(&queue->lock);
            break;
        }

        // Get the next block from the queue
        block = queue->blocks[queue->head];

        // Move the head pointer to the next position
        queue->head = (queue->head + 1) % queue->capacity;

        // Decrease the count
        queue->count--;

        // Signal that the queue is not full
        pthread_cond_signal(&queue->notFull);

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
 * @name createBlock
 * @brief Allocates and initializes a Block struct with data read from stdin.
 * @param keyLen  Size of the key in bytes (also the block size).
 * @param key     Pointer to the pre-rotated key snapshot for this block.
 * @param blockIdx Index of this block in the input stream.
 * @return Pointer to a fully initialized Block, or NULL on EOF (0 bytes read).
 *         Exits on allocation failure.
*********************************************************************************************/
Block *createBlock(size_t keyLen, uint8_t *key, size_t blockIdx)
{
    // Local variable to hold the new block pointer
    Block *block;
    // Local variable to hold bytes read from stdin
    size_t bytesRead;

    // Allocate block struct
    block = (Block *)malloc(sizeof(Block));
    if(block == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for new block\n");
        exit(1);
    }

    // Allocate data buffer
    block->data = (uint8_t *)malloc(keyLen);
    if(block->data == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for block data\n");
        free(block);
        exit(1);
    }

    // Read up to keyLen bytes from stdin into data buffer
    bytesRead = fread(block->data, 1, keyLen, stdin);

    // End of frame, discard pre-allocated shell and signal caller
    if(bytesRead == 0)
    {
        free(block->data);
        free(block);
        return NULL;
    }

    // Allocate and snapshot the pre-rotated key for this block
    block->key = (uint8_t *)malloc(keyLen);
    if(block->key == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for block key\n");
        free(block->data);
        free(block);
        exit(1);
    }
    memcpy(block->key, key, keyLen);

    // Allocate output buffer (worker writes XOR result here)
    block->output = (uint8_t *)malloc(keyLen);
    if(block->output == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for block output\n");
        free(block->key);
        free(block->data);
        free(block);
        exit(1);
    }

    // All allocations succeeded. Fill in metadata and init sync primitives
    block->dataLen  = bytesRead;
    block->keyLen   = keyLen;
    block->blockIdx = blockIdx;
    block->done     = 0;
    pthread_mutex_init(&block->lock, NULL);
    pthread_cond_init(&block->ready, NULL);

    return block;
}

/**********************************************************************************************
 * @name drainPendingBlocks
 * @brief Blocks until the pendingBlocks slot for blockIdx is free, writing
 *        completed blocks to stdout in order along the way.
 * @param pendingBlocks  Array of pointers to pending blocks.
 * @param capacity       Size of pendingBlocks array.
 * @param blockIdx       Index of the slot we need to free up.
 * @param nextToWrite    Pointer to the index of the next block to write.
*********************************************************************************************/
void drainPendingBlocks(Block **pendingBlocks, size_t capacity, size_t blockIdx, size_t *nextToWrite)
{
    // Local variable to hold the block being drained
    Block *block;

    // Loop until the destination slot is free (NULL means safe to reuse)
    while(pendingBlocks[blockIdx % capacity] != NULL)
    {
        // Get the next block that needs to be written to stdout
        block = pendingBlocks[*nextToWrite % capacity];

        // Block until this block is done, write it, free all resources
        writeBlock(block);

        // Clear the slot and advance the write index
        pendingBlocks[*nextToWrite % capacity] = NULL;
        (*nextToWrite)++;
    }
}

/**********************************************************************************************
 * @name processStdin
 * @brief Reads stdin in blocks, dispatches to worker threads,
 *        and writes encrypted output to stdout in order.
 * @param queue   Pointer to the shared work queue.
 * @param key     Pointer to the original encryption key.
 * @param keyLen  Length of the key in bytes (also defines block size).
*********************************************************************************************/
void processStdin(WorkQueue *queue, uint8_t *key, size_t keyLen)
{
    // Local variable to hold the queue capacity (also pendingBlocks size)
    size_t capacity;
    // Local variable to hold the array of block pointers
    Block **pendingBlocks;
    // Local variable to hold the rolling key (rotated once per block)
    uint8_t *rotatedKey;
    // Local variable to hold the index of the next block to dispatch
    size_t blockIdx;
    // Local variable to hold the index of the next block to write to stdout
    size_t nextToWrite;
    // Local variable to hold each newly created block
    Block *block;

    // Get the capacity of the queue
    capacity = queue->capacity;

    // Allocate and zero-initialize the pending block tracking array
    pendingBlocks = (Block **)calloc(capacity, sizeof(Block *));
    if(pendingBlocks == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for pending blocks\n");
        exit(1);
    }

    // Allocate and initialize the rolling key from the original key
    rotatedKey = (uint8_t *)malloc(keyLen);
    if(rotatedKey == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for rotated key\n");
        free(pendingBlocks);
        exit(1);
    }
    memcpy(rotatedKey, key, keyLen);

    // Initialize dispatch and write indices
    blockIdx    = 0;
    nextToWrite = 0;

    // Main dispatch loop: read one block at a time and hand off to workers
    while(1)
    {
        // Read next block from stdin using the current rolling key snapshot
        // Returns NULL on end of frame, nothing left to process
        block = createBlock(keyLen, rotatedKey, blockIdx);
        if(block == NULL)
        {
            break;
        }

        // Drain completed blocks until the slot for this block is free
        drainPendingBlocks(pendingBlocks, capacity, blockIdx, &nextToWrite);

        // Store block in its tracking slot
        pendingBlocks[blockIdx % capacity] = block;

        // Push block onto the queue for a worker thread to process
        workQueuePush(queue, block);

        // Rotate the key for the next block
        rotateKeyLeft(rotatedKey, keyLen);

        // Advance the block index
        blockIdx++;
    }

    // Final drain: write all remaining blocks to stdout in order
    while(nextToWrite < blockIdx)
    {
        block = pendingBlocks[nextToWrite % capacity];
        writeBlock(block);
        pendingBlocks[nextToWrite % capacity] = NULL;
        nextToWrite++;
    }

    // Free local allocations
    free(pendingBlocks);
    free(rotatedKey);

    // Signal all worker threads that no more blocks will be pushed
    pthread_mutex_lock(&queue->lock);
    queue->finished = 1;
    pthread_cond_broadcast(&queue->notEmpty);
    pthread_mutex_unlock(&queue->lock);
}

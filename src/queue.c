/**********************************************************************************************
 * @file queue.c
 * @brief Implementation of the work queue for managing blocks.
 * 
 * This file implements the functions declared in queue.h.
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/

#include "queue.h"

/**********************************************************************************************
 * @name workQueueInit
 * @brief Initializes the work queue with the specified capacity.
 * @param queue Pointer to the work queue.
 * @param capacity Capacity of the queue.
*********************************************************************************************/
void workQueueInit(WorkQueue *queue, size_t capacity)
{
    // Allocate memory for the blocks queue
    queue->blocks = (Block **)malloc(sizeof(Block *) * capacity);

    // Check if memory allocation was successful
    if(queue->blocks == NULL)
    {
        fprintf(stderr, 
                "Error: failed to allocate memory for work queue\n");
        exit(1);
    }

    // Initialize queue's head, tail, capacity, count, and finished flag
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = capacity;
    queue->count = 0;
    queue->finished = 0;

    // Initialize queue mutex and condition variables 
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->notEmpty, NULL);
    pthread_cond_init(&queue->notFull, NULL);
}

/**********************************************************************************************
 * @name workQueueDestroy
 * @brief Destroys the work queue and frees all associated memory.
 * @param queue Pointer to the work queue.
*********************************************************************************************/
void workQueueDestroy(WorkQueue *queue)
{
    // Free the memory allocated for the blocks queue
    free(queue->blocks);

    // Destroy the mutex and condition variables
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->notEmpty);
    pthread_cond_destroy(&queue->notFull);
}

/**********************************************************************************************
 * @name workQueuePush
 * @brief Pushes a block into the work queue.
 * @param queue Pointer to the work queue.
 * @param block Pointer to the block to be pushed.
*********************************************************************************************/
void workQueuePush(WorkQueue *queue, Block *block)
{
    // Lock the queue mutex to ensure thread-safe access
    pthread_mutex_lock(&queue->lock);

    // Wait until there is space in the queue to add a new block
    while(queue->count == queue->capacity)
    {
        pthread_cond_wait(&queue->notFull, &queue->lock);
    }

    // Add the block to the queue
    queue->blocks[queue->tail] = block;
    // Update the tail index to the next position
    queue->tail = (queue->tail + 1) % queue->capacity;
    // Increase count
    queue->count++;
    
    // Signal that the queue is not empty
    pthread_cond_signal(&queue->notEmpty);
    // Unlock the queue mutex
    pthread_mutex_unlock(&queue->lock);
}

/**********************************************************************************************
 * @name writeBlock
 * @brief Writes the processed block to stdout.
 * @param block Pointer to the block to be written.
*********************************************************************************************/
void writeBlock(Block *block)
{
    // Local variable to store the number of bytes written
    size_t bytesWritten;

    // Lock the block mutex to ensure thread-safe access
    pthread_mutex_lock(&block->lock);
    
    // Wait until the block is done processing
    while(!block->done)
    {
        pthread_cond_wait(&block->ready, &block->lock);
    }

    // Write the processed block to stdout and get the number of bytes written
    bytesWritten = fwrite(block->output, 1, block->dataLen, stdout);

    // Unlock the block mutex
    pthread_mutex_unlock(&block->lock);

    // Check if the write operation was successful
    if(bytesWritten != block->dataLen)
    {
        fprintf(stderr, 
                "Error: failed to write all bytes for block %lu\n", 
                (unsigned long)block->blockIdx);
        exit(1);
    }

    // Free the memory allocated for the block's data, key, and output
    free(block->data);
    free(block->key);
    free(block->output);

    // Destroy the mutex and condition variable for the block
    pthread_mutex_destroy(&block->lock);
    pthread_cond_destroy(&block->ready);

    // Free the block memory
    free(block);
}

/**********************************************************************************************
 * @name isBlockDone
 * @brief Checks if the block is done processing.
 * @param block Pointer to the block.
 * @return 1 if the block is done, 0 otherwise.
*********************************************************************************************/
int isBlockDone(Block *block)
{
    // Local variable to store current status of the block
    int done;

    // Lock the block mutex to ensure thread-safe access
    pthread_mutex_lock(&block->lock);

    // Get current status of the block
    done = block->done;

    // Unlock the block mutex
    pthread_mutex_unlock(&block->lock);

    return done;
}

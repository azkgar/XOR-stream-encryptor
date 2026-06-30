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

#include "include/queue.h"

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
    // Initialize head to 0
    queue->head = 0;
    // Initialize tail to 0
    queue->tail = 0;
    // Set the capacity of the queue
    queue->capacity = capacity;
    // Set finished flag to 0 (not finished)
    queue->finished = 0;
    // Initialize queue mutex and condition variables 
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->notEmpty, NULL);
    pthread_cond_init(&queue->notFull, NULL);
}

void workQueueDestroy(WorkQueue *queue)
{
    // Free the memory allocated for the blocks queue
    free(queue->blocks);
    // Destroy the mutex and condition variables
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->notEmpty);
    pthread_cond_destroy(&queue->notFull);
}

void workQueuePush(WorkQueue *queue, Block *block)
{
    // Lock the queue mutex to ensure thread-safe access
    pthread_mutex_lock(&queue->lock);

    // Wait until there is space in the queue to add a new block
    while((queue->tail + 1) % queue->capacity == queue->head)
    {
        pthread_cond_wait(&queue->notFull, &queue->lock);
    }
    // Add the block to the queue
    queue->blocks[queue->tail] = block;
    // Update the tail index to the next position
    queue->tail = (queue->tail + 1) % queue->capacity;
    // Signal that the queue is not empty
    pthread_cond_signal(&queue->notEmpty);
    // Unlock the queue mutex
    pthread_mutex_unlock(&queue->lock);
}

void writeBlock(Block *block)
{
    // Wait for the block to be processed
    pthread_mutex_lock(&block->lock);
    
    while(!block->done)
    {
        pthread_cond_wait(&block->ready, &block->lock);
    }

    // Write the block's output to stdout
    size_t bytesWritten = fwrite(block->output, 1, block->dataLen, stdout);
    pthread_mutex_unlock(&block->lock);

    if(bytesWritten != block->dataLen)
    {
        fprintf(stderr, 
                "Error: failed to write all bytes for block %lu\n", 
                (unsigned long)block->blockIdx);
        exit(1);
    }

    // Free block
    free(block->data);
    free(block->key);
    free(block->output);

    // Destroy the mutex and condition variable for the block
    pthread_mutex_destroy(&block->lock);
    pthread_cond_destroy(&block->ready);

    free(block);
}

int isBlockDone(Block *block)
{
    pthread_mutex_lock(&block->lock);
    int done = block->done;
    pthread_mutex_unlock(&block->lock);
    return done;
}

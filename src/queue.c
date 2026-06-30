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

void *workerThread(void *arg)
{
    WorkQueue *queue = (WorkQueue *)arg;

    while(1)
    {
        pthread_mutex_lock(&queue->lock);

        while(queue->head == queue->tail && !queue->finished)
        {
            pthread_cond_wait(&queue->notEmpty, &queue->lock);
        }

        if(queue->head == queue->tail && queue->finished)
        {
            pthread_mutex_unlock(&queue->lock);
            break;
        }

        // Pop a block from the queue
        Block *block = queue->blocks[queue->head];
        queue->head = (queue->head + 1) % queue->capacity;

        pthread_mutex_unlock(&queue->lock);

        // XOR the block against its prerotated key
        for(size_t idx = 0; idx < block->dataLen; idx++)
        {
            block->output[idx] = block->data[idx] ^ block->key[idx];
        }

        pthread_mutex_lock(&block->lock);
        block->done = 1;
        pthread_cond_signal(&block->ready);
        pthread_mutex_unlock(&block->lock);
    }

    return NULL;
}
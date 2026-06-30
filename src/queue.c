#include "include/queue.h"

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

        pthread_mutex_lock(&queue->lock);
        block->done = 1;
        pthread_cond_signal(&block->ready);
        pthread_mutex_unlock(&block->lock);
    }

    return NULL;
}
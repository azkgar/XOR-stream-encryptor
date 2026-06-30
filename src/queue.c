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

void dispatchBlocks(WorkQueue *queue, uint8_t *key, size_t keyLen)
{
    uint8_t *rotatedKey = (uint8_t *)malloc(keyLen);
    if(rotatedKey == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for rotated key\n");
        exit(1);
    }

    memcpy(rotatedKey, key, keyLen);

    size_t blockIdx = 0;

    while(1)
    {
        // malloc a buffer of keyLen bytes for block data
        Block *block = (Block *)malloc(sizeof(Block));
        if(block == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for new block\n");
            exit(1);
        }

        // fread up to keyLen bytes from stdin into the block's data
        block->data = (uint8_t *)malloc(keyLen);
        if(block->data == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for block data\n");
            exit(1);
        }

        size_t bytesRead = fread(block->data, 1, keyLen, stdin);

        if(bytesRead == 0)
        {
            // No more data to read, free the block and break the loop
            free(block->data);
            free(block);
            break;
        }

        // build a block struct
        block->dataLen = bytesRead;
        block->key = (uint8_t *)malloc(keyLen);
        if(block->key == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for block key\n");
            exit(1);
        }
        memcpy(block->key, rotatedKey, keyLen);
        block->output = (uint8_t *)malloc(keyLen);
        if(block->output == NULL)
        {
            fprintf(stderr, 
                    "Error: failed to allocate memory for block output\n");
            exit(1);
        }
        block->keyLen = keyLen;
        block->blockIdx = blockIdx;
        block->done = 0;

        // push block into queue
        workQueuePush(queue, block);

        // rotate key for next block
        rotateKeyLeft(rotatedKey, keyLen);

        // move block index to the next block
        blockIdx++;

    }

    free(rotatedKey);

    // Signal queue->finished = 1 and broadcast to all waiting threads
    pthread_mutex_lock(&queue->lock);
    queue->finished = 1;
    pthread_cond_broadcast(&queue->notEmpty);
    pthread_mutex_unlock(&queue->lock);
}

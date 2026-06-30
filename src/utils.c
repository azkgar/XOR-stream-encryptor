#include "utils.h"

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

void processStdin(WorkQueue *queue, uint8_t *key, size_t keyLen)
{
    size_t capacity = queue->capacity;

    Block **pendingBlocks = (Block **)calloc(capacity, sizeof(Block *));

    if(pendingBlocks == NULL)
    {
        fprintf(stderr, 
                "Error: failed to allocate memory for pending blocks\n");
        exit(1);
    }

    uint8_t *rotatedKey = (uint8_t *)malloc(keyLen);
    if(rotatedKey == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory for rotated key\n");
        exit(1);
    }

    memcpy(rotatedKey, key, keyLen);

    size_t blockIdx = 0;
    size_t nextToWrite = 0;

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

        pthread_mutex_init(&block->lock, NULL);
        pthread_cond_init(&block->ready, NULL);

        // store pendingBlocks[blockIdx % capacity] = block;
        pendingBlocks[blockIdx % capacity] = block;

        // push block into queue
        workQueuePush(queue, block);

        // rotate key for next block
        rotateKeyLeft(rotatedKey, keyLen);

        // move block index to the next block
        blockIdx++;

        // 8. NON-BLOCKING drain
        while(pendingBlocks[nextToWrite % capacity] != NULL)
        {
            Block *nextBlock = pendingBlocks[nextToWrite % capacity];
            pthread_mutex_lock(&nextBlock->lock);
            int done = nextBlock->done;
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
        Block *block = pendingBlocks[nextToWrite % capacity];
        writeBlock(block);   // this BLOCKS until done — that's fine here
        pendingBlocks[nextToWrite % capacity] = NULL;
        nextToWrite++;
    }

    free(pendingBlocks);
    free(rotatedKey);

    // Signal queue->finished = 1 and broadcast to all waiting threads
    pthread_mutex_lock(&queue->lock);
    queue->finished = 1;
    pthread_cond_broadcast(&queue->notEmpty);
    pthread_mutex_unlock(&queue->lock);
}

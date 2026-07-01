/**********************************************************************************************
 * @file queue.h
 * @brief defines the structures and function prototypes for the work queue and block 
 *        processing.
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/

#ifndef QUEUE_H
#define QUEUE_H

/* ======================================================================================= */
/*                                 External dependencies                                   */
/* ======================================================================================= */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/* ======================================================================================= */
/*                             Macro definitions and constants                             */
/* ======================================================================================= */

/* ======================================================================================= */
/*                           Data type, enum & struct definitions                          */
/* ======================================================================================= */

/** @brief Structure representing a block of data to be processed */

typedef struct Block {
    uint8_t             *data;          // Block plaintext
    size_t              dataLen;        // Bytes in block. (Last block may be smaller)
    uint8_t             *key;           // Block key
    size_t              keyLen;         // Bytes in key
    size_t              blockIdx;       // Index of the block in the original data
    uint8_t             *output;        // XOR result
    int                 done;           // Flag: 1 when thread finished this block, 0 otherwise
    pthread_mutex_t     lock;           // Mutex for synchronizing access to this block
    pthread_cond_t      ready;          // Condition variable to signal when block is processed

} Block;

/** @brief Structure representing the work queue */

typedef struct WorkQueue {
    Block               **blocks;       // Array of pointers to blocks
    size_t              head;           // Index of the next block to be processed
    size_t              tail;           // Index of the next free slot in the queue
    size_t              capacity;       // Maximum number of blocks the queue can hold
    size_t              count;          // Tracks slots used
    int                 finished;       // Flag: 1 when all blocks have been processed, 0 otherwise
    pthread_mutex_t     lock;           // Mutex for synchronizing access to the queue
    pthread_cond_t      notEmpty;       // Condition variable to signal when queue is not empty
    pthread_cond_t      notFull;        // Condition variable to signal when queue is not full
} WorkQueue;

/* ======================================================================================= */
/*                              Global variables declaration                               */
/* ======================================================================================= */

/* ======================================================================================= */
/*                                   Function prototypes                                   */
/* ======================================================================================= */

/**
 * @brief Initializes the work queue with the specified capacity.
 * @param queue Pointer to the WorkQueue structure to initialize.
 * @param capacity Maximum number of blocks the queue can hold.
 */
void workQueueInit(WorkQueue *queue, size_t capacity);

/**
 * @brief Destroys the work queue and frees associated resources.
 * @param queue Pointer to the WorkQueue structure to destroy.
 */
void workQueueDestroy(WorkQueue *queue);

/**
 * @brief Pushes a block onto the work queue.
 * @param queue Pointer to the WorkQueue structure where the block will be pushed.
 * @param block Pointer to the Block structure to push.
 */
void workQueuePush(WorkQueue *queue, Block *block);

/**
 * @brief Writes the processed block to the output.
 * @param block Pointer to the Block structure to write.
 */
void writeBlock(Block *block);

#endif /* QUEUE_H */

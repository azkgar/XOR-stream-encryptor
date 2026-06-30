/**
 * Defines structures for thread safe queue implementation.
 */

 /********************************************
 * @file main.h
 * @brief Header file for main.c
*********************************************/

#ifndef QUEUE_H
#define QUEUE_H

/* External dependencies */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

/* Macro definitions and constants */

/* Data type, enum & struct definitions */

typedef struct Block {
    uint8_t             *data;          // Block plaintext
    size_t              dataLen;        // Bytes in block. (Last block may be smaller)
    uint8_t             *key;           // Block key
    size_t              keyLen;
    size_t              blockIdx;
    uint8_t             *output;        // XOR result
    int                 done;           // Flag: 1 when thread finished this block, 0 otherwise
    pthread_mutex_t     lock;           // Mutex for synchronizing access to this block
    pthread_cond_t      ready;          // Condition variable to signal when block is processed

} Block;

typedef struct WorkQueue {
    Block               **blocks;       // Array of pointers to blocks
    size_t              head;
    size_t              tail;
    size_t              capacity;
    int                 finished;
    pthread_mutex_t     lock;
    pthread_cond_t      notEmpty;
    pthread_cond_t      notFull;
} WorkQueue;

/* Global variables declaration */

/* Function prototypes */

#endif /* QUEUE_H */
/**********************************************************************************************
 * @file utils.h
 * @brief defines the structures and function prototypes for the utility functions.
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/

#ifndef UTILS_H
#define UTILS_H

/* ======================================================================================= */
/*                                 External dependencies                                   */
/* ======================================================================================= */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "queue.h"

/* ======================================================================================= */
/*                             Macro definitions and constants                             */
/* ======================================================================================= */

/* ======================================================================================= */
/*                           Data type, enum & struct definitions                          */
/* ======================================================================================= */

/* ======================================================================================= */
/*                              Global variables declaration                               */
/* ======================================================================================= */

/* ======================================================================================= */
/*                                   Function prototypes                                   */
/* ======================================================================================= */

/**
 * @brief Worker thread function that processes blocks from the work queue.
 * @param context Pointer to the context.
 */
void *workerThread(void *context);

/**
 * @brief Allocates and initializes a Block struct with data read from stdin.
 * @param keyLen Size of the key in bytes (also the block size).
 * @param key PPointer to the rotated key for this block.
 * @param blockIdx Index of this block in the input stream.
 * @return Pointer to a fully initialized Block, or NULL on EOF (0 bytes read).
 */
Block *createBlock(size_t keyLen, uint8_t *key, size_t blockIdx);

/**
 * @brief Blocks until the pendingBlocks slot for blockIdx is free, writing
 *        completed blocks to stdout in order along the way.
 * @param pendingBlocks Array of pointers to pending blocks.
 * @param capacity Size of pendingBlocks array.
 * @param blockIdx Index of the slot we need to free up.
 * @param nextToWrite Pointer to the index of the next block to write.
 */
void drainPendingBlocks(Block **pendingBlocks, size_t capacity, size_t blockIdx, size_t *nextToWrite);

/**
 * @brief Reads stdin in blocks, dispatches to worker threads,
 *        and writes encrypted output to stdout in order.
 * @param queue Pointer to the shared work queue.
 * @param key Pointer to the original encryption key.
 * @param keyLen Length of the key in bytes (also defines block size).
 */
void processStdin(WorkQueue *queue, uint8_t *key, size_t keyLen);



#endif /* UTILS_H */

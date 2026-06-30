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
 * @brief Processes the standard input stream, reading data in blocks, rotating the key, and pushing blocks into the work queue.
 * @param queue Pointer to the work queue where blocks will be pushed.
 * @param key Pointer to the initial key used for XOR operations.
 * @param keyLen Length of the key in bytes.
 */
void processStdin(WorkQueue *queue, uint8_t *key, size_t keyLen);



#endif /* UTILS_H */

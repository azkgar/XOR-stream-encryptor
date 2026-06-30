/**********************************************************************************************
 * @file crypto.h
 * @brief defines the structures and function prototypes for the bitwise rotation.
 * 
 * @author Azkary Garcia
 * @date June 30th, 2026
 * @version 1.0.0
*********************************************************************************************/

#ifndef CRYPTO_H
#define CRYPTO_H

/* ======================================================================================= */
/*                                 External dependencies                                   */
/* ======================================================================================= */
#include <stdint.h>
#include <stddef.h>

/* ======================================================================================= */
/*                             Macro definitions and constants                             */
/* ======================================================================================= */
/** @brief The number of bits to rotate the key */
#define     BIT_KEY_ROTATION        1u    
/** @brief A mask for extracting the least significant bit */
#define     LSB_MASK                0x01u

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
 * @brief Rotates the key to the left by one bit.
 * @param key Pointer to the key buffer.
 * @param keyLen Length of the key buffer.
 */
void rotateKeyLeft(uint8_t *key, size_t keyLen);

#endif /* CRYPTO_H */

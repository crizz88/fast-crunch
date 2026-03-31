/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef BITSET_H
#define BITSET_H

#include <stdint.h>
#include <stdatomic.h>
#include "common.h"

/**
  * A cacheline of the bitset for uniqueness and duplicates
  *
  */
typedef struct {
    _Atomic uint64_t occupied[4];
    _Atomic uint64_t isDuplicate[4];
} __attribute__((aligned(CL_SIZE))) Bucket;


static Bucket *bitset;

extern void bitset_init();
extern size_t bitset_put(uint32_t *p, size_t actual_bytes);
extern void bitset_get_results(uint64_t *unique, uint64_t *only_once);

#endif

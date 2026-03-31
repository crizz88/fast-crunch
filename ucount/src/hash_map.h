/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef HASH_MAP_H
#define HASH_MAP_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <immintrin.h>
#include <stdint.h>
#include <stdatomic.h>
#include "common.h"

/**
  * Buckets and linked list bucket extends of hash map with size of cacheline.
  *
  */
typedef struct {
    _Atomic(struct Extend*) next;		// next pointer
    _Atomic uint_least16_t occupied;		// indicate occupied / free slot, also locking bit for modification
    _Atomic uint_least16_t isDuplicate;		// indicate duplicates
    uint32_t data[13];				// store keys in the remaining space
} __attribute__((aligned(CL_SIZE))) Extend;

/**
  * Dynamic hash map struct
  *
  */
typedef struct {
    __m256i v_prime;				// vectorized hash
    __m256i v_mask;				// vectorized hash
    size_t map_mask;				// number of buckets - 1
    size_t num_buckets;				// number of buckets
    Extend *bucket;				// buckets
} HashMap;


static HashMap map;

extern void hash_map_init(size_t num_buckets);
extern void hash_map_put(uint32_t *p, size_t actual_bytes);
extern void hash_map_get_results(uint64_t *unique, uint64_t *only_once);
extern void hash_map_get_results_range(size_t start, size_t end, uint64_t *unique, uint64_t *only_once);

#endif

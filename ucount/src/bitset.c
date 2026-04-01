/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include <sys/mman.h>
#include "bitset.h"

#define SHIFT_BIT 1ULL
#define VEC CL_SIZE / sizeof(uint32_t) // typically 8
#define BUCKET_INDEX_SHIFT 8
#define NUM_BUCKETS (SHIFT_BIT << (32 - BUCKET_INDEX_SHIFT))  // all uint32_t / 8 bits / 4 64-bit slots per bucket
#define SLOT_IDX_MASK 0x03
#define UINT64_MASK 0x3F


/**
  * Some inlined calculation functions
  *
  */

static inline uint32_t get_bucket_index(uint32_t p) {
    return p >> BUCKET_INDEX_SHIFT;
}

static inline void bucket_insert(uint32_t bucket_idx, uint32_t val) {
    int b_slot_idx = (val >> 6) & SLOT_IDX_MASK;
    uint64_t bitmask = SHIFT_BIT << (val & UINT64_MASK);
    // optimistic atomic put
    if (!(atomic_load_explicit(&bitset[bucket_idx].occupied[b_slot_idx], memory_order_relaxed) & bitmask)) {
        if (atomic_fetch_or_explicit(&bitset[bucket_idx].occupied[b_slot_idx], bitmask, memory_order_relaxed) & bitmask) {
            atomic_fetch_or_explicit(&bitset[bucket_idx].isDuplicate[b_slot_idx], bitmask, memory_order_relaxed);
        }
    } else {
        atomic_fetch_or_explicit(&bitset[bucket_idx].isDuplicate[b_slot_idx], bitmask, memory_order_relaxed);
    }
}

/**
  * Public part
  *
  */


/**
  * Initialize bitmap with mmap ~ 1GB in size.
  *
  */
void bitset_init() {
    size_t bitset_size = (size_t) NUM_BUCKETS * sizeof(Bucket);
    bitset = mmap(NULL, bitset_size, PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (bitset == MAP_FAILED) {
        // Fallback
        bitset = mmap(NULL, bitset_size, PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }
    // better do it now:
    memset(bitset, 0, bitset_size);
    static_assert(atomic_is_lock_free(&bitset[0].occupied[0]));
    static_assert(atomic_is_lock_free(&bitset[0].isDuplicate[0]));
}


/**
  * Put records in map. Includes vectorized calculation of bucket index prefetching.
  *
  */
size_t bitset_put(uint32_t *p, size_t actual_bytes) {
    const size_t n_elements = actual_bytes / sizeof(uint32_t);  //

    uint32_t indices_a[VEC] __attribute__((aligned(32)));
    uint32_t indices_b[VEC] __attribute__((aligned(32)));
    uint32_t *curr_ptr = indices_a;
    uint32_t *next_ptr = indices_b;
    size_t i = 0;

    // 1. Prefetch buckets 0-7
    if (n_elements >= VEC) {
        for (int k = 0; k < VEC; k++) {
            curr_ptr[k] = get_bucket_index(p[k]);
            __builtin_prefetch(&bitset[curr_ptr[k]], 1, 3);
        }
        i = VEC;
    }
    // 2. Store previous buckets and prefetch next
    for (; i + VEC <= n_elements; i += VEC) {
        // calculate and prefetch next
        for (int k = 0; k < VEC; k++) {
            next_ptr[k] = get_bucket_index(p[i + k]);
            __builtin_prefetch(&bitset[next_ptr[k]], 1, 3);
        }
        // process current
        for (int k = 0; k < VEC; k++) {
            bucket_insert(curr_ptr[k], p[i - VEC + k]);
        }
        // swap next -> current without mem copy
        uint32_t *tmp = curr_ptr;
        curr_ptr = next_ptr;
        next_ptr = tmp;
    }
    // 3. Store last round
    if (i >= VEC) {
        for (int k = 0; k < VEC; k++) {
            bucket_insert(curr_ptr[k], p[i - VEC + k]);
        }
    }
    // 4. corner case: less remaining keys than batch
    for (; i < n_elements; i++) {
        bucket_insert(get_bucket_index(p[i]), p[i]);
    }
    return n_elements;
}

/**
  * Result calculation. 'Only Once' means 'Unique' - 'Duplicates'.
  *
  */
void bitset_get_results(uint64_t *unique_count, uint64_t *only_once_count) {
    uint64_t total_unique = 0;
    uint64_t total_once = 0;

    for (size_t i = 0; i < NUM_BUCKETS; i++) {
        for (int j = 0; j < 4; j++) {
            uint64_t occ = atomic_load_explicit(&bitset[i].occupied[j], memory_order_relaxed);
            uint64_t dup = atomic_load_explicit(&bitset[i].isDuplicate[j], memory_order_relaxed);

            total_unique += __builtin_popcountll(occ);
            total_once += __builtin_popcountll(occ & ~dup);
        }
    }
    *unique_count = total_unique;
    *only_once_count = total_once;
}

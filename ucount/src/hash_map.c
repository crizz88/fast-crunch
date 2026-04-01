/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include <sys/mman.h>
#include "hash_map.h"

const uint32_t HASH_PRIME = 0x45d9f3b;
#define LOCK_BIT 0x8000
#define VMASK 0x1FFF
#define VECTORIZED 8

const int vec = VECTORIZED / sizeof(uint32_t);

/**
  * Initialize dynamic hash map.
  *
  */
void hash_map_init(size_t num_buckets) {
    map.num_buckets = num_buckets;
    map.map_mask = num_buckets - 1;
    map.v_prime = _mm256_set1_epi32((int)HASH_PRIME);
    map.v_mask  = _mm256_set1_epi32((int)map.map_mask);

    size_t map_size = (size_t)num_buckets * sizeof(Extend);
    map.bucket = mmap(NULL, map_size, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (map.bucket == MAP_FAILED) {
    // Fallback
    map.bucket = mmap(NULL, map_size, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
}

/**
  * Scalar hash function
  *
  */
static inline uint32_t get_hash_scalar(uint32_t val) {
    uint32_t hashed = val * HASH_PRIME;
    uint32_t shifted = hashed >> 16;
    hashed = hashed ^ shifted;
    return hashed & map.map_mask;
}

/**
  * Vectorized hash function
  *
  */
static inline __m256i get_hash_vectorized(uint32_t *p, __m256i v_prime, __m256i v_mask) {
    __m256i v_data    = _mm256_load_si256((__m256i*) p);
    __m256i v_hashed  = _mm256_mullo_epi32(v_data, v_prime);
    __m256i v_shifted = _mm256_srli_epi32(v_hashed, 16); 
    v_hashed = _mm256_xor_si256(v_hashed, v_shifted);
    return _mm256_and_si256(v_hashed, v_mask);
}





/**
  * Vectorized search for slot with equal key value.
  *
  */
static int find_slot(Extend *e, uint32_t val, uint16_t occ_clean) {
    __m256i v_search = _mm256_set1_epi32(val);

    // Slots 0-7
    __m256i v_data1 = _mm256_loadu_si256((__m256i*)&e->data[0]);
    int mask1 = _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(v_data1, v_search)));
    mask1 &= (occ_clean & 0xFF); 
    if (mask1) return __builtin_ctz(mask1);

    // Slots 8-11
    __m128i v_data2 = _mm_loadu_si128((__m128i*)&e->data[8]);
    int mask2 = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(v_data2, _mm_set1_epi32(val))));
    mask2 &= ((occ_clean >> 8) & 0xF);
    if (mask2) return 8 + __builtin_ctz(mask2);

    // Slot 12
    if ((occ_clean & (1u << 12)) && e->data[12] == val) return 12;

    return RET_ERR;
}


/**
  * Insert a key in the bucket of a hash map. Extends of cache line size are created dynamically.
  * Occupied slots and duplicates are indicated by separate vectors.
  */
static void bucket_insert(uint32_t bucket_idx, uint32_t val) {
    Extend *curr = &map.bucket[bucket_idx];

    while (curr) {
        // paranoid acquire
        uint16_t occ = atomic_load_explicit(&curr->occupied, memory_order_acquire);

    // extra lock bit check for save reader mask
        if (occ & LOCK_BIT) {
            __builtin_ia32_pause();
            continue; 
        }

        // search slot with mask
        int slot = find_slot(curr, val, occ & VMASK);
        if (slot != RET_ERR) {
            atomic_fetch_or_explicit(&curr->isDuplicate, (1u << slot), memory_order_relaxed);
            return;
        }

        // try insert
        uint16_t clean_occ = occ & VMASK;
        if (clean_occ != VMASK) {
            uint16_t expected = clean_occ; // assume LOCK_BIT == 0
            if (atomic_compare_exchange_weak_explicit(
                    &curr->occupied, &expected, expected | LOCK_BIT,
                    memory_order_acq_rel, memory_order_acquire)) {
                // --- CRITICAL ---
                // check for concurrent update
                int retry_slot = find_slot(curr, val, expected);
                if (retry_slot == RET_ERR) {
                    // we won, let's do work
                    int free_bit = __builtin_ctz(~expected & VMASK);
                    curr->data[free_bit] = val;

                    // memory barrier and release.
                    atomic_thread_fence(memory_order_release);
                    atomic_store_explicit(&curr->occupied, expected | (1u << free_bit), memory_order_release);
                    return;
                } else {
                    // somebody else inserted equal key, set duplicate
                    if (!(atomic_load_explicit(&curr->isDuplicate, memory_order_relaxed) & (1u << slot))) {
                        atomic_fetch_or_explicit(&curr->isDuplicate, (1u << retry_slot), memory_order_relaxed);
                    }
                    atomic_fetch_and_explicit(&curr->occupied, ~LOCK_BIT, memory_order_release);
                    return;
                }
            }
            __builtin_ia32_pause();
            continue; 
        }

        // linked list bucket extends
        Extend* next = (Extend*)atomic_load_explicit(&curr->next, memory_order_acquire);
        if (!next) {
            // prepare unlinked extend with data
            Extend* new_bucket = calloc(1, sizeof(Extend));
            new_bucket->data[0] = val;
            new_bucket->occupied = 1u;

            Extend* expected_next = NULL;
            if (atomic_compare_exchange_strong_explicit((_Atomic uintptr_t*)&curr->next, 
                                                       (uintptr_t*)&expected_next, (uintptr_t)new_bucket,
                                                       memory_order_acq_rel, memory_order_acquire)) {
                return;
            } else {
                // others were faster, insert in other extend
                free(new_bucket);
                curr = (Extend*)expected_next;
                continue;
            }
        }
        curr = next;
    }
}

/**
  * Put records in map. Includes vectorized calculation of bucket index prefetching.
  *
  */
void hash_map_put(uint32_t *p, size_t actual_bytes) {
    const size_t n_elements = actual_bytes / sizeof(uint32_t);

    uint32_t indices_a[8] __attribute__((aligned(32)));
    uint32_t indices_b[8] __attribute__((aligned(32)));
    uint32_t *curr_ptr = indices_a;
    uint32_t *next_ptr = indices_b;
    size_t i = 0;

    // 1. Prefetch buckets 0-7
    if (n_elements >= vec) {
        __m256i v_idx = get_hash_vectorized(&p[0], map.v_prime, map.v_mask);
        _mm256_store_si256((__m256i*)curr_ptr, v_idx);
        for (int k = 0; k < vec; k++) {
            __builtin_prefetch(&map.bucket[curr_ptr[k]], 1, 3);
        }
        i = vec;
    }
    // 2. Store previous buckets and prefetch next
    for (; i + vec <= n_elements; i += vec) {
        // calculate and prefetch next
        __m256i v_next = get_hash_vectorized(&p[i], map.v_prime, map.v_mask);
        _mm256_store_si256((__m256i*)next_ptr, v_next);
        for (int k = 0; k < vec; k++) {
            __builtin_prefetch(&map.bucket[next_ptr[k]], 1, 3);
        }
        // insert current
        for (int k = 0; k < vec; k++) {
            bucket_insert(curr_ptr[k], p[i - vec + k]);
        }
        // swap next -> current
        uint32_t *tmp = curr_ptr;
        curr_ptr = next_ptr;
        next_ptr = tmp;
    }
    // 3. Store last round
    if (i >= vec) {
        for (int k = 0; k < vec; k++) {
            bucket_insert(curr_ptr[k], p[i - vec + k]);
        }
    }

    // 4. Scalar corner cases
    for (; i < n_elements; i++) {
        bucket_insert(get_hash_scalar(p[i]), p[i]);
    }
}


/**
  * Single threaded result calculation
  *
  */
void hash_map_get_results(uint64_t *unique, uint64_t *only_once) {
    hash_map_get_results_range(0, map.num_buckets, unique, only_once);
}

/**
  * Multi threaded result calculation
  *
  */
void hash_map_get_results_range(size_t start, size_t end, uint64_t *unique, uint64_t *only_once) {
    uint64_t u = 0, o = 0;
    for (size_t i = start; i < end; i++) {
        Extend *curr = &map.bucket[i];
        while (curr) {
            uint16_t occ_val = atomic_load_explicit(&curr->occupied, memory_order_relaxed) & VMASK;
            uint16_t dup_val = atomic_load_explicit(&curr->isDuplicate, memory_order_relaxed); 
            u += __builtin_popcount(occ_val);
            o += __builtin_popcount(occ_val & ~dup_val);
            curr = (Extend*)atomic_load_explicit(&curr->next, memory_order_relaxed);
        }
    }
    *unique = u;
    *only_once = o;
}

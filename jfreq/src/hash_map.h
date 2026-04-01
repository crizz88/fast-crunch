/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdint.h>
#include <stdatomic.h>
#include "common.h"

#define SLOTS_PER_EXTEND 4

/**
  * Attribute string.
  *
  */
typedef struct {
    size_t size;
    char *data;
} AttributeLabel;

/**
  * Hash map entry of attribute string. 12 byte -> 4 per cacheline
  *
  */
typedef struct {
    uint32_t hash;                  // calculated hash
    _Atomic uint32_t count;         // atomic counter
    uint32_t external_storage_offset;   // copy of attribute string for comparison
} HashEntry;

/**
  * Buckets and linked list bucket extends of hash map with size of cacheline.
  *
  */
typedef struct Extend {
    _Atomic(struct Extend*) next;       // next pointer
    _Atomic uint_least8_t occupied;     // indicate occupied / free slot, also locking bit for modification
    uint8_t padding[7];             // indicate duplicates
    HashEntry data[SLOTS_PER_EXTEND];       // store attribute-labels in the remaining space
} __attribute__((aligned(CL_SIZE))) Extend;

/**
  * Dynamic hash map struct
  *
  */
typedef struct {
    size_t external_storage_size;       // size of external storage for strings
    _Atomic size_t external_storage_offset; // offset for insert position
    char *external_storage;         // external storage area
    size_t map_mask;                            // number of buckets - 1
    size_t num_buckets;             // number of buckets
    Extend *bucket;             // buckets
} HashMap;

/**
  * Result set.
  *
  */
typedef struct {
    AttributeLabel attr;
    uint32_t count;
} AttributeResultCount;

/**
  * use an iterator pattern for output
  *
  */
typedef struct {
    size_t current_bucket;
    uint8_t current_slot;
    Extend *current_extend;
} HashMapCursor;

static HashMap map;

extern void hash_map_init(size_t num_buckets, size_t external_storage_size);
extern void hash_map_put_or_increment(char *p, size_t actual_bytes);
extern HashMapCursor init_hash_map_cursor();
extern AttributeResultCount *hash_map_next(HashMapCursor *cursor);

#endif

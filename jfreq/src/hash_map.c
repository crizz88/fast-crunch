/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include <sys/mman.h>
#include "hash_map.h"

static atomic_flag ext_lock = ATOMIC_FLAG_INIT;
const uint32_t HASH_PRIME = 0x45d9f3b;
#define OMASK (uint8_t)(0x0F)
#define VMASK (uint8_t)(0xF0)


/**
  * First some static inline stuff
  *
  */


/**
 * FNV-1a hash for AttributeLabel
 */
static inline uint32_t fn1a_hash(AttributeLabel *label) {
    uint32_t hash = HASH_PRIME;
    for (size_t i = 0; i < label->size; i++) {
        hash ^= (uint8_t)label->data[i];
        hash *= 16777619u;
    }
    return hash;
}



/**
  * Compare strings of attribute labels
  *
  */
static inline bool compare_attribute_labels(AttributeLabel *a, AttributeLabel *b) {
    if (a->size != b->size) return false;
    return memcmp(a->data, b->data, b->size) == 0;
}

/**
  * Get attribute label from external storage.
  *
  */
static inline AttributeLabel hash_map_get_external(uint32_t offset) {
    AttributeLabel res;
    res.size = *(uint32_t*) (map.external_storage + offset);
    res.data = map.external_storage + offset + sizeof(uint32_t);
    return res;
}



static inline void ensure_external_space(size_t off) {
    if (off < map.external_storage_size) return;

    while (atomic_flag_test_and_set(&ext_lock));
    size_t old_size = map.external_storage_size;
    if (off >= old_size) {
        size_t new_size = old_size * 2;
        while (off > new_size) new_size *= 2;
        mprotect(map.external_storage, new_size, PROT_READ | PROT_WRITE);
        map.external_storage_size = new_size;
    }
    atomic_flag_clear(&ext_lock);
}


/**
  * external storage in dedicated storage area
  *
  */
static inline uint32_t storeExternal(AttributeLabel *label) {
    // 4 byte size + attribute label of size length
    size_t total_needed = sizeof(uint32_t) + label->size;
    // reserve required space at current offset atomically
    size_t off = atomic_fetch_add_explicit(&map.external_storage_offset, 
                                           total_needed, 
                                           memory_order_relaxed);
    ensure_external_space(map.external_storage_offset);

    uint8_t *base = atomic_load_explicit(&map.external_storage, memory_order_acquire);

    *(uint32_t*)(base + off) = (uint32_t)label->size;
    char *dest_str = base + off + sizeof(uint32_t);
    memcpy(dest_str, label->data, label->size);
    return (uint32_t)off;
}





/**
  * Search for slot with equal attribute label.
  *
  */
static inline int find_slot(Extend *e, AttributeLabel *label, uint32_t hash, uint8_t occ_clean) {
    for (int i = 0; i < SLOTS_PER_EXTEND; i++) {
        if ((occ_clean & (1 << i)) && 
            (e->data[i].hash == hash)) {
            AttributeLabel ext = hash_map_get_external(
                            e->data[i].external_storage_offset);
            if (compare_attribute_labels(&ext, label))
                return i;
        }
    }
    return RET_ERR;
}


/**
  * Return stored label or NULL if not found
  *
  */
static inline HashEntry *seek_label(AttributeLabel *label, uint32_t hash, Extend **bucket) {
    uint8_t occ_clean;
    Extend *curr = &map.bucket[hash & map.map_mask];
    *bucket = curr;
    occ_clean = atomic_load_explicit(&curr->occupied, memory_order_acquire) & (OMASK | VMASK);

    while (curr) {
        uint16_t slot;
        if ((slot = find_slot(curr, label, hash, occ_clean)) != RET_ERR) {
            return &curr->data[slot];
        }
        // try next extend
        curr = (Extend*) atomic_load_explicit(&curr->next, memory_order_acquire);
    }
    return NULL;
}



/**
  * Increment attribute label counter atomically
  *
  */
static inline void bucket_entry_increment(HashEntry *entry) {
    atomic_fetch_add(&entry->count, 1);
}



/**
  * Insert a hash in the bucket of a hash map. Extends of cache line size are created dynamically.
  * Occupied slots are indicated by separate vector.
  */
static void bucket_insert(AttributeLabel *label, uint32_t hash, Extend *bucket) {
    Extend *curr = bucket;
    while (curr) {
        int free_bit = -1;
        uint8_t occ = atomic_load_explicit(&curr->occupied, memory_order_acquire);

        // first, search, if the entry already exist and increment
        uint16_t slot = find_slot(curr, label, hash, occ);
        if (slot != RET_ERR) {
            bucket_entry_increment(&curr->data[slot]);
            return;
        }

        // check for free slot
        uint8_t mask = ~occ & OMASK;
        if (mask != 0) {
            free_bit = __builtin_ctz((uint8_t)(~occ & OMASK));

            // reserve space 
            uint8_t expected = occ;
            if (!atomic_compare_exchange_strong(&curr->occupied, &expected, occ | (1u << free_bit)))
            continue; // someone else was faster, retry

            // insert key values
            curr->data[free_bit].hash = hash;
            curr->data[free_bit].external_storage_offset = storeExternal(label);


            occ = expected |= (1u << free_bit);
            if (!atomic_compare_exchange_strong(&curr->occupied, &expected, occ | (1u << (free_bit + SLOTS_PER_EXTEND)))) {
                // re-check for concurrent insertions
                Extend *retry_bucket;
                HashEntry *retry_entry = seek_label(label, hash, &retry_bucket);
                if ((retry_entry != NULL) && (retry_entry->external_storage_offset != curr->data[free_bit].external_storage_offset)) {
                    // rollback and increment
                    atomic_fetch_and_explicit(&curr->occupied, ~(1u << (free_bit)), memory_order_release);
                    bucket_entry_increment(retry_entry);
                return;
            } else {
                // make visible
                atomic_fetch_or_explicit(&curr->occupied, 1u << (free_bit + SLOTS_PER_EXTEND), memory_order_release);
            }
        }
        bucket_entry_increment(&curr->data[free_bit]);
        return;
    }

        // key not found, no space for insert, check for extend
        Extend* next = (Extend*)atomic_load_explicit(&curr->next, memory_order_acquire);
        if (!next) {
        // prepare unlinked extend with data
            Extend* new_bucket = calloc(1, sizeof(Extend));

            Extend* expected_next = NULL;
            if (atomic_compare_exchange_strong_explicit(&curr->next, 
                                            &expected_next, new_bucket,
                                            memory_order_acq_rel, memory_order_acquire)) {
                curr = new_bucket;
            } else {
                // others were faster, insert in other extend
                free(new_bucket);
                curr = expected_next;
            }
        continue;
        }
        curr = next;
    }
}



/**
  * public interfaces
  *
  */


/**
  * Initialize dynamic hash map with external storage area.
  *
  */
void hash_map_init(size_t num_buckets, size_t external_storage_size) {
    map.num_buckets = num_buckets;
    map.map_mask = num_buckets - 1;

    size_t map_size = (size_t)num_buckets * sizeof(Extend);
    map.bucket = mmap(NULL, map_size, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (map.bucket == MAP_FAILED) {
    // Fallback
        map.bucket = mmap(NULL, map_size, PROT_READ | PROT_WRITE, 
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    atomic_store_explicit(&map.external_storage_offset, 0, memory_order_relaxed);


    map.external_storage_size = external_storage_size;
    map.external_storage = mmap(NULL, 1 << 30, PROT_NONE, 
                      MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (map.external_storage == MAP_FAILED) {
        // Fallback
        map.external_storage = mmap(NULL, 1 << 30, PROT_NONE, 
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }
    mprotect(map.external_storage, map.external_storage_size, PROT_READ | PROT_WRITE);
}


/**
  * Put attribute labels in map.
  *
  */
void hash_map_put_or_increment(char *p, size_t actual_bytes) {
    Extend *bucket;
    AttributeLabel label;
    label.size = actual_bytes;
    label.data = p;

    uint32_t hash = fn1a_hash(&label);

    HashEntry *entry = seek_label(&label, hash, &bucket);

    // check if attribute already exist in hash map
    if (entry == NULL) {
        // if not, insert it
        bucket_insert(&label, hash, bucket);
    } else {
        // increment count
        bucket_entry_increment(entry);
    }
}


/**
  * Initialize cursor and position at first slot
  *
  */
HashMapCursor init_hash_map_cursor() {
    HashMapCursor cursor = {0};
    if (map.num_buckets > 0) {
        cursor.current_bucket = 0;
        cursor.current_slot = 0;
        cursor.current_extend = NULL;
    }
    return cursor;
}

/**
  * return next attribute and result count
  *
  */
AttributeResultCount* hash_map_next(HashMapCursor *cursor) {
    while (cursor->current_bucket < map.num_buckets) {
        if (!cursor->current_extend) {
            cursor->current_extend = &map.bucket[cursor->current_bucket];
            cursor->current_slot = 0;
        }

        Extend *curr = cursor->current_extend;
        uint8_t occ = atomic_load_explicit(&curr->occupied, memory_order_relaxed) & (OMASK | VMASK);

        // next inside extend
        while (cursor->current_slot < SLOTS_PER_EXTEND) {
            int s = cursor->current_slot++;
            if (occ & (1 << s)) {
                static AttributeResultCount res;
                res.attr = hash_map_get_external(curr->data[s].external_storage_offset);
                res.count = atomic_load_explicit(&curr->data[s].count, memory_order_relaxed);
                return &res;
            }
        }

        // next extend (linked or slot)
        Extend *next = (Extend*) atomic_load_explicit(&curr->next, memory_order_relaxed);
        if (next) {
            cursor->current_extend = next;
            cursor->current_slot = 0;
        } else {
            cursor->current_bucket++;
            cursor->current_extend = NULL;
        }
    }
    return NULL; // finished
}

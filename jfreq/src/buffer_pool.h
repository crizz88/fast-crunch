/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stdatomic.h>
#include <stdint.h>
#include "common.h"

#ifndef POOL_SIZE
#define POOL_SIZE 14
#endif

#define SLOT_EMPTY 0
#define SLOT_FULL 1
#define SLOT_BUSY 2
#define SLOT_FINISH 3


/**
  * Buffer length and slot status cache-aligned
  *
  */
typedef struct __attribute__((aligned(CL_SIZE))) {
    size_t len;
    atomic_int status;
} BufStatPadding;

/**
  * Buffer pool, filled by readers and consumed by counters.
  *
  */
typedef struct {
    BufStatPadding misc[POOL_SIZE];
    atomic_bool finished;
    char __attribute__((aligned(FIO_SIZE))) data[POOL_SIZE][FIO_SIZE];
} BufferPool;

extern BufferPool pool;
//extern atomic_bool finished;


void setFinished();

// Keep it fast and simple and fast with static inlines.

static inline bool isFinished() {
    return pool.finished;
}

static inline uint16_t getSlotByFlag(uint32_t flag, uint16_t start) {
    uint16_t slot = start;
    uint32_t poll_count = 0;
    while(!isFinished()) {
        if(atomic_load(&(pool.misc[slot].status))==flag) return slot;
        slot = (slot + 1) % (POOL_SIZE);
        checked_polling(&poll_count, 1);
    }
    return RET_ERR;
}


static inline uint16_t getEmptySlot(uint16_t start) {
    return getSlotByFlag(SLOT_EMPTY, start);
}


static inline uint16_t getFilledSlot(uint16_t start) {
    return getSlotByFlag(SLOT_FULL, start);
}


static inline char *getBufferData(uint16_t slot) {
    return &(pool.data[slot][0]);
}

static inline size_t *getBufferLen(uint16_t slot) {
    return &(pool.misc[slot].len);
}

static inline atomic_int *getBufferStatus(uint16_t slot) {
    return &(pool.misc[slot].status);
}

static inline bool setBufferStatus(uint16_t slot, atomic_int expected_status, atomic_int new_status) {
    return atomic_compare_exchange_strong(&(pool.misc[slot].status), &expected_status, new_status);
}

static inline void setBufferLen(uint16_t slot, size_t len) {
    pool.misc[slot].len = len;
}



#endif

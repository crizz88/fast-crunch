/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include "buffer_pool.h"

BufferPool pool __attribute__((aligned(FIO_SIZE))) = {0};

/**
  * After reading EOF, finish buffer pool slots, that counter worker can terminate
  *
  */
void setFinished() {
   uint16_t slot;
   size_t finishedSlots = 0;
    while((finishedSlots < POOL_SIZE) 
		&& (slot = getEmptySlot(0))!=RET_ERR) {
        atomic_store(&(pool.misc[slot].status), SLOT_FINISH);
	finishedSlots++;
	__builtin_ia32_pause();
    }
    atomic_store(&(pool.finished), true);
}


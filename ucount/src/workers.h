/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef WORKERS_H
#define WORKERS_H
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#ifndef N_READERS
#define N_READERS 1             // better lower for HDD, higher for SSD
#endif
#ifndef N_COUNTERS
#define N_COUNTERS 3        // rest of cores
#endif


/**
  * Args of worker: READERS
  *
  */
typedef struct {
    char* path;
    off_t start_offset;
    off_t end_offset;
    uint16_t slot;
} ReaderArg;

typedef struct {
    size_t n_total;
    uint16_t slot;
} CounterArg;


extern void initReaderParam(ReaderArg* ra, void* arg, int id);

void* worker_read(void* arg);
void* worker_count(void* arg);


#endif

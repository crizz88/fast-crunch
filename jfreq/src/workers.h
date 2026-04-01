/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef WORKERS_H
#define WORKERS_H
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>


#ifndef MAX_ATTRIBUTE_STR_LEN
#define MAX_ATTRIBUTE_STR_LEN 60
#endif
#ifndef MAX_ATTRIBUTE_LABEL_LEN
#define MAX_ATTRIBUTE_LABEL_LEN 20
#endif
#ifndef N_READERS
#define N_READERS 1             // 1 for streaming and unaligned data
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
    bool json_stdin;
    off_t start_offset;
    off_t end_offset;
    uint16_t slot;
} ReaderArg;

typedef struct {
    char label[MAX_ATTRIBUTE_LABEL_LEN];
    uint16_t label_length;
    uint16_t slot;
} CounterArg;

extern void initReaderParam(ReaderArg* ra, void* arg, int id);
extern void initCounterParam(CounterArg* ca, void* arg, int id);

void* worker_read(void* arg);
void* worker_count(void* arg);


#endif

/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include "common.h"
#include "workers.h"
#include "buffer_pool.h"
#include "hash_map.h"


#ifndef EXTERNAL_STORAGE_SIZE
#define EXTERNAL_STORAGE_SIZE 1024
#endif
#ifndef HASH_MAP_BUCKET_SLOTS
#define HASH_MAP_BUCKET_SLOTS 8
#endif


int main(int argc, char *argv[]) {
    pthread_t tr[N_READERS], tc[N_COUNTERS];
    uint16_t off[N_COUNTERS];
    ReaderArg ra[N_READERS];
    CounterArg ca[N_COUNTERS];
    uint64_t counter_a = 0, counter_b = 0;
    double start[2];
    Config conf = {0};
    uint64_t exp_map_buckets;

    start[0] = get_time();

    init_config(argc, argv, &conf);

    uint32_t n_buckets = conf.expected_records ? next_pow2(conf.expected_records / SLOTS_PER_EXTEND) : next_pow2(HASH_MAP_BUCKET_SLOTS);

    hash_map_init(n_buckets, EXTERNAL_STORAGE_SIZE);

    // Start threads
    for(int i = 0; i < N_READERS; i++) {
        initReaderParam(&ra[i], &conf, i);
        pthread_create(&tr[i], NULL, worker_read, &ra[i]);
    }
    for(int  i = 0; i < N_COUNTERS; i++) {
        initCounterParam(&ca[i], &conf, i);
        pthread_create(&tc[i], NULL, worker_count, &ca[i]);
    }

    if (conf.verbose) printf("%d READERS and %d COUNTERS started.\n", N_READERS, N_COUNTERS);

    // Wait for threads to finish
    for(int i = 0; i < N_READERS; i++) {
        pthread_join(tr[i], NULL);
    }

    if (conf.verbose) printf("READERS finished work after %lf seconds.\n", get_time()-start[0]);

    // Signal consumer work finished
    setFinished();
    for(int  i = 0; i < N_COUNTERS; i++) {
        pthread_join(tc[i], NULL);
    }


    if (conf.verbose) {
        printf("COUNTERS finished work after %lf seconds.\n", get_time()-start[0]);
        start[1] = get_time();
    }

    //calculate result
    AttributeResultCount *next;
    size_t total_attributes = 0;
    HashMapCursor cur = init_hash_map_cursor();
    while((next = hash_map_next(&cur)) != NULL) {
        printf("%.*s: %u\n", (int) next->attr.size, next->attr.data, next->count);
        ++total_attributes;
    }
    printf("Total %s(s) - %lu\n", conf.label, total_attributes);

    if (conf.verbose) printf("Final counting took %lf seconds.\n", get_time()-start[1]);

    if (conf.verbose) printf("Total Duration %lf seconds.\n", get_time()-start[0]);
    return 0;
}

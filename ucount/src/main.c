#include "common.h"
#include "workers.h"
#include "buffer_pool.h"
#include "bitset.h"
#include "hash_map.h"


int main(int argc, char *argv[]) {
    pthread_t tr[N_READERS], tc[N_COUNTERS];
    ReaderArg ra[N_READERS];
    CounterArg ca[N_COUNTERS];
    uint64_t counter_a = 0, counter_b = 0, counter_c = 0;
    double start[2];
    Config conf = {0};
    uint64_t exp_map_buckets;

    init_config(argc, argv, &conf);

    if (conf.verbose) start[0] = get_time();

    if (conf.demo) {
        uint32_t demo_ints[3][4] __attribute__((aligned(CL_SIZE))) = 
                {   {0x100, 0x100, 0xfff, 0xfff},
                    {0x100, 0x100, 0x100, 0x100},
                    {0x100, 0x100, 0x800, 0xfff}};
        printf("Demo mode:\n");
        printf("===========================================\n");
        for (int i = 0; i < 3; i++) {
            // For small datasets ( < 1GB ) use HashMap
            hash_map_init(1);
            hash_map_put(&demo_ints[i][0], 16);
            hash_map_get_results(&counter_a, &counter_b);
            //bitset_init();
            //bitset_put(&demo_ints[i][0], 16);
            //bitset_get_results(&counter_a, &counter_b);
            counter_c = 4;
            if(conf.mode_a) printf("0x%x 0x%x 0x%x 0x%x -> %ld unique numbers\n", 
                demo_ints[i][0], demo_ints[i][1], demo_ints[i][2], demo_ints[i][3], counter_a);
            if(conf.mode_b) printf("0x%x 0x%x 0x%x 0x%x -> %ld numbers seen only once\n", 
                demo_ints[i][0], demo_ints[i][1], demo_ints[i][2], demo_ints[i][3], counter_b);
            if(conf.mode_c) printf("0x%x 0x%x 0x%x 0x%x -> %ld total numbers\n", 
                demo_ints[i][0], demo_ints[i][1], demo_ints[i][2], demo_ints[i][3], counter_c);
            printf("===========================================\n");

        }
    return 0;
    }

    bitset_init();

    // Start threads
    for(int i = 0; i < N_READERS; i++) {
        initReaderParam(&ra[i], &conf, i);
        pthread_create(&tr[i], NULL, worker_read, &ra[i]);
    }
    for(int  i = 0; i < N_COUNTERS; i++) {
        ca[i].n_total = 0;
        ca[i].slot = (POOL_SIZE / N_COUNTERS) * i;
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
    bitset_get_results(&counter_a, &counter_b);

    if (conf.mode_c) {
        for(int i = 0; i < N_COUNTERS; i++) {
            counter_c += ca[i].n_total;
        }
    }


    if (conf.verbose) printf("Final counting took %lf seconds.\n", get_time()-start[1]);

    if (conf.mode_a) printf("%ld unique numbers\n", counter_a);
    if (conf.mode_b) printf("%ld numbers seen only once\n", counter_b);
    if (conf.mode_c) printf("%ld total numbers\n", counter_c);
    if (conf.verbose) printf("Total Duration %lf seconds.\n", get_time()-start[0]);
    return 0;
}

/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#ifndef COMMON_H
#define COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#ifndef FIO_SIZE
#define FIO_SIZE 4096 * 32         //higher for HDD
#endif
#define CL_SIZE 64
#define MAXPOLL (uint32_t) 0xEF
#define POLLSLEEP 100

#define RET_ERR 0xFFFF


typedef struct {
    bool json_stdin;
    char *file_name;
    char *label;
    bool verbose;
    off_t file_size;
    uint32_t expected_records;
} Config;

static struct option in_options[] = {
    {"json", no_argument, 0, 's'},
    {"file", required_argument, 0, 'f'},
    {"label", required_argument, 0, 'l'},
    {"expected records", required_argument, 0, 'm'},
    {0, 0, 0, 0}
};

static inline double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static inline uint32_t next_pow2(uint32_t n) {
    if (n <= 1) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

static inline void checked_polling(uint32_t *poll_count, uint32_t relax) {
    if (++(*poll_count) > relax ? relax : MAXPOLL) {
        usleep(POLLSLEEP);
        *poll_count = 0;
    } else {
        __builtin_ia32_pause();
    }
}

static inline void init_config(int argc, char **argv, Config *conf) {
    int opt;
    while ((opt = getopt_long(argc, argv, "f:l:n:sv", in_options, NULL)) != -1) {
        switch (opt) {
	    case 'l': if(optarg) conf->label = strdup(optarg); break;
	    case 's': conf->json_stdin = true; break;
            case 'f': if(optarg) conf->file_name = strdup(optarg); break;
	    case 'n': if(optarg) conf->expected_records = atoi(optarg); break;
	    case 'v': conf->verbose = true; break;
            case '?': exit(EXIT_FAILURE);
        }
    }
    if (conf->json_stdin) {
	conf->file_size = -1;
	return;
    }
    if (!conf->file_name) {
        fprintf(stderr, "Specify necessary options (-s or -f (path_to_integer_file) and -l (attribute label).\n");
        exit(EXIT_FAILURE);
    }
    int fd = open((conf->file_name), O_RDONLY| O_DIRECT);
   if(fd < 0) {
        fprintf(stderr, "Error opening file");
        exit(EXIT_FAILURE);
    }
    conf->file_size = lseek(fd, 0, SEEK_END);
    close(fd);
}




#endif

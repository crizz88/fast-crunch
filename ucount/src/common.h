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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#ifndef FIO_SIZE
#define FIO_SIZE 4096 * 32         //higher for HDD
#endif
#define CL_SIZE 64

#define RET_ERR 0xFFFF


typedef struct {
    char *file_name;
    bool mode_a;
    bool mode_b;
    bool mode_c;
    bool demo;
    bool verbose;
    off_t file_size;
} Config;


static inline double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static inline void init_config(int argc, char **argv, Config *conf) {
    int opt;
    while ((opt = getopt(argc, argv, "adf:ouv")) != -1) {
        switch (opt) {
            case 'f': conf->file_name = strdup(optarg); break;
            case 'a': conf->mode_c = true; break;
            case 'u': conf->mode_a = true; break;
            case 'o': conf->mode_b = true; break;
            case 'd': conf->demo = true; break;
            case 'v': conf->verbose = true; break;
            case '?': exit(EXIT_FAILURE);
        }
    }
    if (conf->demo) return;
    if (!conf->file_name) {
        fprintf(stderr, "Specify necessary options (-a and/or -b, and -f (path_to_integer_file).\n");
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

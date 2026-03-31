/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include <fcntl.h>

#include "workers.h"
#include "buffer_pool.h"
#include "bitset.h"
/**
  * Initialize reader parameters.
  *
  */

void initReaderParam(ReaderArg *ra, void* arg, int id) {
    Config *conf = (Config*) arg;
    ra->path = conf->file_name;
    off_t chunk = (conf->file_size / N_READERS) / 4096 * 4096; 
    ra->start_offset = chunk * id;
    if (id == N_READERS - 1) {
        //Ensure nothing is missing
        ra->end_offset = conf->file_size; 
    } else {
        ra->end_offset = chunk * (id + 1);
    }
    ra->slot = (POOL_SIZE / N_READERS) * id;

}

/**
  * Worker: READER aka producer
  *
  */
void* worker_read(void* arg) {
    ReaderArg *ra = (ReaderArg*)arg;
    int fd = open((ra->path), O_RDONLY| O_DIRECT);
    off_t current_pos = ra->start_offset;
    uint16_t emptySlot;
    ssize_t bytesRead;
    if(fd < 0) {
        fprintf(stderr, "Error opening file");
        exit(EXIT_FAILURE);
    }

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE);

    while (current_pos < ra->end_offset) {

        emptySlot = getEmptySlot(ra->slot);

        if (!setBufferStatus(emptySlot, SLOT_EMPTY, SLOT_BUSY)) continue;
        size_t toRead = FIO_SIZE;
        if (current_pos + toRead > ra->end_offset) toRead = ra->end_offset - current_pos;
        bytesRead = pread(fd, getBufferData(emptySlot), toRead, current_pos);
        if (bytesRead <= 0) {
	    setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FINISH);
	    break;
	}
        setBufferLen(emptySlot, bytesRead);
        setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FULL);
        current_pos += bytesRead;
    }
    close(fd);
    pthread_exit(NULL);
}

/**
  * Worker: COUNTER aka consumer
  *
  */
void* worker_count(void* arg) {
    uint16_t filledSlot;
    CounterArg *ca = (CounterArg*)arg;
    while ((filledSlot = getFilledSlot(ca->slot)) != RET_ERR) {
        if (!setBufferStatus(filledSlot, SLOT_FULL, SLOT_BUSY)) continue;
        uint32_t *p = (uint32_t*) getBufferData(filledSlot);
        size_t actual_bytes = *getBufferLen(filledSlot);

	ca->n_total += bitset_put(p, actual_bytes);

        setBufferStatus(filledSlot, SLOT_BUSY, SLOT_EMPTY);
    }
    pthread_exit(NULL);
}


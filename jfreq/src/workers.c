/*
 * Copyright (c) 2026 cR!zZ
 * Licensed under the MIT License. See LICENSE file in the project root for full license information.
 */

#include <fcntl.h>

#include "workers.h"
#include "buffer_pool.h"
#include "hash_map.h"

#ifndef OBJECTTERMINATOR
#define OBJECTTERMINATOR '\n'
#endif
#ifndef LABELTERMINATOR
#define LABELTERMINATOR '"'
#endif
/**
  * some private inline stuff
  *
  */

/**
  * get unclosed object data from previous buffer
  *
  */
static inline void getRemainder(size_t remainder_len, char *remainder, uint16_t emptySlot) {
    if (remainder_len > 0) memcpy(getBufferData(emptySlot), remainder, remainder_len);
}

/**
  * fill remainder on EOF
  *
  */
static inline bool isEOF_remainder(size_t bytesRead, size_t remainder_len, uint16_t emptySlot) {
    if (bytesRead <= 0) {
	setBufferLen(emptySlot, remainder_len);
	setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FULL);
	return true;
    }
    return false;
}

/**
  * remember remainder and return its length
  *
  */
static inline ssize_t processRemainderGetLength(char* remainder, size_t *remainder_len, ssize_t total_read, ssize_t emptySlot) {
    if (total_read == 0) return 0; // EOF
    char *last_bracket = memrchr(getBufferData(emptySlot), OBJECTTERMINATOR, total_read);

    if (!last_bracket) {
	if (!memrchr(getBufferData(emptySlot), ']', total_read)) {// race condition at EOF -1
            fprintf(stderr, "Error: Single JSON object exceeds slot size. Increase FIO_SIZE or check input. %ld, %ld, %s\n", total_read, *remainder_len, getBufferData(emptySlot));
            exit(EXIT_FAILURE);
        }
	return 0;
    }

    ssize_t valid_len = (last_bracket - getBufferData(emptySlot)) + 1;
    size_t leftover_len = total_read - valid_len;
    memcpy(remainder, last_bracket + 1, leftover_len);
    *remainder_len = leftover_len;
    return valid_len;
}

/**
  * parse object attribute from json and put it in the hash map
  *
  */
static inline void processJson(uint16_t slot, CounterArg *ca) {
    char *ptr = (char*)getBufferData(slot);
    char *end = ptr + (*getBufferLen(slot));

    while ((ptr = memmem(ptr, end - ptr, ca->label, ca->label_length))) {
        ptr += ca->label_length;
        char *start = ptr;
        while (ptr < end && *ptr != LABELTERMINATOR) ptr++;
        size_t val_len = ptr - start;

	if (val_len > MAX_ATTRIBUTE_STR_LEN) {
	    fprintf(stderr, 
		"Error: Attribute string exceeds maximum size (%d > %ld). Increase MAX_ATTRIBUTE_LABEL_LEN.\n"
		,MAX_ATTRIBUTE_STR_LEN, val_len);
	    exit(EXIT_FAILURE);
	}

        if (val_len > 0) {
            hash_map_put_or_increment(start, val_len);
        }
	if (ptr < end) ptr++;
    }
}




/**
  * public interfaces
  *
  */

/**
  * Initialize counter parameters
  *
  */
void initCounterParam(CounterArg *ca, void*arg, int id) {
    Config *conf = (Config*) arg;
    snprintf(ca->label, sizeof(ca->label), "\"%s\":\"", conf->label);
    ca->label_length = strlen(ca->label);
    ca->slot = (POOL_SIZE / N_COUNTERS) * id;
}


/**
  * Initialize reader parameters. For sharded mode, limits are adapted to entire JSON objects.
  *
  */

void initReaderParam(ReaderArg *ra, void* arg, int id) {
    Config *conf = (Config*) arg;
    ra->path = conf->file_name;
    ra->json_stdin = conf->json_stdin;
    if (ra->json_stdin) {
	ra->slot = 0;
	if (N_READERS > 1) {
	    fprintf(stderr, "Error: Only one reader is assumed for streaming processing.\n");
	    exit(EXIT_FAILURE);
	}
	return;
    }
   off_t chunk = conf->file_size / N_READERS;
    off_t start = chunk * id;
    off_t end = (id == N_READERS - 1) ? conf->file_size : chunk * (id + 1);

    int fd = open(conf->file_name, O_RDONLY);
    char c;

    // in sharded reading mode, ensure to read json objects entirely
    if (id > 0) {
        lseek(fd, start, SEEK_SET);
        while (read(fd, &c, 1) > 0 && c != OBJECTTERMINATOR) start++;
        start++;
    }

    if (id < N_READERS - 1) {
        lseek(fd, end, SEEK_SET);
        while (read(fd, &c, 1) > 0 && c != OBJECTTERMINATOR) end++;
        end++;
    }
    close(fd);

    ra->start_offset = start;
    ra->end_offset = end;
    ra->slot = (POOL_SIZE / N_READERS) * id;
}

/**
  * Worker: READER aka producer
  *
  */
void* worker_read(void* arg) {
    ReaderArg *ra = (ReaderArg*)arg;
    int fd;
    uint16_t emptySlot;
    ssize_t bytesRead;
    size_t remainder_len = 0;
    char remainder[FIO_SIZE];
    if (ra->json_stdin) {
	fd = STDIN_FILENO;
	uint32_t poll_count = 0;
	while (true) {
            emptySlot = getEmptySlot(ra->slot);

	    if (!setBufferStatus(emptySlot, SLOT_EMPTY, SLOT_BUSY)) continue;

	    getRemainder(remainder_len, remainder, emptySlot);

	    ssize_t total_read = remainder_len;
	    poll_count = 0;
	    while (total_read < FIO_SIZE) {
		bytesRead = read(fd, getBufferData(emptySlot) + total_read, FIO_SIZE - total_read);
		total_read += bytesRead;
		checked_polling(&poll_count, 1);
		if (poll_count == 0 && isEOF_remainder(bytesRead, remainder_len, emptySlot)) break;
	    }
    	    ssize_t buf_len = processRemainderGetLength(remainder, &remainder_len, total_read, emptySlot);
	    setBufferLen(emptySlot, buf_len);
	    if (buf_len == 0) {
	        setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FINISH);
		break; // no valid data on buffer
	    }
  	    setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FULL);
	}
    } else {
	fd = open((ra->path), O_RDONLY); // | O_DIRECT); crazy stuff happens with O_DIRECT and unaligned reads
	off_t current_pos = ra->start_offset;
	if(fd < 0) {
            fprintf(stderr, "Error opening file");
            exit(EXIT_FAILURE);
	}

	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE);

	while (current_pos < ra->end_offset) {

            emptySlot = getEmptySlot(ra->slot);

            if (!setBufferStatus(emptySlot, SLOT_EMPTY, SLOT_BUSY)) continue;

	    getRemainder(remainder_len, remainder, emptySlot);

	    size_t maxToRead = FIO_SIZE - remainder_len;
            size_t remainingInFile = ra->end_offset - current_pos;
            size_t toRead = (remainingInFile < maxToRead) ? remainingInFile : maxToRead;

            ssize_t bytesFromDiskThisSlot = 0;
            while (bytesFromDiskThisSlot < toRead) {
                bytesRead = pread(fd, 
                                 (char*)getBufferData(emptySlot) + remainder_len + bytesFromDiskThisSlot, 
                                 toRead - bytesFromDiskThisSlot, 
                                 current_pos + bytesFromDiskThisSlot);
                if (bytesRead <= 0) break;
                bytesFromDiskThisSlot += bytesRead;
            }

            current_pos += bytesFromDiskThisSlot;

            size_t total_in_slot = remainder_len + bytesFromDiskThisSlot;
            ssize_t buf_len = processRemainderGetLength(remainder, &remainder_len, total_in_slot, emptySlot);
            setBufferLen(emptySlot, buf_len);
            if (buf_len == 0) {
                setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FINISH);
                break; // no valid data on buffer
            }
            setBufferStatus(emptySlot, SLOT_BUSY, SLOT_FULL);

        }
    }
    close(fd);
    pthread_exit(NULL);
}

/**
  * Worker: COUNTER aka consumer
  *
  */
void* worker_count(void* arg) {
    CounterArg *ca = (CounterArg*) arg;
    uint16_t filledSlot;
    uint32_t poll_count = 0;
    while ((filledSlot = getFilledSlot(ca->slot)) != RET_ERR) {
        if (!setBufferStatus(filledSlot, SLOT_FULL, SLOT_BUSY)) continue;

	processJson(filledSlot, ca);

        setBufferStatus(filledSlot, SLOT_BUSY, SLOT_EMPTY);
    }
    pthread_exit(NULL);
}



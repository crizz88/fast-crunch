export SRC_DIR = src
export BIN_DIR = bin
export TARGET  = jfreq

export N_COUNTERS ?= 1
export N_READERS ?= 1
export OBJECTTERMINATOR ?= "'\n'"
export POOL_SIZE ?= 4
export FIO_SIZE ?= 4194304
export HASH_MAP_BUCKET_SLOT ?= 8
export EXTERNAL_STORAGE_SIZE ?= 512



export CFLAGS = -O3 -march=native -flto -fomit-frame-pointer -funroll-loops -pthread -D_GNU_SOURCE
export CFLAGS += -DN_COUNTERS=$(N_COUNTERS) -DN_READERS=$(N_READERS) -DOBJECTTERMINATOR=$(OBJECTTERMINATOR) -DPOOL_SIZE=$(POOL_SIZE) -DFIO_SIZE=$(FIO_SIZE) -DHASH_MAP_BUCKET_SLOT=$(HASH_MAP_BUCKET_SLOT) -DEXTERNAL_STORAGE_SIZE=$(EXTERNAL_STORAGE_SIZE)
export CC = gcc

.PHONY: all clean

all: $(BIN_DIR)
	$(MAKE) -C $(SRC_DIR)
	@mv $(SRC_DIR)/$(TARGET) ./$(TARGET)
	@echo "Build successful: ./$(TARGET)"

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	$(MAKE) -C $(SRC_DIR) clean
	rm -rf $(BIN_DIR)
	rm -f $(TARGET)
	rm -f *.out

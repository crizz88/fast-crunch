export SRC_DIR = src
export BIN_DIR = bin
export TARGET  = ucount

export N_COUNTERS ?= 6
export N_READERS ?= 2
export POOL_SIZE ?= 12
export FIO_SIZE ?= 8388608

export CFLAGS = -O3 -march=native -flto -fomit-frame-pointer -funroll-loops -pthread -D_GNU_SOURCE
export CFLAGS += -DN_COUNTERS=$(N_COUNTERS) -DN_READERS=$(N_READERS) -DPOOL_SIZE=$(POOL_SIZE) -DFIO_SIZE=$(FIO_SIZE)
export CC = gcc

.PHONY: all clean $(SRC_DIR)

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
	rm -f *.bin

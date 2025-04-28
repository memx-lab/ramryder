PROJECT_ROOT := $(abspath ..)
SRC_DIR := $(PROJECT_ROOT)/src
TEST_DIR := $(PROJECT_ROOT)/test

CC = gcc

ifdef DEBUG
    CFLAGS = -Wall -Wextra -g -DENABLE_DEBUG -I$(SRC_DIR) -I$(PROJECT_ROOT)/include
else
    CFLAGS = -Wall -Wextra -O2 -I$(SRC_DIR) -I$(PROJECT_ROOT)/include
endif

LDFLAGS = -L$(PROJECT_ROOT)/lib -lpcm_public -ljson-c -lpthread -lcurl -lm -lpfm -lstdc++

SRC_FILES = \
    $(SRC_DIR)/memory_pool.c \
    $(SRC_DIR)/guest_agent.c \
    $(SRC_DIR)/guest_monitor.c \
    $(SRC_DIR)/qemu_agent.c \
    $(SRC_DIR)/util_socket.c \
    $(SRC_DIR)/vm_manager.c \
    $(SRC_DIR)/perf_counter.c

TESTS = test_memory_pool test_perf_event

all: $(TESTS)

$(TESTS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(SRC_FILES) $(LDFLAGS)

clean:
	rm -f $(TESTS)

.PHONY: all clean

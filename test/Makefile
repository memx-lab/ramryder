PROJECT_ROOT := $(abspath ..)
SRC_DIR := $(PROJECT_ROOT)/src

CC = gcc

-include $(PROJECT_ROOT)/config.mk

CFLAGS = -Wall -Wextra
ifeq ($(CONFIG_DEBUG),y)
    CFLAGS += -g -DENABLE_DEBUG
else
    CFLAGS += -O2
endif

CFLAGS += -I$(SRC_DIR) -I$(PROJECT_ROOT)/include
CFLAGS += -I$(PROJECT_ROOT)/third_party/zlog/src
CFLAGS += -I$(PROJECT_ROOT)/third_party/pcm/include

ZLOG_LIB = $(PROJECT_ROOT)/third_party/zlog/src/libzlog.a
LDFLAGS = -L$(PROJECT_ROOT)/third_party/pcm/lib -lpcm_public -ljson-c -lpthread -lcurl -lm -lpfm -lstdc++ $(ZLOG_LIB)

SRC_FILES = \
    $(SRC_DIR)/memory_pool.c \
    $(SRC_DIR)/guest_agent.c \
    $(SRC_DIR)/guest_monitor.c \
    $(SRC_DIR)/qemu_agent.c \
    $(SRC_DIR)/util_log.c \
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

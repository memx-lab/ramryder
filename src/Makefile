PROJECT_ROOT := $(abspath ..)

CC = gcc

ifdef DEBUG
    CFLAGS = -Wall -Wextra -g -DENABLE_DEBUG -I$(PROJECT_ROOT)/include
else
    CFLAGS = -Wall -Wextra -O2 -I$(PROJECT_ROOT)/include
endif

LDFLAGS = -L$(PROJECT_ROOT)/lib -lpcm_public -ljson-c -lpthread -lcurl -lm -lstdc++
TARGETS = resource_manager resource_client
SRC_MANAGER = resource_manager.c memory_pool.c guest_agent.c guest_monitor.c qemu_agent.c util_socket.c vm_manager.c perf_counter.c
SRC_CLIENT = resource_client.c

OBJ_MANAGER = $(SRC_MANAGER:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)

all: $(TARGETS)
	@rm -f $(OBJ_MANAGER) $(OBJ_CLIENT)

resource_manager: $(OBJ_MANAGER)
	$(CC) $(CFLAGS) -o resource_manager $(OBJ_MANAGER) $(LDFLAGS)

resource_client: $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o resource_client $(OBJ_CLIENT) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) $(OBJ_MANAGER) $(OBJ_CLIENT)

.PHONY: all clean


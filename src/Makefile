CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -ljson-c
TARGETS = resource_manager resource_client
SRC_MANAGER = resource_manager.c memory_resource.c
SRC_CLIENT = resource_client.c
OBJ_MANAGER = $(SRC_MANAGER:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)

all: $(TARGETS)
	@rm -f $(OBJ_MANAGER) $(OBJ_CLIENT)

resource_manager: $(OBJ_MANAGER)
	$(CC) $(CFLAGS) -o resource_manager $(OBJ_MANAGER) $(LDFLAGS)

resource_manager_client: $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o resource_client $(OBJ_CLIENT)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) $(OBJ_MANAGER) $(OBJ_CLIENT)

.PHONY: all clean


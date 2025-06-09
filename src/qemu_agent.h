#ifndef QEMU_AGENT_H
#define QEMU_AGENT_H
#include <stdbool.h>
#include "memory_pool.h"

#define STRING_ID_LEN 32

struct hotplug_request {
    char dimm_id[STRING_ID_LEN];    // e.g., "dimm1"
    char memdev_id[STRING_ID_LEN];  // e.g., "mem1"
    char dev_path[DEV_PATH_LEN];    // e.g., "/dev/dax2.0"
    uint64_t size_bytes;            // e.g., 134217728
    uint64_t align_bytes;           // e.g., 134217728
    bool share;                     // true or false
    int numa_node;                  // e.g., 2; if < 0, automatically find best one
};

struct hotunplug_request {
    char dimm_id[STRING_ID_LEN];    // e.g., "dimm1"
    char memdev_id[STRING_ID_LEN];  // e.g., "mem1"
};

int qemu_agent_hotplug_memory(int vm_id, struct hotplug_request *request);
int qemu_agent_hotunplug_memory(int vm_id, struct hotunplug_request *request);

#endif // QEMU_AGENT_H

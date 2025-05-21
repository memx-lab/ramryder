#ifndef QEMU_AGENT_H
#define QEMU_AGENT_H
#include <stdbool.h>

struct hotplug_request {
    char *dimm_id;      // e.g., "dimm1"
    char *memdev_id;    // e.g., "mem1"
    char *mem_path;     // e.g., "/dev/dax2.0"
    uint64_t size_bytes;      // e.g., 134217728
    uint64_t align_bytes;     // e.g., 134217728
    bool share;               // true or false
    int numa_node;            // e.g., 2; if < 0, skip
};

int qemu_agent_hotplug_memory(int vm_id, struct hotplug_request *request);

#endif // QEMU_AGENT_H

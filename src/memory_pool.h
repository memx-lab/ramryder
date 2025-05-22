#ifndef MEMORY_RESOURCE_H
#define MEMORY_RESOURCE_H
#include "util_queue.h"

#define DEV_PATH_LEN 64

struct memory_request {
    int tier_id;
    int dax_id;
    char dev_path[DEV_PATH_LEN];
    int offset_mb;
    int size_mb;
    int alignment;
    int memdev_idx;

    TAILQ_ENTRY(memory_request) link;
};

struct memory_node_info {
    int tier_id;
    int dax_id;
};

int memory_pool_init(const char* config_file);
void memory_pool_get_usage(char *buffer, int buffer_size);
int memory_pool_allocate_segments(int tier_id, int dax_id, int vm_id,
                    int size_mb, struct memory_request *mem_req);
int memory_pool_release_segments(int tier_id, int dax_id, int vm_id, 
                    int offset_mb, int size_mb);
int memory_pool_get_num_devs(void);
int memory_pool_get_node_info(int node_id, struct memory_node_info *node_info);
void memory_pool_release_vm_memory(int vm_id);

#endif // MEMORY_RESOURCE_H

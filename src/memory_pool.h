#ifndef MEMORY_RESOURCE_H
#define MEMORY_RESOURCE_H

#define DEV_PATH_LEN 64

struct memory_request {
    char dev_path[DEV_PATH_LEN];
    int offset_mb;
    int size_mb;
};

int memory_pool_init(const char* config_file);
void memory_pool_get_usage(char *buffer, int buffer_size);
int memory_pool_allocate_segments(int tier_id, int dev_id, int vm_id,
                    int size_mb, struct memory_request *mem_req);
int memory_pool_release_segments(int tier_id, int dev_id, int vm_id, 
                    int offset_mb, int size_mb);

#endif // MEMORY_RESOURCE_H

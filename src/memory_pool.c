#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "util_common.h"
#include "memory_pool.h"
#include "vm_manager.h"

#define MAX_DEVICES 10
#define MAX_SEGMENTS 4096

struct memory_segment {
    bool allocated;
    int used_vm_id;
};

struct memory_dax_dev {
    char dev_path[DEV_PATH_LEN];
    /*
     * Note that this node id is not same as real node id in guest kerel.
     *
     * This node id is used to interact with QEMU, which will expose this
     * as a PXM in guest kernel. Then guest kernel will translate PXM into
     * a real node id which we finnaly see in kernel.
     */
    int node_id;
    int tier_id;
    /* We use dax id to refer to a parallel unit of a memory tier. */
    int dax_id;
    int total_size_mb;
    int total_segments;
    int used_segments;
    struct memory_segment segments[MAX_SEGMENTS];
};

static struct memory_dax_dev g_mem_devs[MAX_DEVICES];
static int g_num_devs = 0;
static int g_segment_size_mb = 0;

static inline bool is_aligned(int value, int alignment) {
    return value >= 0 && (value % alignment == 0);
}

static int find_free_segments(struct memory_dax_dev *mem_dev, int num_segments)
{
    int i = 0;

    while (i <= mem_dev->total_segments - num_segments) {
        int j = 0;
        for (; j < num_segments; j++) {
            if (mem_dev->segments[i + j].allocated) {
                break;
            }
        }

        if (j == num_segments) {
            return i;
        }

        i = i + j + 1;
    }

    return -1;
}

static int allocate_segments(struct memory_dax_dev *mem_dev, int num_segments, int vm_id)
{
    int start_index = -1;
    int free_segments = 0;

    assert(mem_dev != NULL);
    assert(num_segments > 0);

    free_segments = mem_dev->total_segments - mem_dev->used_segments;
    if (free_segments < num_segments) {
        fprintf(stderr, "No enough segments, free: %d, required: %d\n",
                free_segments, num_segments);
    }

    start_index = find_free_segments(mem_dev, num_segments);
    if (start_index == -1) {
        perror("Cannot find required size in dev\n");
        return -1;
    }

    for (int i = 0; i < num_segments; i++) {
        mem_dev->segments[start_index + i].allocated = true;
        mem_dev->segments[start_index + i].used_vm_id = vm_id;
    }

    mem_dev->used_segments += num_segments;
    return start_index;
}

int memory_pool_allocate_segments(int tier_id, int dax_id, int vm_id,
                    int size_mb, struct memory_request *mem_req)
{
    struct memory_dax_dev *mem_dev = NULL;
    int num_segments= 0, start_index = -1;

    if (!is_aligned(size_mb, g_segment_size_mb)) {
        fprintf(stderr, "Invalid size %d that should align in %dMB and be non-zero\n", 
                size_mb, g_segment_size_mb);
        return -1;
    }

    if (vm_mngr_check_exit(vm_id) == false) {
        fprintf(stderr, "VM %d has not been created\n", vm_id);
        return -1;
    }

    for (int i = 0; i < g_num_devs; i++) {
        if (g_mem_devs[i].tier_id == tier_id && g_mem_devs[i].dax_id == dax_id) {
            mem_dev = &g_mem_devs[i];
            break;
        }
    }

    if (mem_dev == NULL) {
        fprintf(stderr, "Cannot find memory device with tier id %d and dax id: %d\n",
                tier_id, dax_id);
        return -1;
    }

    num_segments = size_mb / g_segment_size_mb;
    start_index = allocate_segments(mem_dev, num_segments, vm_id);
    if (start_index < 0) {
        fprintf(stderr, "Filed to alllocate segments\n");
        return -1;
    }

    snprintf(mem_req->dev_path, DEV_PATH_LEN, "%s", mem_dev->dev_path);
    mem_req->offset_mb = start_index * g_segment_size_mb;
    mem_req->size_mb = num_segments * g_segment_size_mb;
    mem_req->alignment = g_segment_size_mb;
    mem_req->memdev_idx = vm_mngr_get_new_memdev_idx(vm_id);

    printf("Allocated memory, index: %d, dev: %s, offset: %dMB, size: %dMB, alignment: %dMB\n",
            mem_req->memdev_idx, mem_req->dev_path, mem_req->offset_mb, mem_req->size_mb, mem_req->alignment);

    return 0;
}

static int release_segments(struct memory_dax_dev *mem_dev, int vm_id, 
                        int start_index, int segment_count)
{
    for (int i = 0; i < segment_count; i++) {
        int idx = start_index + i;

        /* only release used segments by the VM with the id @vm_id */
        if (idx >= 0 && idx < mem_dev->total_segments &&
            mem_dev->segments[idx].allocated &&
            mem_dev->segments[idx].used_vm_id == vm_id) {
            mem_dev->segments[idx].allocated = false;
            mem_dev->segments[idx].used_vm_id = -1;
            mem_dev->used_segments--;
        } else {
            fprintf(stderr, "Filed to release segment, index %d, alloacted %d, vm id %d\n",
                idx, mem_dev->segments[idx].allocated, mem_dev->segments[idx].used_vm_id);
            return -1;
        }
    }

    return 0;
}

int memory_pool_release_segments(int tier_id, int dax_id, int vm_id, 
                    int offset_mb, int size_mb)
{
    int ret;
    int start_index, segment_count;
    struct memory_dax_dev *mem_dev = NULL;

    if (!is_aligned(size_mb, g_segment_size_mb) || !is_aligned(offset_mb, g_segment_size_mb)) {
        fprintf(stderr, "Invalid size %d or offset %d that should align in %dMB and be non-zero\n", 
                size_mb, offset_mb, g_segment_size_mb);
        return -1;
    }

    if (vm_mngr_check_exit(vm_id) == false) {
        fprintf(stderr, "Failed to release segments as VM %d has not been created\n", vm_id);
        return -1;
    }

    for (int i = 0; i < g_num_devs; i++) {
        if (g_mem_devs[i].tier_id == tier_id && g_mem_devs[i].dax_id == dax_id) {
            mem_dev = &g_mem_devs[i];
            break;
        }
    }

    if (mem_dev == NULL) {
        fprintf(stderr, "Cannot find memory device with tier id %d and dax id: %d\n",
                tier_id, dax_id);
        return -1;
    }

    start_index = offset_mb / g_segment_size_mb;
    segment_count = size_mb / g_segment_size_mb;

    ret = release_segments(mem_dev, vm_id, start_index, segment_count);
    if (ret < 0) {
        fprintf(stderr, "Failed to release all segments, tier id: %d, dax id: %d, vm id: %d, offset: %dMB, size: %dMB\n",
        tier_id, dax_id, vm_id, offset_mb, size_mb);   
        return -1;
    }

    printf("Rleased memory, dev: %s, offset: %dMB, size: %dMB\n",
            mem_dev->dev_path, offset_mb, size_mb);

    return 0;
}

void memory_pool_release_vm_memory(int vm_id)
{
    struct memory_dax_dev *mem_dev = NULL;

    for (int i = 0; i < g_num_devs; i++) {
        mem_dev = &g_mem_devs[i];
        for (int j = 0; j < mem_dev->total_segments; j++) {
            if (mem_dev->segments[j].allocated && mem_dev->segments[j].used_vm_id == vm_id) {
                mem_dev->segments[j].allocated = false;
                mem_dev->segments[j].used_vm_id = -1;
                mem_dev->used_segments--;
            }
        }
    }
}

static int memory_pool_load_config(const char* config_file)
{
    char line[512];
    FILE *file;

    file = fopen(config_file, "r");
    if (!file) {
        perror("Failed to open config file");
        return -1;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || 
            strncmp(line, "[global]", strlen("[global]")) == 0 ||
            strncmp(line, "[devices]", strlen("[devices]")) == 0 ||
            strncmp(line, "[clouddb]", strlen("[clouddb]")) == 0) {
            continue;
        }
        
        char key[32], value1[128], value2[128], value3[128], value4[128];
        if (sscanf(line, "%s %s %s %s %s", key, value1, value2, value3, value4) >= 2) {
            if (strcmp(key, "segment_size_mb") == 0) {
                g_segment_size_mb = atoi(value1);
                if (g_segment_size_mb <= 0) {
                    fprintf(stderr, "Invalid segment size: %d\n", g_segment_size_mb);
                    fclose(file);
                    return -1;
                }
            } else if (strcmp(key, "dev") == 0) {
                if (g_num_devs >= MAX_DEVICES) {
                    fprintf(stderr, "Exceeded maximum number of devices\n");
                    fclose(file);
                    return -1;
                }
                struct memory_dax_dev mem_device;
                if (sscanf(value1, "path=%255s", mem_device.dev_path) != 1 ||
                    sscanf(value2, "size_mb=%d", &mem_device.total_size_mb) != 1 ||
                    sscanf(value3, "tier_id=%d", &mem_device.tier_id) != 1 ||
                    sscanf(value4, "dax_id=%d", &mem_device.dax_id) != 1) {
                    fprintf(stderr, "Invalid device entry: %s %s %s %s\n", value1, value2, value3, value4);
                    fclose(file);
                    return -1;
                }
                if (mem_device.total_size_mb <= 0) {
                    fprintf(stderr, "Invalid device size: %d\n", mem_device.total_size_mb);
                    fclose(file);
                    return -1;
                }
                if (mem_device.tier_id < 0 || mem_device.dax_id < 0) {
                    fprintf(stderr, "Invalid tier id: %d or dax id: %d\n",
                        mem_device.tier_id, mem_device.dax_id);
                }
                g_mem_devs[g_num_devs++] = mem_device;
            }
        }
    }
    fclose(file);
    return 0;
}

static int init_memory_resource(const char* config_file)
{
    int ret;

    ret = memory_pool_load_config(config_file);
    if (ret != 0) {
        perror("Failed to load memory configuration\n");
        return -1;
    }

    for (int i = 0; i < g_num_devs; i++) {
        /* Generate an unique node id for each dax device */
        g_mem_devs[i].node_id = i;
        g_mem_devs[i].total_segments = g_mem_devs[i].total_size_mb / g_segment_size_mb;
        g_mem_devs[i].used_segments = 0;
    }

    return 0;
}

int memory_pool_get_num_devs(void)
{
    return g_num_devs;
}

int memory_pool_get_node_info(int node_id, struct memory_node_info *node_info)
{
    for (int i = 0; i < g_num_devs; i++) {
        if (g_mem_devs[i].node_id == node_id) {
            node_info->tier_id = g_mem_devs[i].tier_id;
            node_info->dax_id = g_mem_devs[i].dax_id;
            return 0;
        }
    }

    return -1;
}

void memory_pool_get_usage(char *buffer, int buffer_size)
{
    if (g_num_devs == 0) {
        snprintf(buffer, buffer_size, "No memory devices initialized.\n");
        return;
    }
    
    int offset = snprintf(buffer, buffer_size, "Index | Device Path | Size (MB) | Segment (MB) | Used Seg | Total Seg | Node ID | Tier ID | Dax ID\n");
    
    for (int i = 0; i < g_num_devs && offset < buffer_size; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                "%5d | %s | %9d | %12d | %8d | %9d | %7d | %7d | %6d\n",
                i, g_mem_devs[i].dev_path, g_mem_devs[i].total_size_mb, g_segment_size_mb,
                g_mem_devs[i].used_segments, g_mem_devs[i].total_segments,
                g_mem_devs[i].node_id, g_mem_devs[i].tier_id, g_mem_devs[i].dax_id);
        if (offset >= buffer_size - 1) {
            break;
        }
    }
}

int memory_pool_init(const char* config_file)
{
    int ret;

    ret = init_memory_resource(config_file);
    if (ret != 0) {
        perror("Failed to init memory resource\n");
        return -1;
    }

    return 0;
}
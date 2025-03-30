#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "memory_resource.h"

#define DEFAULT_CONFIG "dax_memory.conf"
#define MAX_DEVICES 10
#define MAX_SEGMENTS 4096
#define DEV_INFO_SIZE 1024

struct memory_segment {
    bool allocated;
    int used_vm_id;
};

struct memory_dax_dev {
    char dev_path[256];
    int total_size;
    int segment_size;
    int total_segments;
    int used_segments;
    struct memory_segment segments[MAX_SEGMENTS];
};

static struct memory_dax_dev mem_devs[MAX_DEVICES];
static int g_dev_count = 0;
static int g_segment_size = 0;

static int load_config(void)
{
    char line[256];
    FILE *file;

    file = fopen(DEFAULT_CONFIG, "r");
    if (!file) {
        perror("Failed to open config file");
        return -1;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || 
            strncmp(line, "[global]", 8) == 0 ||
            strncmp(line, "[devices]", 9) == 0) {
            continue;
        }
        
        char key[32], value1[256], value2[256];
        if (sscanf(line, "%s %s %s", key, value1, value2) >= 2) {
            if (strcmp(key, "segment_size") == 0) {
                g_segment_size = atoi(value1);
                if (g_segment_size <= 0) {
                    fprintf(stderr, "Invalid segment size: %d\n", g_segment_size);
                    fclose(file);
                    return -1;
                }
            } else if (strcmp(key, "dev") == 0) {
                if (g_dev_count >= MAX_DEVICES) {
                    fprintf(stderr, "Exceeded maximum number of devices\n");
                    fclose(file);
                    return -1;
                }
                struct memory_dax_dev mem_device;
                if (sscanf(value1, "path=%255s", mem_device.dev_path) != 1 ||
                    sscanf(value2, "size=%d", &mem_device.total_size) != 1) {
                    fprintf(stderr, "Invalid device entry: %s %s\n", value1, value2);
                    fclose(file);
                    return -1;
                }
                if (mem_device.total_size <= 0) {
                    fprintf(stderr, "Invalid device size: %d\n", mem_device.total_size);
                    fclose(file);
                    return -1;
                }
                mem_device.segment_size = g_segment_size;
                mem_device.total_segments = mem_device.total_size / mem_device.segment_size;
                mem_devs[g_dev_count++] = mem_device;
            }
        }
    }
    fclose(file);
    return 0;
}

static void init_memory_resource(void)
{
    int i;

    for (i = 0; i < g_dev_count; i++) {
        mem_devs[i].total_segments = mem_devs[i].total_size / mem_devs[i].segment_size;
        mem_devs[i].used_segments = 0;
    }
}

void get_memory_resource(char *buffer, int buffer_size)
{
    if (g_dev_count == 0) {
        snprintf(buffer, buffer_size, "No memory devices initialized.\n");
        return;
    }
    
    int offset = snprintf(buffer, buffer_size, "Index | Device Path | Size (MB) | Segment (MB) | Used Seg | Total Seg \n");
    
    for (int i = 0; i < g_dev_count && offset < buffer_size; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, "%5d | %s | %9d | %12d | %8d | %9d\n", i,
                           mem_devs[i].dev_path, mem_devs[i].total_size, mem_devs[i].segment_size,
                           mem_devs[i].used_segments, mem_devs[i].total_segments);
        if (offset >= buffer_size - 1) {
            break;
        }
    }
}

int memory_manager_init(void) {
    char buffer[DEV_INFO_SIZE];

    if (load_config() != 0) {
        perror("Failed to load configuration\n");
        return -1;
    }

    init_memory_resource();
    get_memory_resource(buffer, DEV_INFO_SIZE);
#ifdef ENABLE_DEBUG
    printf("%s", buffer);
#endif
    return 0;
}
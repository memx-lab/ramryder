#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <signal.h>
#include "util_common.h"
#include "memory_pool.h"
#include "vm_manager.h"

#define CONFIG_FILE "elasticmm.conf"
#define DEV_INFO_SIZE 1024

int main()
{
    int ret;
    char buffer[DEV_INFO_SIZE];
    struct memory_request mem_req;

    if (memory_pool_init(CONFIG_FILE) != 0) {
        exit(EXIT_FAILURE);
    }

    memory_pool_get_usage(buffer, DEV_INFO_SIZE);
    printf("%s", buffer);

    // must create instance before allocating memory
    ret = vm_mngr_instance_create(0, "[0,1]");
    BUG_ON(ret != 0);

    ret = memory_pool_allocate_segments(0, 1, 0, 128, &mem_req);
    BUG_ON(ret != 0);
    BUG_ON(mem_req.memdev_idx != 0);
    BUG_ON(mem_req.offset_mb != 0);
    BUG_ON(mem_req.size_mb != 128);

    ret = memory_pool_allocate_segments(0, 1, 0, 256, &mem_req);
    BUG_ON(ret != 0);
    BUG_ON(mem_req.memdev_idx != 1);
    BUG_ON(mem_req.offset_mb != 128);
    BUG_ON(mem_req.size_mb != 256);

    ret = memory_pool_release_segments(0, 1, 0, 128, 128);
    BUG_ON(ret != 0);

    ret = memory_pool_allocate_segments(0, 1, 0, 256, &mem_req);
    BUG_ON(ret != 0);
    BUG_ON(mem_req.memdev_idx != 2); // memdev index should not deceease when releasing
    BUG_ON(mem_req.offset_mb != 384);
    BUG_ON(mem_req.size_mb != 256);

    ret = memory_pool_allocate_segments(0, 1, 0, 128, &mem_req);
    BUG_ON(ret != 0);
    BUG_ON(mem_req.memdev_idx != 3); // memdev index should not deceease when releasing
    BUG_ON(mem_req.offset_mb != 128);
    BUG_ON(mem_req.size_mb != 128);

    return 0;
}
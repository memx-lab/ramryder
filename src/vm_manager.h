#ifndef VM_MANAGER_H
#define VM_MANAGER_H
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "perf_counter.h"
#include "util_queue.h"
#include "memory_pool.h"

#define ENABLE_PERF
#define MAX_NUM_VM 16

struct vm_mem_req {
    char dev_path[DEV_PATH_LEN];
    int offset_mb;
    int size_mb;
    int align_mb;
    int memdev_idx;
};

struct memory_dev {
    int memdev_idx;
    struct memory_request memory_req;
    TAILQ_ENTRY(memory_dev) link;
};

struct vm_instance {
    // basic
    int vm_id;
    int num_cores;
    char core_set[256];
    int core_index; // used by perf counter setup
    bool initialized;
    bool running;

    // monitor related
    uint64_t mem_bw_local;
    uint64_t mem_bw_remote;
    double mem_bw_util;
    uint64_t mem_cp;
    double mem_cp_util;

    // perf event related counters and metrics
    int *perf_event_fds[PERF_EVENT_TYPE_MAX];
    uint64_t cum_perf_event_counts[PERF_EVENT_TYPE_MAX];
	uint64_t delta_perf_event_counts[PERF_EVENT_TYPE_MAX];
	double cur_metrics[METRIC_TYPE_MAX];
	double ewma_metrics[METRIC_TYPE_MAX];
    bool ewma_initialized;

    // memory management
    int memdev_counter;
    TAILQ_HEAD(, memory_dev) attached_devs;
};

typedef void (*vm_handler_fn)(struct vm_instance *VM, void *arg);
typedef void (*core_handler_fn)(struct vm_instance *VM, int core_id, void *arg);

// VM managment
int vm_mngr_instance_create(int vm_id, char *core_set);
int vm_mngr_instance_destroy(int vm_id);
void vm_mngr_for_each_vm(vm_handler_fn vm_handler, void *arg);
void vm_mngr_for_each_vm_running(vm_handler_fn vm_handler, void *arg);
void vm_mngr_for_each_core(struct vm_instance *VM, core_handler_fn core_handler, void *arg);
int vm_mngr_instance_start(int vm_id);
int vm_mngr_instance_stop(int vm_id);
bool vm_mngr_check_exit(int vm_id);
int vm_mngr_instance_alloc_mem(int tier_id, int dax_id, int vm_id,
                        int size_mb, struct vm_mem_req *vm_mem_req);
int vm_mngr_instance_free_mem(int vm_id, int memdev_idx);
struct memory_request *vm_mngr_instance_get_mem(int vm_id, int memdev_idx);

// VM monitor
void vm_mngr_update_perf_counters(void);
void vm_mngr_update_metrics(int operation_window_s);

#endif
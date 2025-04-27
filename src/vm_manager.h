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

#define MAX_NUM_VM 16


struct vm_instance {
    // basic
    int vm_id;
    int num_cores;
    char core_set[256];
    int core_index; // used by perf counter setup
    bool initialized;

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
};

typedef void (*vm_handler_fn)(struct vm_instance *VM, void *arg);
typedef void (*core_handler_fn)(struct vm_instance *VM, int core_id, void *arg);

int vm_mngr_instance_create(int vm_id, char *core_set);
void vm_mngr_instance_destroy(int vm_id);
void vm_mngr_for_each_vm(vm_handler_fn vm_handler, void *arg);
void  vm_mngr_for_each_core(struct vm_instance *VM, core_handler_fn core_handler, void *arg);

void vm_mngr_update_perf_counters(void);
void vm_mngr_update_metrics(int operation_window_s);

#endif
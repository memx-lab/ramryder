#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "perf_counter.h"
#include "util_common.h"

#define MAX_VM_INSTANCE 16

struct vm_instance {
    // basic
    int vm_id;
    int num_cores;
    char core_set[256];
    int core_index; // used by perf counter setup
    bool initialized;

    // monitor related
    int *perf_event_fds[PERF_EVENT_TYPE_MAX];
    uint64_t cum_perf_event_counts[PERF_EVENT_TYPE_MAX];
	uint64_t delta_perf_event_counts[PERF_EVENT_TYPE_MAX];
	double cur_metrics[METRIC_TYPE_MAX];
	double ewma_metrics[METRIC_TYPE_MAX];
};

struct vm_instance_manager {
    int count;
    // TODO: use TAILQ instead
    struct vm_instance VMs[MAX_VM_INSTANCE];
};

static struct vm_instance_manager g_vm_mngr;

typedef void (*core_handler_fn)(int core_id, void *arg);

void for_each_core(const char *core_set, core_handler_fn core_handler, void *arg)
{
    char *copy = strdup(core_set);
    char *token = strtok(copy, ",");

    // format: aa-bb, xx-yy
    while (token) {
        int start = -1, end = -1;

        if (sscanf(token, "%d-%d", &start, &end) == 2) {
            // core range（e.g., "20-39"）
            if (start <= end) {
                for (int i = start; i <= end; ++i) {
                    core_handler(i, arg);
                }
            }
        } else if (sscanf(token, "%d", &start) == 1) {
            // single core（e.g., "41"）
            core_handler(start, arg);
        }

        token = strtok(NULL, ",");
    }
    free(copy);
}

static void __vm_perf_counter_setup_core(int core_id, void *arg)
{
    int leader_fd = -1;
    struct vm_instance *VM = (struct vm_instance *)arg;

    BUG_ON(core_id < 0);
    for (int j = 0; j < PERF_EVENT_TYPE_MAX; j++) {
        if (perf_event_is_leader_arr[j]) {
            leader_fd = -1;
        }
                
        VM->perf_event_fds[j][VM->core_index] = perf_event_setup((int)core_id,
            perf_event_config_arr[j], perf_event_config1_arr[j],
            /* pin_event */ perf_event_pin_arr[j], false, false, false, false,
            /* exclude_host */ false,  /* exclude_guest */ false,
            leader_fd);
        BUG_ON(VM->perf_event_fds[j][VM->core_index] < 0);

        if (leader_fd == -1) {
            leader_fd = VM->perf_event_fds[j][VM->core_index];
        }
    }

    VM->core_index++;
}

static void vm_perf_counter_setup(struct vm_instance *VM)
{
    BUG_ON(VM->num_cores <= 0);

    // init perf counters for VM
    for (int j = 0; j < PERF_EVENT_TYPE_MAX; ++j) {
		VM->perf_event_fds[j] = (int *)malloc(VM->num_cores * sizeof(int));
		BUG_ON(VM->perf_event_fds[j] == NULL);
		VM->cum_perf_event_counts[j] = 0;
		VM->delta_perf_event_counts[j] = 0;
	}
	for (int j = 0; j < METRIC_TYPE_MAX; ++j) {
		VM->cur_metrics[j] = 0;
		VM->ewma_metrics[j] = 0;
	}

    // init perf counters for each core of the VM
    VM->core_index = 0;
    for_each_core(VM->core_set, __vm_perf_counter_setup_core, VM);
}

static void vm_perf_counter_teardown(struct vm_instance *VM)
{
    uint64_t count, new_cum_count;

    for (int j = 0; j < PERF_EVENT_TYPE_MAX; ++j) {
		new_cum_count = 0;
		for (int k = 0; k < VM->num_cores; ++k) {
			count = perf_event_read(VM->perf_event_fds[j][k]);
			new_cum_count += count;
			perf_event_teardown(VM->perf_event_fds[j][k]);
		}
		if (new_cum_count < VM->cum_perf_event_counts[j]) {
			new_cum_count = VM->cum_perf_event_counts[j];
		}
		VM->delta_perf_event_counts[j] = new_cum_count - VM->cum_perf_event_counts[j];
		VM->cum_perf_event_counts[j] = new_cum_count;
		free(VM->perf_event_fds[j]);
	}
    //memset(VM->cum_perf_event_counts, 0, sizeof(VM->cum_perf_event_counts));
    //memset(VM->delta_perf_event_counts, 0, sizeof(VM->delta_perf_event_counts));
    //memset(VM->cur_metrics, 0, sizeof(VM->cur_metrics));
    //memset(VM->ewma_metrics, 0, sizeof(VM->ewma_metrics));
}

static void __vm_num_core_update(int core_id __attribute__((unused)), void *arg)
{
    struct vm_instance *VM = (struct vm_instance *)arg;

    VM->num_cores++;
}

int vm_mngr_instance_create(int vm_id, char *core_set)
{
    struct vm_instance *VM;

    if (g_vm_mngr.count >= MAX_VM_INSTANCE) {
        fprintf(stderr, "cannot create more VMs\n");
        return -1;
    }

    for (int i = 0; i < MAX_VM_INSTANCE; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            break;
        }
    }

    BUG_ON(VM->initialized);
    VM->vm_id = vm_id;
    for_each_core(core_set, __vm_num_core_update, VM); // parse number of cores
    snprintf(VM->core_set, sizeof(VM->core_set), "%s", core_set);
    vm_perf_counter_setup(VM);
    VM->initialized = true;

    g_vm_mngr.count++;
    printf("VM %d created, core number: %d, coreset %s\n",
            VM->vm_id, VM->num_cores, VM->core_set);

    return 0;
}

void vm_mngr_instance_destroy(int vm_id)
{
    struct vm_instance *VM;

    for (int i = 0; i < MAX_VM_INSTANCE; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (VM->vm_id == vm_id && VM->initialized) {
            VM->vm_id = -1;
            VM->num_cores = 0;
            memset(VM->core_set, 0, sizeof(VM->core_set));
            vm_perf_counter_teardown(VM);
            VM->initialized = false;

            g_vm_mngr.count--;
            printf("VM %d destroyed\n", vm_id);
            return;
        }
    }

    fprintf(stderr, "cannot find VM instance: %d\n", vm_id);
}

void update_perf_counters(void)
{
    struct vm_instance *VM;

    for (int i = 0; i < MAX_VM_INSTANCE; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            continue;
        }
        for (int j = 0; j < PERF_EVENT_TYPE_MAX; ++j) {
			uint64_t new_cum_count = 0;
			for (int k = 0; k < VM->num_cores; ++k) {
				uint64_t count = perf_event_read(VM->perf_event_fds[j][k]);
				new_cum_count += count;
			}
			if (new_cum_count < VM->cum_perf_event_counts[j]) {
				new_cum_count = VM->cum_perf_event_counts[j];
			}
			VM->delta_perf_event_counts[j] = new_cum_count - VM->cum_perf_event_counts[j];
			VM->cum_perf_event_counts[j] = new_cum_count;
		}
    }
}
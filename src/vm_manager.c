#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "perf_counter.h"
#include "util_common.h"
#include "vm_manager.h"
#include "guest_agent.h"

struct vm_instance_manager {
    int count;
    // TODO: use TAILQ instead
    struct vm_instance VMs[MAX_NUM_VM];
};

static struct vm_instance_manager g_vm_mngr;

void vm_mngr_for_each_vm(vm_handler_fn vm_handler, void *arg)
{
    struct vm_instance *VM;

    if (g_vm_mngr.count == 0) {
        return;
    }

    for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            continue;
        }

        vm_handler(VM, arg);
    }
}

void  vm_mngr_for_each_core(struct vm_instance *VM, core_handler_fn core_handler, void *arg)
{
    BUG_ON(VM->core_set[0] == '\0');
    char *copy = strdup(VM->core_set);
    char *token = strtok(copy, ",");

    // format: aa-bb, xx-yy
    while (token) {
        int start = -1, end = -1;

        if (sscanf(token, "%d-%d", &start, &end) == 2) {
            // core range（e.g., "20-39"）
            if (start <= end) {
                for (int i = start; i <= end; ++i) {
                    core_handler(VM, i, arg);
                }
            }
        } else if (sscanf(token, "%d", &start) == 1) {
            // single core（e.g., "41"）
            core_handler(VM, start, arg);
        }

        token = strtok(NULL, ",");
    }
    free(copy);
}

#ifdef ENABLE_PERF
static void __vm_perf_counter_setup_core(struct vm_instance *VM, int core_id,
                        void *arg __attribute__((unused)))
{
    int leader_fd = -1;

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
    VM->ewma_initialized = false;

    // init perf counters for each core of the VM
    VM->core_index = 0;
    vm_mngr_for_each_core(VM, __vm_perf_counter_setup_core, NULL);
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
#endif

static void __vm_num_core_update(struct vm_instance *VM,
                        int core_id __attribute__((unused)),
                        void *arg __attribute__((unused)))
{
    VM->num_cores++;
}

static void vm_struct_reset(struct vm_instance *VM)
{
    memset(VM->core_set, 0, sizeof(VM->core_set));
    VM->initialized = false;
}

static bool vm_mngr_check_exit(int vm_id)
{
    struct vm_instance *VM;

    for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (VM->vm_id == vm_id && VM->initialized) {
            return true;
        }
    }

    return false;
}

int vm_mngr_instance_create(int vm_id, char *core_set)
{
    int ret;
    struct vm_instance *VM;

    if (g_vm_mngr.count >= MAX_NUM_VM) {
        fprintf(stderr, "cannot create more VMs\n");
        return -1;
    }

    // check VM existing
    if (vm_mngr_check_exit(vm_id)) {
        fprintf(stderr, "Already initialized VM %d\n", vm_id);
        return -1;
    }

    // find a free enrty
    // TODO: use TAILQ instead
    for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            break;
        }
    }

    BUG_ON(VM->initialized);
    VM->vm_id = vm_id;
    VM->num_cores = 0;
    // TODO: check core overlap with other VMs
    snprintf(VM->core_set, sizeof(VM->core_set), "%s", core_set);
    vm_mngr_for_each_core(VM, __vm_num_core_update, NULL);
#ifdef ENABLE_PERF
    vm_perf_counter_setup(VM);
#endif
    ret = guest_agent_init(vm_id);
    if (ret < 0) {
        fprintf(stderr, "Failed to init guest agent for vm %d\n", vm_id);
#ifdef ENABLE_PERF
        vm_perf_counter_teardown(VM);
#endif
        vm_struct_reset(VM);
        return -1;
    }
    VM->initialized = true;

    printf("VM %d created, core number: %d, coreset %s\n",
            VM->vm_id, VM->num_cores, VM->core_set);

    g_vm_mngr.count++;

    return 0;
}

int vm_mngr_instance_destroy(int vm_id)
{
    bool found = false;
    struct vm_instance *VM;

    for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (VM->vm_id == vm_id && VM->initialized) {
            found = true;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "cannot find VM instance: %d\n", vm_id);
        return -1;
    }

    guest_agent_cleanup(VM->vm_id);
#ifdef ENABLE_PERF
    vm_perf_counter_teardown(VM);
#endif
    vm_struct_reset(VM);

    g_vm_mngr.count--;
    printf("VM %d destroyed\n", vm_id);

    return 0;
}

#ifdef ENABLE_PERF
void vm_mngr_update_perf_counters(void)
{
    struct vm_instance *VM;

    if (g_vm_mngr.count == 0) {
        return;
    }

    for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            continue;
        }
#ifdef ENABLE_DEBUG
    printf("###### VM %d Perf Events:\n", VM->vm_id);
#endif
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
#ifdef ENABLE_DEBUG
            printf("Event: %-60s: %lu\n", perf_event_name_arr[j], new_cum_count);
#endif
		}
    }
}

#define TSC_FREQ 2.1e9
void vm_mngr_update_metrics(int operation_window_s __attribute__((unused)))
{
    double ewma_constant = 0.2;
    struct vm_instance *VM;

    if (g_vm_mngr.count == 0) {
        return;
    }

	for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            continue;
        }

        // L3 miss latency (i.e., main memory access latency)
		VM->cur_metrics[METRIC_TYPE_LOAD_L3_MISS_LAT] =
			1e9 * ((double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD]
					/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD])
			/ ((double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD]
				/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC]
				* TSC_FREQ);

		for (int j = 0; j < METRIC_TYPE_MAX; ++j) {
			VM->cur_metrics[j] = MIN(metric_max_arr[j], MAX(0.0, VM->cur_metrics[j]));
		}

		// Calculate EWMA
		for (int j = 0; j < METRIC_TYPE_MAX; ++j) {
			if (VM->ewma_initialized) {
				VM->ewma_metrics[j] = ewma_constant * VM->cur_metrics[j] + (1 - ewma_constant) * VM->ewma_metrics[j];
			} else {
				VM->ewma_metrics[j] = VM->cur_metrics[j];
			}
		}
		VM->ewma_initialized = true;

#ifdef ENABLE_DEBUG
        printf("###### VM %d Metrics:\n", VM->vm_id);
        for (int j = 0; j < METRIC_TYPE_MAX; ++j) {
            printf("Metric: %-60s: %f\n", metric_name_arr[j], VM->cur_metrics[j]);
        }
#endif
	}
}
#endif
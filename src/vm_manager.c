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

    for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            continue;
        }

        vm_handler(VM, arg);
    }
}

static void for_each_core(const char *core_set, core_handler_fn core_handler, void *arg)
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
    VM->ewma_initialized = false;

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
    for_each_core(core_set, __vm_num_core_update, VM); // parse and get number of cores
    snprintf(VM->core_set, sizeof(VM->core_set), "%s", core_set);
    vm_perf_counter_setup(VM);
    ret = guest_agent_init(vm_id);
    if (ret < 0) {
        fprintf(stderr, "Failed to init guest agent for vm %d\n", vm_id);
        vm_perf_counter_teardown(VM);
        vm_struct_reset(VM);
        return -1;
    }
    VM->initialized = true;

    printf("VM %d created, core number: %d, coreset %s\n",
            VM->vm_id, VM->num_cores, VM->core_set);

    g_vm_mngr.count++;

    return 0;
}

void vm_mngr_instance_destroy(int vm_id)
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
        return;
    }

    guest_agent_cleanup(VM->vm_id);
    vm_perf_counter_teardown(VM);
    vm_struct_reset(VM);

    g_vm_mngr.count--;
    printf("VM %d destroyed\n", vm_id);
}

void vm_mngr_update_perf_counters(void)
{
    struct vm_instance *VM;

    for (int i = 0; i < MAX_NUM_VM; i++) {
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

#define TSC_FREQ 2.2e9
void vm_mngr_update_metrics(int operation_window_s)
{
    double ewma_constant = 0.2;
    struct vm_instance *VM;
	for (int i = 0; i < MAX_NUM_VM; i++) {
        VM = &g_vm_mngr.VMs[i];
        if (!VM->initialized) {
            continue;
        }
#if 0
		VM->cur_metrics[METRIC_TYPE_DTLB_LOAD_MISS_LAT] =
			(double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_DTLB_LOAD_MISSES_WALK_ACTIVE]
			/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_DTLB_LOAD_MISSES_WALK_COMPLETED];
		VM->cur_metrics[METRIC_TYPE_DLTB_LOAD_MPI] =
			(double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_DTLB_LOAD_MISSES_WALK_COMPLETED]
			/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_INST_RETIRED];
		VM->cur_metrics[METRIC_TYPE_LOAD_L3_MISS_LAT] =
			1e9 * ((double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD]
					/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD])
			/ ((double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD]
				/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC]
				* TSC_FREQ);
		VM->cur_metrics[METRIC_TYPE_LOAD_L2_MISS_LAT] =
			1e9 * ((double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_OUTSTANDING_DEMAND_DATA_RD]
					/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_DEMAND_DATA_RD])
			/ ((double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD]
				/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC]
				* TSC_FREQ);
		VM->cur_metrics[METRIC_TYPE_L2_DEMAND_DATA_RD_MPI] =
			(double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_MEM_LOAD_RETIRED_L2_MISS]
			/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_INST_RETIRED];
		VM->cur_metrics[METRIC_TYPE_L2_MPI] =
			(double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_L2_LINES_IN_ALL]
			/ (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_INST_RETIRED];
#endif
		double a, b;
		a = (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_READS_TO_CORE_L3_MISS_LOCAL_SOCKET];
		b = (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_HWPF_L3_L3_MISS_LOCAL];
		double read_count = a + b;
		VM->cur_metrics[METRIC_TYPE_CORE_READ] = read_count * 64 / 1e6;
		VM->cur_metrics[METRIC_TYPE_CORE_READ] /= operation_window_s;

		a = (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_MODIFIED_WRITE_ANY_RESPONSE];
		b = (double) VM->delta_perf_event_counts[PERF_EVENT_TYPE_OCR_RFO_TO_CORE_L3_HIT_M];
		double write_count = MAX(0.0, (a - b));
		VM->cur_metrics[METRIC_TYPE_CORE_WRITE] = (a - b) * 64 / 1e6;
		VM->cur_metrics[METRIC_TYPE_CORE_WRITE] /= operation_window_s;
#if 0
		VM->cur_metrics[METRIC_TYPE_2LM_MISS_RATIO] = L3_MISS_LAT_TO_2LM_MR_INTERCEPT
			+ L3_MISS_LAT_TO_2LM_MR_SLOPE * VM->cur_metrics[METRIC_TYPE_LOAD_L3_MISS_LAT];
		VM->cur_metrics[METRIC_TYPE_2LM_MISS_RATIO] = min(1.0, max(0.0, VM->cur_metrics[METRIC_TYPE_2LM_MISS_RATIO]));

		VM->cur_metrics[METRIC_TYPE_2LM_MPKI] =
			1e3 * VM->cur_metrics[METRIC_TYPE_2LM_MISS_RATIO]
			* (read_count + write_count)
			/ VM->delta_perf_event_counts[PERF_EVENT_TYPE_INST_RETIRED];

		VM->cur_metrics[METRIC_TYPE_2LM_PER_PAGE_MISS_RATE] =
			VM->cur_metrics[METRIC_TYPE_2LM_MISS_RATIO]
			* (read_count + write_count)
			/ (vm_count_all_contended(VM) + ((128 << 20) >> COLOR_PAGE_SHIFT))
			/ operation_window_s;

		for (int j = 0; j < METRIC_TYPE_MAX; ++j) {
			VM->cur_metrics[j] = min(metric_max_arr[j], max(0.0, VM->cur_metrics[j]));
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
#endif
	}
}
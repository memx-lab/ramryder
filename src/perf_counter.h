#ifndef PERF_COUNTER_H
#define PERF_COUNTER_H

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <linux/perf_event.h>
#include <pcm/pcm_c_public.h>

enum PerfEventType {
	// basic counters
	PERF_EVENT_TYPE_INST_RETIRED = 0,
	PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD,
	PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC,

	// L3 related counters
	PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD,
	PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD,

	PERF_EVENT_TYPE_MAX
};

extern uint64_t perf_event_config_arr[PERF_EVENT_TYPE_MAX];
extern uint64_t perf_event_config1_arr[PERF_EVENT_TYPE_MAX];
extern bool perf_event_is_leader_arr[PERF_EVENT_TYPE_MAX];
extern bool perf_event_pin_arr[PERF_EVENT_TYPE_MAX];
extern const char *perf_event_name_arr[PERF_EVENT_TYPE_MAX];

// VM metric based on perf events
enum MetricType {
	METRIC_TYPE_LOAD_L3_MISS_LAT, // metric_Load_L3_Miss_Latency_using_ORO_events(ns)

	METRIC_TYPE_MAX,
};

extern const char *metric_name_arr[METRIC_TYPE_MAX];
extern double metric_max_arr[METRIC_TYPE_MAX];

void perf_event_reset(int event_fd);
void perf_event_enable(int event_fd);
void perf_event_disable(int event_fd);
void perf_event_teardown(int fd);
int perf_event_setup(int core, uint64_t config, uint64_t config1, bool pin_event,
	bool exclude_user, bool exclude_kernel, bool exclude_hv, bool exclude_idle,
	bool exclude_host, bool exclude_guest, int group_fd);
uint64_t perf_event_read(int event_fd);

// system-wide events provided by PCM
int perf_agent_init(void);
void perf_agent_cleanup(void);
void perf_agent_get_metrics(memdata_t *md, core_metrics_t *core_metrics, bool output);
uint32_t perf_agent_get_num_cores();

#endif
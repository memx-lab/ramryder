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
	PERF_EVENT_TYPE_INST_RETIRED = 0,
	PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD,
	PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC,
	PERF_EVENT_TYPE_DTLB_LOAD_MISSES_WALK_ACTIVE,
	PERF_EVENT_TYPE_DTLB_LOAD_MISSES_WALK_COMPLETED,
	PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD,
	PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD,
	PERF_EVENT_TYPE_OCR_OUTSTANDING_DEMAND_DATA_RD,
	PERF_EVENT_TYPE_OCR_DEMAND_DATA_RD,
	PERF_EVENT_TYPE_MEM_LOAD_RETIRED_L2_MISS,
	PERF_EVENT_TYPE_L2_LINES_IN_ALL,
	PERF_EVENT_TYPE_OCR_MODIFIED_WRITE_ANY_RESPONSE,
	PERF_EVENT_TYPE_OCR_RFO_TO_CORE_L3_HIT_M,
	PERF_EVENT_TYPE_OCR_READS_TO_CORE_L3_MISS_LOCAL_SOCKET,
	PERF_EVENT_TYPE_OCR_HWPF_L3_L3_MISS_LOCAL,
	PERF_EVENT_TYPE_MAX
};

extern uint64_t perf_event_config_arr[PERF_EVENT_TYPE_MAX];
extern uint64_t perf_event_config1_arr[PERF_EVENT_TYPE_MAX];
extern bool perf_event_is_leader_arr[PERF_EVENT_TYPE_MAX];
extern bool perf_event_pin_arr[PERF_EVENT_TYPE_MAX];
extern const char *perf_event_name_arr[PERF_EVENT_TYPE_MAX];

// VM metric based on perf events
enum MetricType {
	METRIC_TYPE_DTLB_LOAD_MISS_LAT = 0,		// metric_DTLB load miss latency (in core clks)
	METRIC_TYPE_DLTB_LOAD_MPI,				// metric_DTLB (2nd level) load MPI
	METRIC_TYPE_LOAD_L3_MISS_LAT,			// metric_Load_L3_Miss_Latency_using_ORO_events(ns)
	METRIC_TYPE_LOAD_L2_MISS_LAT,			// metric_Load_L2_Miss_Latency_using_ORO_events(ns)
	METRIC_TYPE_L2_DEMAND_DATA_RD_MPI,		// metric_L2 demand data read MPI
	METRIC_TYPE_L2_MPI,						// metric_L2 MPI (includes code+data+rfo w/ prefetches)
	METRIC_TYPE_CORE_READ,					// metric_core initiated local socket memory read bandwidth (MB/sec)
	METRIC_TYPE_CORE_WRITE,					// metric_core initiated write bandwidth (MB/sec)
	METRIC_TYPE_2LM_MISS_RATIO,				// metric_memory mode near memory cache read miss rate% (estimated)
	METRIC_TYPE_2LM_MPKI,					// metric_memory mode MPKI (estimated)
	METRIC_TYPE_2LM_PER_PAGE_MISS_RATE,		// metric_memory mode per-page miss rate (estimated)
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

// system-wide uncore events provided by PCM
int perf_uncore_agent_init(void);
void perf_uncore_agent_cleanup(void);
void perf_uncore_agent_get_bandwidth(memdata_t *md, bool output);

#endif
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
#include "util_common.h"
#include "perf_counter.h"

#define PERF_READ_FORMAT (PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING)

struct perf_read_format {
	uint64_t value;
	uint64_t time_enabled;
	uint64_t time_running;
};

// Config format: X86_RAW_EVENT_MASK
uint64_t perf_event_config_arr[] = {
	0x000000C0UL,	// INST_RETIRED.ANY
	0x0000003CUL,	// CPU_CLK_UNHALTED.THREAD
	0x00000300UL,	// CPU_CLK_UNHALTED.REF_TSC
	0x01001012UL,	// DTLB_LOAD_MISSES.WALK_ACTIVE
	0x00000E12UL,	// DTLB_LOAD_MISSES.WALK_COMPLETED
	0x00001020UL,	// OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD
	0x00001021UL,	// OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD
	0x00000120UL,	// OFFCORE_REQUESTS_OUTSTANDING.DEMAND_DATA_RD
	0x00000121UL,	// OFFCORE_REQUESTS.DEMAND_DATA_RD
	0x000010D1UL,	// MEM_LOAD_RETIRED.L2_MISS
	0x00001F25UL,	// L2_LINES_IN.ALL
	0x0000012AUL,	// OCR.MODIFIED_WRITE.ANY_RESPONSE
	0x0000012BUL,	// OCR.RFO_TO_CORE.L3_HIT_M
	0x0000012AUL,	// OCR.READS_TO_CORE.L3_MISS_LOCAL_SOCKET
	0x0000012BUL,	// OCR.HWPF_L3.L3_MISS_LOCAL
};
STATIC_ASSERT(
	(sizeof(perf_event_config_arr) / sizeof(*perf_event_config_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_config_arr does not match with PERF_EVENT_TYPE_MAX");

uint64_t perf_event_config1_arr[] = {
	0x0000000000UL,	// INST_RETIRED.ANY
	0x0000000000UL,	// CPU_CLK_UNHALTED.THREAD
	0x0000000000UL,	// CPU_CLK_UNHALTED.REF_TSC
	0x0000000000UL,	// DTLB_LOAD_MISSES.WALK_ACTIVE
	0x0000000000UL,	// DTLB_LOAD_MISSES.WALK_COMPLETED
	0x0000000000UL,	// OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD
	0x0000000000UL,	// OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD
	0x0000000000UL,	// OFFCORE_REQUESTS_OUTSTANDING.DEMAND_DATA_RD
	0x0000000000UL,	// OFFCORE_REQUESTS.DEMAND_DATA_RD
	0x0000000000UL,	// MEM_LOAD_RETIRED.L2_MISS
	0x0000000000UL,	// L2_LINES_IN.ALL
	0x0000010808UL,	// OCR.MODIFIED_WRITE.ANY_RESPONSE
	0x1F80040022UL,	// OCR.RFO_TO_CORE.L3_HIT_M
	0x070CC04477UL,	// OCR.READS_TO_CORE.L3_MISS_LOCAL_SOCKET
	0x0084002380UL,	// OCR.HWPF_L3.L3_MISS_LOCAL
};
STATIC_ASSERT(
	(sizeof(perf_event_config1_arr) / sizeof(*perf_event_config_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_config1_arr does not match with PERF_EVENT_TYPE_MAX");

bool perf_event_is_leader_arr[] = {
	true,	// INST_RETIRED.ANY
	true,	// CPU_CLK_UNHALTED.THREAD
	true,	// CPU_CLK_UNHALTED.REF_TSC
	true,	// DTLB_LOAD_MISSES.WALK_ACTIVE
	false,	// DTLB_LOAD_MISSES.WALK_COMPLETED
	true,	// OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD
	true,	// OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD
	true,	// OFFCORE_REQUESTS_OUTSTANDING.DEMAND_DATA_RD
	false,	// OFFCORE_REQUESTS.DEMAND_DATA_RD
	true,	// MEM_LOAD_RETIRED.L2_MISS
	true,	// L2_LINES_IN.ALL
	true,	// OCR.MODIFIED_WRITE.ANY_RESPONSE
	false,	// OCR.RFO_TO_CORE.L3_HIT_M
	true,	// OCR.READS_TO_CORE.L3_MISS_LOCAL_SOCKET
	false,	// OCR.HWPF_L3.L3_MISS_LOCAL
};
STATIC_ASSERT(
	(sizeof(perf_event_is_leader_arr) / sizeof(*perf_event_is_leader_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_is_leader_arr does not match with PERF_EVENT_TYPE_MAX");

bool perf_event_pin_arr[] = {
	true,	// INST_RETIRED.ANY
	true,	// CPU_CLK_UNHALTED.THREAD
	true,	// CPU_CLK_UNHALTED.REF_TSC
	false,	// DTLB_LOAD_MISSES.WALK_ACTIVE
	false,	// DTLB_LOAD_MISSES.WALK_COMPLETED
	true,	// OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD
	true,	// OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD
	false,	// OFFCORE_REQUESTS_OUTSTANDING.DEMAND_DATA_RD
	false,	// OFFCORE_REQUESTS.DEMAND_DATA_RD
	false,	// MEM_LOAD_RETIRED.L2_MISS
	false,	// L2_LINES_IN.ALL
	false,	// OCR.MODIFIED_WRITE.ANY_RESPONSE
	false,	// OCR.RFO_TO_CORE.L3_HIT_M
	false,	// OCR.READS_TO_CORE.L3_MISS_LOCAL_SOCKET
	false,	// OCR.HWPF_L3.L3_MISS_LOCAL
};
STATIC_ASSERT(
	(sizeof(perf_event_pin_arr) / sizeof(*perf_event_pin_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_pin_arr does not match with PERF_EVENT_TYPE_MAX");

const char *perf_event_name_arr[] = {
	"INST_RETIRED.ANY",
	"CPU_CLK_UNHALTED.THREAD",
	"CPU_CLK_UNHALTED.REF_TSC",
	"DTLB_LOAD_MISSES.WALK_ACTIVE",
	"DTLB_LOAD_MISSES.WALK_COMPLETED",
	"OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD",
	"OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD",
	"OFFCORE_REQUESTS_OUTSTANDING.DEMAND_DATA_RD",
	"OFFCORE_REQUESTS.DEMAND_DATA_RD",
	"MEM_LOAD_RETIRED.L2_MISS",
	"L2_LINES_IN.ALL",
	"OCR.MODIFIED_WRITE.ANY_RESPONSE",
	"OCR.RFO_TO_CORE.L3_HIT_M",
	"OCR.READS_TO_CORE.L3_MISS_LOCAL_SOCKET",
	"OCR.HWPF_L3.L3_MISS_LOCAL",
};
STATIC_ASSERT(
	(sizeof(perf_event_name_arr) / sizeof(*perf_event_name_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_name_arr does not match with PERF_EVENT_TYPE_MAX");


// VM metric based on perf events
const char *metric_name_arr[] = {
	"metric_DTLB load miss latency (in core clks)",
	"metric_DTLB (2nd level) load MPI",
	"metric_Load_L3_Miss_Latency_using_ORO_events(ns)",
	"metric_Load_L2_Miss_Latency_using_ORO_events(ns)",
	"metric_L2 demand data read MPI",
	"metric_L2 MPI (includes code+data+rfo w/ prefetches)",
	"metric_core initiated local socket memory read bandwidth (MB/sec)",
	"metric_core initiated write bandwidth (MB/sec)",
	"metric_memory mode near memory cache read miss rate% (estimated)",
	"metric_memory mode MPKI (estimated)",
	"metric_memory mode per-page miss rate (estimated)",
};
STATIC_ASSERT(
	(sizeof(metric_name_arr) / sizeof(*metric_name_arr)) == METRIC_TYPE_MAX,
	"metric_name_arr does not match with METRIC_TYPE_MAX");

double metric_max_arr[] = {
	200,							// metric_DTLB load miss latency (in core clks)
	0.5,							// metric_DTLB (2nd level) load MPI
	350,							// metric_Load_L3_Miss_Latency_using_ORO_events(ns)
	200,							// metric_Load_L2_Miss_Latency_using_ORO_events(ns)
	0.5,							// metric_L2 demand data read MPI
	0.5,							// metric_L2 MPI (includes code+data+rfo w/ prefetches)
	512000,							// metric_core initiated local socket memory read bandwidth (MB/sec)
	512000,							// metric_core initiated write bandwidth (MB/sec)
	100,							// metric_memory mode near memory cache read miss rate% (estimated)
	100,							// metric_memory mode MPKI (estimated)
	//numeric_limits<double>::max(),	// metric_memory mode per-page miss rate (estimated)
    512000,
};
STATIC_ASSERT(
	(sizeof(metric_max_arr) / sizeof(*metric_max_arr)) == METRIC_TYPE_MAX,
	"metric_max_arr does not match with METRIC_TYPE_MAX");


static int perf_event_open(struct perf_event_attr *attr,
	pid_t pid, int core, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, core, group_fd, flags);
}

void perf_event_reset(int event_fd)
{
	int ret = ioctl(event_fd, PERF_EVENT_IOC_RESET, 0);
	BUG_ON(ret < 0);
}

void perf_event_enable(int event_fd)
{
	int ret = ioctl(event_fd, PERF_EVENT_IOC_ENABLE, 0);
	BUG_ON(ret < 0);
}

void perf_event_disable(int event_fd)
{
	int ret = ioctl(event_fd, PERF_EVENT_IOC_DISABLE, 0);
	BUG_ON(ret < 0);
}

void perf_event_teardown(int fd)
{
	int ret;
	ret = close(fd);
	BUG_ON(ret != 0);
}

int perf_event_setup(int core, uint64_t config, uint64_t config1, bool pin_event,
	bool exclude_user, bool exclude_kernel, bool exclude_hv, bool exclude_idle,
	bool exclude_host, bool exclude_guest, int group_fd)
{
	struct perf_event_attr event_attr;
	memset(&event_attr, 0, sizeof(event_attr));
	event_attr.type = PERF_TYPE_RAW;
	event_attr.size = sizeof(event_attr);
	event_attr.config = config;
	event_attr.config1 = config1;
	event_attr.disabled = 0;
	event_attr.read_format = PERF_READ_FORMAT;
	if (pin_event)
		event_attr.pinned = 1;
	if (exclude_user)
		event_attr.exclude_user = 1;
	if (exclude_kernel)
		event_attr.exclude_kernel = 1;
	if (exclude_hv)
		event_attr.exclude_hv = 1;
	if (exclude_idle)
		event_attr.exclude_idle = 1;
	if (exclude_host)
		event_attr.exclude_host = 1;
	if (exclude_guest)
		event_attr.exclude_guest = 1;

	int fd = perf_event_open(&event_attr, -1, core, group_fd, 0);
	return fd;
}

uint64_t perf_event_read(int event_fd)
{
	struct perf_read_format data;
	int ret = read(event_fd, &data, sizeof(data));
	BUG_ON(ret != sizeof(data));
	double value = ((double) data.value) * ((double) data.time_enabled) / ((double) data.time_running);
	return (uint64_t) value;
}


int perf_uncore_agent_init(void)
{
    if (pcm_c_init() != 0) {
        perror("Filed to init  PCM\n");
        return -1;
    }
 
    /* Start to record uncore counter usage */
    pcm_c_start();

    return 0;
}

void perf_uncore_agent_cleanup(void)
{
    pcm_c_cleanup();
}

void perf_uncore_agent_get_bandwidth(memdata_t *md, bool output)
{
    pcm_c_get_bandwidth(md, output);
}

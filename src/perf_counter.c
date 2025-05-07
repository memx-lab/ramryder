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
#include <linux/perf_event.h>    /* Definition of PERF_* constants */
#include <linux/hw_breakpoint.h> /* Definition of HW_* constants */
#include <sys/syscall.h>         /* Definition of SYS_* constants */
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
	// basic counters: config_arr are same for different CPU arch?
	[PERF_EVENT_TYPE_INST_RETIRED] = 0x000000C0UL,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD] = 0x0000003CUL,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC] = 0x00000300UL,

	[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD] = 0x00001060UL,
	[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD] = 0x000010b0UL,
};
STATIC_ASSERT(
	(sizeof(perf_event_config_arr) / sizeof(*perf_event_config_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_config_arr does not match with PERF_EVENT_TYPE_MAX");

uint64_t perf_event_config1_arr[] = {
	[PERF_EVENT_TYPE_INST_RETIRED] = 0x0000000000UL,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD] = 0x0000000000UL,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC] = 0x0000000000UL,

	[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD] = 0x0000000000UL,
	[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD] = 0x0000000000UL,
};
STATIC_ASSERT(
	(sizeof(perf_event_config1_arr) / sizeof(*perf_event_config_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_config1_arr does not match with PERF_EVENT_TYPE_MAX");

bool perf_event_is_leader_arr[] = {
	[PERF_EVENT_TYPE_INST_RETIRED] = true,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD] = true,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC] = true,

	[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD] = true,
	[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD] = true,
};
STATIC_ASSERT(
	(sizeof(perf_event_is_leader_arr) / sizeof(*perf_event_is_leader_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_is_leader_arr does not match with PERF_EVENT_TYPE_MAX");

bool perf_event_pin_arr[] = {
	[PERF_EVENT_TYPE_INST_RETIRED] = true,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD] = true,
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC] = true,

	[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD] = true,
	[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD] = false,
};
STATIC_ASSERT(
	(sizeof(perf_event_pin_arr) / sizeof(*perf_event_pin_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_pin_arr does not match with PERF_EVENT_TYPE_MAX");

const char *perf_event_name_arr[] = {
	[PERF_EVENT_TYPE_INST_RETIRED]             = "INST_RETIRED.ANY_P",
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_THREAD]  = "CPU_CLK_UNHALTED.THREAD_P",
	[PERF_EVENT_TYPE_CPU_CLK_UNHALTED_REF_TSC] = "CPU_CLK_UNHALTED.REF_TSC",

	[PERF_EVENT_TYPE_OCR_OUTSTANDING_L3_MISS_DEMAND_DATA_RD] = "OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD",
	[PERF_EVENT_TYPE_OCR_L3_MISS_DEMAND_DATA_RD]             = "OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD",
};
STATIC_ASSERT(
	(sizeof(perf_event_name_arr) / sizeof(*perf_event_name_arr)) == PERF_EVENT_TYPE_MAX,
	"perf_event_name_arr does not match with PERF_EVENT_TYPE_MAX");


// VM metric based on perf events
const char *metric_name_arr[] = {
	[METRIC_TYPE_LOAD_L3_MISS_LAT] = "metric_Load_L3_Miss_Latency_using_ORO_events(ns)",
};
STATIC_ASSERT(
	(sizeof(metric_name_arr) / sizeof(*metric_name_arr)) == METRIC_TYPE_MAX,
	"metric_name_arr does not match with METRIC_TYPE_MAX");

double metric_max_arr[] = {
	[METRIC_TYPE_LOAD_L3_MISS_LAT] = 350, // metric_Load_L3_Miss_Latency_using_ORO_events(ns)
};
STATIC_ASSERT(
	(sizeof(metric_max_arr) / sizeof(*metric_max_arr)) == METRIC_TYPE_MAX,
	"metric_max_arr does not match with METRIC_TYPE_MAX");


// raw perf event system calls
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

// system-wide event APIs provided by PCM public libiary
int perf_agent_init(void)
{
    if (pcm_c_init() != 0) {
        perror("Filed to init  PCM\n");
        return -1;
    }
 
    /* Start to record uncore counter usage */
    pcm_c_start();

    return 0;
}

void perf_agent_cleanup(void)
{
    pcm_c_cleanup();
}

void perf_agent_get_metrics(memdata_t *md, core_metrics_t *core_metrics, bool output)
{
	pcm_c_get_metrics(md, core_metrics, output);
}

uint32_t perf_agent_get_num_cores()
{
	return pcm_c_get_num_cores();
}
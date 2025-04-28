#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include "perf_counter.h"

#if 0
const char *perf_event_name_arr[] = {
    "INST_RETIRED.ANY_P",
    "CPU_CLK_UNHALTED.THREAD_P",
    "CPU_CLK_UNHALTED.REF_TSC",
    "DTLB_LOAD_MISSES.WALK_ACTIVE",
    "DTLB_LOAD_MISSES.WALK_COMPLETED",
    "OFFCORE_REQUESTS_OUTSTANDING.L3_MISS_DEMAND_DATA_RD",
    "OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD",
    "OFFCORE_REQUESTS_OUTSTANDING.DEMAND_DATA_RD",
    "OFFCORE_REQUESTS.DEMAND_DATA_RD",
    "MEM_LOAD_RETIRED.L2_MISS",
    "L2_LINES_IN.ALL",
    "OCR.MODIFIED_WRITE_ANY_RESPONSE",
    "OCR.RFO_TO_CORE_L3_HIT_M",
    "OCR.READS_TO_CORE_L3_MISS_LOCAL_SOCKET",
    "OCR.HWPF_L3_L3_MISS_LOCAL",
};
#endif

void print_perf_event_attr(const struct perf_event_attr *attr, const char *name, const char *fstr) {
    printf("Event: %-50s\n", name);
    printf("  config     = 0x%016llx\n", (unsigned long long)attr->config);
    printf("  config1    = 0x%016llx\n", (unsigned long long)attr->config1);
    printf("  type       = %u\n", attr->type);
    printf("  size       = %u\n", attr->size);
    printf("  disabled   = %u\n", attr->disabled);
    printf("  exclude_user   = %u\n", attr->exclude_user);
    printf("  exclude_kernel = %u\n", attr->exclude_kernel);
    printf("  pinned     = %u\n", attr->pinned);
    printf("  read_format = 0x%lx\n", (unsigned long)attr->read_format);
    if (fstr) {
        printf("  fstr       = %s\n", fstr);
    }
    printf("------------------------------------------------------------\n");
}

int main(void) {
    if (pfm_initialize() != PFM_SUCCESS) {
        fprintf(stderr, "Failed to initialize libpfm\n");
        return 1;
    }

    size_t num_events = sizeof(perf_event_name_arr) / sizeof(perf_event_name_arr[0]);
    for (size_t i = 0; i < num_events; ++i) {
        struct perf_event_attr attr = {};
        pfm_perf_encode_arg_t arg = {};
        char *fstr = NULL;

        memset(&attr, 0, sizeof(attr));
        memset(&arg, 0, sizeof(arg));

        attr.size = sizeof(attr);
        arg.attr = &attr;
        arg.size = sizeof(arg);
        arg.cpu = -1;
        arg.fstr = &fstr;
        arg.flags = 0;

        int ret = pfm_get_os_event_encoding(perf_event_name_arr[i],
                                            PFM_PLM3,
                                            PFM_OS_PERF_EVENT,
                                            &arg);

        if (ret != PFM_SUCCESS) {
            fprintf(stderr, "[!] Error encoding event '%s': %s\n", perf_event_name_arr[i], pfm_strerror(ret));
            continue;
        }

        print_perf_event_attr(&attr, perf_event_name_arr[i], fstr);

        if (fstr) free(fstr);
    }

    pfm_terminate();
    return 0;
}

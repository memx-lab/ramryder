#ifndef PCM_C_PUBLIC_H
#define PCM_C_PUBLIC_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_SOCKETS 256
#define MAX_CONTROLLER 32
#define MAX_CHANNELS 32
#define MAX_CXL_PORTS 6

// use same definiton as pcm
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef enum {
    PartialWrites,
    Pmem,
    PmemMemoryMode,
    PmemMixedMode
} ServerUncoreMemoryMetrics;

// the following structures should be exactly same as that in pcm_memory.cpp
#define MAX_CORES 4096
typedef struct core_metrics {
    uint64 core_local_bw[MAX_CORES];
    uint64 core_remote_bw[MAX_CORES];
} core_metrics_t;

typedef struct memdata {
    float iMC_Rd_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float iMC_Wr_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float iMC_PMM_Rd_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float iMC_PMM_Wr_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float MemoryMode_Miss_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float iMC_Rd_socket[MAX_SOCKETS];
    float iMC_Wr_socket[MAX_SOCKETS];
    float iMC_PMM_Rd_socket[MAX_SOCKETS];
    float iMC_PMM_Wr_socket[MAX_SOCKETS];
    float CXLMEM_Rd_socket_port[MAX_SOCKETS][MAX_CXL_PORTS];
    float CXLMEM_Wr_socket_port[MAX_SOCKETS][MAX_CXL_PORTS];
    float CXLCACHE_Rd_socket_port[MAX_SOCKETS][MAX_CXL_PORTS];
    float CXLCACHE_Wr_socket_port[MAX_SOCKETS][MAX_CXL_PORTS];
    float MemoryMode_Miss_socket[MAX_SOCKETS];
    bool NM_hit_rate_supported;
    bool BHS_NM;
    bool BHS;
    float MemoryMode_Hit_socket[MAX_SOCKETS];
    bool M2M_NM_read_hit_rate_supported;
    float NM_hit_rate[MAX_SOCKETS];
    float M2M_NM_read_hit_rate[MAX_SOCKETS][MAX_CONTROLLER];
    float EDC_Rd_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float EDC_Wr_socket_chan[MAX_SOCKETS][MAX_CHANNELS];
    float EDC_Rd_socket[MAX_SOCKETS];
    float EDC_Wr_socket[MAX_SOCKETS];
    float cpuUtil[MAX_SOCKETS]; 
    uint64 partial_write[MAX_SOCKETS];
    ServerUncoreMemoryMetrics metrics;
} memdata_t;

// all must be called after pcm_c_init()
int pcm_c_init();
void pcm_c_start();
void pcm_c_cleanup();

void pcm_c_print_metrics();
void pcm_c_get_metrics(memdata_t *md, core_metrics_t *core_metrics, bool output);
uint32 pcm_c_get_num_cores();

#endif
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "guest_agent.h"
#include "guest_monitor.h"
#include "perf_counter.h"
#include "util_common.h"
#include "vm_manager.h"
#include "util_log.h"

// These are used by monitor thread to filter unreasonable results
#define MAX_BW_THRESHOLD_GB 250
#define MIN_BW_THRESHOLD_GB 2

// for monitor
static volatile int running = 1;
static pthread_t monitor_thread;
static uint32_t monitor_interval_in_second = 1;

// for cloud db
#define INFLUXDB_DATA_LENGTH 256
static bool enable_cloud_db = false;
static char *influxdb_url = NULL;
static char *influxdb_token = NULL;
static bool use_proxy = false;
static char *proxy_addr = NULL;
static CURL *g_curl = NULL;
struct curl_slist *g_headers = NULL;
static uint64_t g_time_now_s;

static int guest_monitor_load_config(const char* config_file)
{
    char line[512];
    FILE *file;

    file = fopen(config_file, "r");
    if (!file) {
        LOG_ERROR("Failed to open config file");
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' ||
            strncmp(line, "[global]", strlen("[global]")) == 0 ||
            strncmp(line, "[devices]", strlen("[devices]")) == 0 ||
            strncmp(line, "[clouddb]", strlen("[clouddb]")) == 0) {
            continue;
        }

        char key[32], value1[256], value2[256];
        if (sscanf(line, "%s %s %s", key, value1, value2) >= 2) {
            if (strcmp(key, "enable_clouddb") == 0) {
                enable_cloud_db = (strcmp(value1, "true") == 0 || strcmp(value1, "1") == 0);
            } else if (strcmp(key, "influxdb_url") == 0) {
                influxdb_url = strdup(value1);
            } else if (strcmp(key, "influxdb_token") == 0) {
                influxdb_token = strdup(value1);
            } else if (strcmp(key, "use_proxy") == 0) {
                use_proxy = (strcmp(value1, "true") == 0 || strcmp(value1, "1") == 0);
            } else if (strcmp(key, "proxy_addr") == 0) {
                proxy_addr = strdup(value1);
            } else if (strcmp(key, "monitor_interval_second") == 0) {
                monitor_interval_in_second = atoi(value1);
            }
        }
    }
#ifdef ENABLE_DEBUG
    LOG_DEBUG("Cloud DB %s, url: %s, size: %lu, token: %s, size: %lu, proxy: %s", enable_cloud_db? "enabled" : "disabled", influxdb_url, strlen(influxdb_url), influxdb_token, strlen(influxdb_token), use_proxy? proxy_addr : "No");
#endif
    fclose(file);
    return 0;
}

static int cloud_db_client_init(void)
{
    char auth_header[512];

    g_curl = curl_easy_init();
    if (g_curl == NULL) {
        LOG_ERROR("Failed to init curl");
        return -1;
    }

    g_headers = curl_slist_append(g_headers, "Content-Type: text/plain");
    snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", influxdb_token);
    g_headers = curl_slist_append(g_headers, auth_header);

    curl_easy_setopt(g_curl, CURLOPT_URL, influxdb_url);
    curl_easy_setopt(g_curl, CURLOPT_HTTPHEADER, g_headers);
    if (use_proxy) {
        curl_easy_setopt(g_curl, CURLOPT_PROXY, proxy_addr);
    }

    return 0;
}

static void cloud_db_client_cleanup(void)
{
    if (g_headers) {
        curl_slist_free_all(g_headers);
    }

    if (g_curl) {
        curl_easy_cleanup(g_curl);
    }

    if (influxdb_url) {
        free(influxdb_url);
    }

    if (influxdb_token) {
        free(influxdb_token);
    }
}

static void cloud_db_client_send(const char *data)
{
    CURLcode res;

    if (!g_curl) {
        return;
    }

    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, data);
    res = curl_easy_perform(g_curl);
    if (res != CURLE_OK) {
        LOG_ERROR("InfluxDB request failed: %s", curl_easy_strerror(res));
    }
}

static void upload_vm_cp_to_cloud_db(struct vm_instance *VM, const char *json_str)
{
    char influx_data[INFLUXDB_DATA_LENGTH];
    struct json_object *parsed_json, *return_obj, *meminfo_list, *meminfo;

    parsed_json = json_tokener_parse(json_str);
    if (!json_object_object_get_ex(parsed_json, "return", &return_obj)) {
        LOG_ERROR("Filed to parse json response: no return found");
        return;
    }

    if (!json_object_object_get_ex(return_obj, "meminfo-list", &meminfo_list)) {
        LOG_ERROR("Failed to parse json response: no meminfo-list found");
        return;
    }

    for (size_t i = 0; i < json_object_array_length(meminfo_list); i++) {
        meminfo = json_object_array_get_idx(meminfo_list, i);
        int index = json_object_get_int(json_object_object_get(meminfo, "index"));
        int mem_free = json_object_get_int(json_object_object_get(meminfo, "mem-free"));
        int mem_total = json_object_get_int(json_object_object_get(meminfo, "mem-total"));
        int mem_available = json_object_get_int(json_object_object_get(meminfo, "mem-available"));

        snprintf(influx_data, sizeof(influx_data),
            "guest_memory,vm_id=%d,node=%d memory_free=%d,memory_total=%d,memory_available=%d %lu",
            VM->vm_id, index, mem_free, mem_total, mem_available, g_time_now_s);
        cloud_db_client_send(influx_data);
    }

    json_object_put(parsed_json);
}

static void upload_vm_bw_to_cloud_db(struct vm_instance *VM)
{
    char influx_data[INFLUXDB_DATA_LENGTH];

    // Filter unresonable bandwidth results
    // TODO: fix this issue. Currently, if we use cores from both sockets,
    // the results looks wired.
    if (MB_TO_GB(VM->mem_bw) > MAX_BW_THRESHOLD_GB) {
        return;
    }

    snprintf(influx_data, sizeof(influx_data),
             "vm_bandwidth,vm_id=%d bandwidth_local=%lu,bandwidth_remote=%lu %lu",
             VM->vm_id, VM->mem_bw_local, VM->mem_bw_remote, g_time_now_s);

    cloud_db_client_send(influx_data);
}

static void upload_vm_latency_to_cloud_db(struct vm_instance *VM)
{
    bool latency_valid = true;
    char influx_data[INFLUXDB_DATA_LENGTH];

    // We use little's law to estimate latency. However, if there is no workload,
    // the estimated latency looks not resonable. Hence, we filter unresonable latency
    // by checking whether bandwidth is in valid range.
    // TODO: find a better way to filter unresonable latency.
    if (MB_TO_GB(VM->mem_bw) > MAX_BW_THRESHOLD_GB ||
        MB_TO_GB(VM->mem_bw) < MIN_BW_THRESHOLD_GB) {
        latency_valid = false;
    }

    snprintf(influx_data, sizeof(influx_data),
        "vm_latency,vm_id=%d l3_miss_latency=%f %lu", VM->vm_id,
        latency_valid ? VM->cur_metrics[METRIC_TYPE_LOAD_L3_MISS_LAT] : 0, g_time_now_s);

    cloud_db_client_send(influx_data);
}

static void get_sys_mem_bw_usage(memdata_t *md, core_metrics_t *core_metrics)
{
    bool output = false;

    perf_agent_get_metrics(md, core_metrics, output);

#if ENABLE_DEBUG
    int max_sockets = 2;
    for (int skt_id = 0; skt_id < max_sockets; skt_id++) {
        LOG_DEBUG("Socket %d, Total: %lu GB/s, Read: %lu GB/s, Write: %lu GB/s", skt_id, MB_TO_GB(md->iMC_Rd_socket[skt_id] + md->iMC_Wr_socket[skt_id]), MB_TO_GB(md->iMC_Rd_socket[skt_id]), MB_TO_GB(md->iMC_Wr_socket[skt_id]));
    }
#endif
}

static void __accumulate_bw_per_core(struct vm_instance *VM, int core_id, void *arg)
{
    core_metrics_t *core_metrics = (core_metrics_t *)arg;

    // units in MB
    VM->mem_bw_local += core_metrics->core_local_bw[core_id];
    VM->mem_bw_remote += core_metrics->core_remote_bw[core_id];
}

static void __get_vm_mem_bw(struct vm_instance *VM, void *arg)
{
    VM->mem_bw_local = 0;
    VM->mem_bw_remote = 0;

    vm_mngr_for_each_core(VM, __accumulate_bw_per_core, arg);

    // We calculate real BW here by ourselves since current metrics from
    // PCM are not real bandwidth.
    VM->mem_bw_local = VM->mem_bw_local / monitor_interval_in_second;
    VM->mem_bw_remote = VM->mem_bw_remote / monitor_interval_in_second;
    VM->mem_bw = VM->mem_bw_local + VM->mem_bw_remote;

    if (enable_cloud_db) {
        upload_vm_bw_to_cloud_db(VM);
    }
#ifdef ENABLE_DEBUG
    LOG_DEBUG("VM %i Bandwidth %lu GB/s, Local %lu GB/s, Remote %lu GB/s", VM->vm_id, MB_TO_GB(VM->mem_bw), MB_TO_GB(VM->mem_bw_local), MB_TO_GB(VM->mem_bw_remote));
#endif
}

static void __get_vm_mem_latency(struct vm_instance *VM, void *arg __attribute__((unused)))
{
    // VM manager already updates VM metrics and print DEBUG information
    // Here, we only need to upload to cloud if needed.
    if (enable_cloud_db) {
        upload_vm_latency_to_cloud_db(VM);
    }
}

static void __get_vm_mem_cp(struct vm_instance *VM, void *arg __attribute__((unused)))
{
    int vm_id = VM->vm_id;
    char *status_json_response;

    status_json_response = guest_agent_get_meminfo(vm_id);
    if (unlikely(!status_json_response)) {
        LOG_ERROR("Failed to get guest memory info, VM %d", vm_id);
        return;
    }

    if (enable_cloud_db) {
        upload_vm_cp_to_cloud_db(VM, status_json_response);
    }

    // TODO: parse memory usage and fill VM
#ifdef ENABLE_DEBUG
    LOG_DEBUG("%s", status_json_response);
#endif
    free(status_json_response);
}

static void *__monitor_loop(void *arg __attribute__((unused)))
{
    memdata_t md;
    core_metrics_t core_metrics;
    uint32_t monitor_interval_in_us = SECOND_TO_US(monitor_interval_in_second);
    struct timespec ts;

    while (running) {
        /* Update current time and use same time for all data per iteration */
        clock_gettime(CLOCK_REALTIME, &ts);
        g_time_now_s = ts.tv_sec;

#ifdef ENABLE_DEBUG
        LOG_DEBUG("======================================================================");
#endif
        /* Get whole system bandwidth including per core and channel/controller (uncore) */
        get_sys_mem_bw_usage(&md, &core_metrics);
    
        /* Monitor memory capacity usage of each VM */
        vm_mngr_for_each_vm_running(__get_vm_mem_cp, NULL);

        /*Monitor memory bandwidth usage of each VM */
        vm_mngr_for_each_vm_running(__get_vm_mem_bw, &core_metrics);

#ifdef ENABLE_PERF
        /* Get perf event counters and calculate VM metrics based on raw counters */
        vm_mngr_update_perf_counters();
        vm_mngr_update_metrics(monitor_interval_in_second);

        /* Monitor memory latency of each VM */
        vm_mngr_for_each_vm_running(__get_vm_mem_latency, NULL);
#endif

        usleep(monitor_interval_in_us);
    }

    return NULL;
}

void guest_monitor_server_stop(void)
{
    running = 0;
    pthread_join(monitor_thread, NULL);

    perf_agent_cleanup();
    if (enable_cloud_db) {
        cloud_db_client_cleanup();
    }
}

int guest_monitor_server_start(const char* config_file)
{
    if (guest_monitor_load_config(config_file) != 0) {
        LOG_ERROR("Failed to load monitor configuration");
        return -1;
    }

    if (enable_cloud_db) {
        if (cloud_db_client_init() != 0) {
            LOG_ERROR("Failed to init cloud db client");
            return -1;
        }
    }

    if (perf_agent_init() != 0) {
        LOG_ERROR("Failed to init uncore agent");
        return -1;
    }

    if (pthread_create(&monitor_thread, NULL, __monitor_loop, NULL) != 0) {
        LOG_ERROR("Failed to create memory query thread");
        return -1;
    }

    return 0;
}

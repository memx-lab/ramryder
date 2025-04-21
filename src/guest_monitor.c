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

struct guest_monitor {
    int vm_ids[MAX_NUM_VM];
    int count;
};

static struct guest_monitor g_guest_monitor;

// for monitor
static volatile int running = 1;
static pthread_t monitor_thread;

// for cloud db
static bool enable_cloud_db = false;
static char *influxdb_url = NULL;
static char *influxdb_token = NULL;
static CURL *g_curl = NULL;
struct curl_slist *g_headers = NULL;

static int guest_monitor_load_config(const char* config_file)
{
    char line[512];
    FILE *file;

    file = fopen(config_file, "r");
    if (!file) {
        perror("Failed to open config file");
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
            }
        }
    }
#ifdef ENABLE_DEBUG
    printf("Cloud DB %s, url: %s, size: %lu, token: %s, size: %lu\n",
            enable_cloud_db? "enabled" : "disabled",
            influxdb_url, strlen(influxdb_url), influxdb_token, strlen(influxdb_token));
#endif
    fclose(file);
    return 0;
}

static int cloud_db_client_init(void)
{
    char auth_header[512];

    g_curl = curl_easy_init();
    if (g_curl == NULL) {
        perror("Failed to init curl\n");
        return -1;
    }

    g_headers = curl_slist_append(g_headers, "Content-Type: text/plain");
    snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", influxdb_token);
    g_headers = curl_slist_append(g_headers, auth_header);

    curl_easy_setopt(g_curl, CURLOPT_URL, influxdb_url);
    curl_easy_setopt(g_curl, CURLOPT_HTTPHEADER, g_headers);

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

static void _cloud_db_client_send(const char *data)
{
    CURLcode res;

    if (!g_curl) {
        return;
    }

    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, data);
    res = curl_easy_perform(g_curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "InfluxDB request failed: %s\n", curl_easy_strerror(res));
    }
}

static void upload_to_cloud_db(const char *json_str)
{
    char influx_data[256];
    struct json_object *parsed_json, *return_obj, *meminfo_list, *meminfo;

    parsed_json = json_tokener_parse(json_str);
    if (!json_object_object_get_ex(parsed_json, "return", &return_obj)) {
        perror("Filed to parse json response: no return found\n");
        return;
    }

    if (!json_object_object_get_ex(return_obj, "meminfo-list", &meminfo_list)) {
        perror("Failed to parse json response: no meminfo-list found\n");
        return;
    }

    for (size_t i = 0; i < json_object_array_length(meminfo_list); i++) {
        meminfo = json_object_array_get_idx(meminfo_list, i);
        int index = json_object_get_int(json_object_object_get(meminfo, "index"));
        int mem_free = json_object_get_int(json_object_object_get(meminfo, "mem-free"));
        int mem_total = json_object_get_int(json_object_object_get(meminfo, "mem-total"));
        int mem_available = json_object_get_int(json_object_object_get(meminfo, "mem-available"));

        snprintf(influx_data, sizeof(influx_data),
                 "guest_memory,vm_id=1,node=%d memory_free=%d,memory_total=%d,memory_available=%d",
                 index, mem_free, mem_total, mem_available);
        _cloud_db_client_send(influx_data);
    }

    json_object_put(parsed_json);
}

static void _monitor_memory_usage(int vm_id)
{
    char *status_json_response;

    status_json_response = guest_agent_get_meminfo(vm_id);
    if (likely(status_json_response)) {
        if (enable_cloud_db) {
            upload_to_cloud_db(status_json_response);
        }
#ifdef ENABLE_DEBUG
        printf("%s\n", status_json_response);
#endif
        free(status_json_response);
    } else {
        printf("Failed to get guest memory info.\n");
    }

}

static void _monitor_bw_usage()
{
    memdata_t md;
    bool output = false;

    perf_uncore_agent_get_bandwidth(&md, output);
#ifdef ENABLE_DEBUG
    printf("  Total Read : %.2f MB/s\n", md.iMC_Rd_socket[0]);
    printf("  Total Write: %.2f MB/s\n", md.iMC_Wr_socket[0]);
#endif
}

static void *_monitor_loop(void *arg __attribute__((unused)))
{
    while (running) {
        /* Monitor memory capacity usage of each VM */
        for (int i = 0; i < g_guest_monitor.count; i++) {
            _monitor_memory_usage(g_guest_monitor.vm_ids[i]);
        }

        /* Monitor memory bandwidth usage */
        // TODO: for each VM
        _monitor_bw_usage();

        usleep(10 * SECOND_IN_US);
    }

    return NULL;
}

int guest_monitor_remove_vm(int vm_id)
{
    bool found = false;

    for (int i = 0; i < g_guest_monitor.count; i++) {
        if (g_guest_monitor.vm_ids[i] == vm_id) {
            found = true;
        }
    }

    if (!found) {
        fprintf(stderr, "Cannot find VM %d\n", vm_id);
        return -1;
    }

    guest_agent_cleanup(vm_id);
    return 0; 
}

int guest_monitor_add_vm(int vm_id)
{
    int ret;

    if (g_guest_monitor.count >= MAX_NUM_VM) {
        fprintf(stderr, "Cannot create more agents (maximum is %d)\n", MAX_NUM_VM);
        return -1;
    }

    for (int i = 0; i < g_guest_monitor.count; i++) {
        if (g_guest_monitor.vm_ids[i] == vm_id) {
            fprintf(stderr, "VM %d already exits\n", vm_id);
            return -1;
        }
    }

    ret = guest_agent_init(vm_id);
    if (ret < 0) {
        fprintf(stderr, "Failed to init guest agent for vm %d\n", vm_id);
        return -1;
    }
    g_guest_monitor.vm_ids[g_guest_monitor.count] = vm_id;
    g_guest_monitor.count++;

    return 0;
}

void guest_monitor_server_stop(void)
{
    running = 0;
    pthread_join(monitor_thread, NULL);

    perf_uncore_agent_cleanup();
    if (enable_cloud_db) {
        cloud_db_client_cleanup();
    }

    /* Remove all VMs if there are still active VMs in case users did not remove them */
    for (int i = 0; i < g_guest_monitor.count; i++) {
        guest_monitor_remove_vm(g_guest_monitor.vm_ids[i]);
    }
}

int guest_monitor_server_start(const char* config_file)
{
    if (guest_monitor_load_config(config_file) != 0) {
        fprintf(stderr, "Failed to load monitor configuration\n");
        return -1;
    }

    if (enable_cloud_db) {
        if (cloud_db_client_init() != 0) {
            fprintf(stderr, "Failed to init cloud db client\n");
            return -1;
        }
    }

    if (perf_uncore_agent_init() != 0) {
        fprintf(stderr, "Failed to init uncore agent\n");
        return -1;
    }

    if (pthread_create(&monitor_thread, NULL, _monitor_loop, NULL) != 0) {
        fprintf(stderr, "Failed to create memory query thread");
        return -1;
    }

    return 0;
}
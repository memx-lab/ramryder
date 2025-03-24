#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "guest_agent.h"
#include "guest_monitor.h"

// for monitor
#define SECOND_IN_US 1000000
static volatile int running = 1;
static pthread_t query_thread;

// for cloud db
#define INFLUXDB_URL ""
#define INFLUXDB_TOKEN ""
static bool enable_cloud_db = true;
static CURL *g_curl = NULL;
struct curl_slist *g_headers = NULL;

static int cloud_db_client_init(void)
{
    g_curl = curl_easy_init();
    if (g_curl == NULL) {
        perror("Failed to init curl\n");
        return -1;
    }

    g_headers = curl_slist_append(g_headers, "Content-Type: text/plain");
    g_headers = curl_slist_append(g_headers, "Authorization: Token " INFLUXDB_TOKEN);

    curl_easy_setopt(g_curl, CURLOPT_URL, INFLUXDB_URL);
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
        fprintf(stderr, "InfluxDB request failed: %s\n", curl_easy_strerror(res));
    } else {
        printf("Data sent to InfluxDB Cloud: %s\n", data);
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
        cloud_db_client_send(influx_data);
    }

    json_object_put(parsed_json);
}

static void *query_memory_loop(void *arg __attribute__((unused)))
{
    while (running) {
        char *json_response = query_vm_status();
        if (json_response) {
            if (enable_cloud_db) {
                upload_to_cloud_db(json_response);
            } else {
                printf("%s\n", json_response);
            }
            free(json_response);
        } else {
            printf("Failed to get guest memory info.\n");
        }
        usleep(10 * SECOND_IN_US);
    }
    return NULL;
}

void stop_guest_monitor(void)
{
    running = 0;
    pthread_join(query_thread, NULL);
    if (enable_cloud_db) {
        cloud_db_client_cleanup();
    }
}

int start_guest_monitor(void)
{
    if (enable_cloud_db) {
        if (cloud_db_client_init() != 0) {
            perror("Failed to init cloud db client\n");
            return -1;
        }
    }

    if (pthread_create(&query_thread, NULL, query_memory_loop, NULL) != 0) {
        perror("Failed to create memory query thread");
        return -1;
    }

    return 0;
}
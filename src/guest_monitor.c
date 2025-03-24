#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "guest_agent.h"
#include "guest_monitor.h"

#define US_SECOND 5000000

static volatile int running = 1;
static pthread_t query_thread;

static void *query_memory_loop(void *arg __attribute__((unused)))
{
    while (running) {
        char *response = query_vm_status();
        if (response) {
            printf("%s\n", response);
            free(response);
        } else {
            printf("Failed to get guest memory info.\n");
        }
        printf("\n");
        usleep(US_SECOND);
    }
    return NULL;
}

void stop_guest_monitor(void)
{
    running = 0;
}

int start_guest_monitor(void)
{
    if (pthread_create(&query_thread, NULL, query_memory_loop, NULL) != 0) {
        perror("Failed to create memory query thread");
        return -1;
    }

    return 0;
}
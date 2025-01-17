
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define BLOCK_SIZE (64)
#define INTER_LEAVE_SIZE (64)
#define DEFAULT_RUNTIME 120
#define REPORT_INTERVAL 1

volatile int running = 1;

void handle_signal(int sig) {
    running = 0;
}

double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void bandwidth_test(void *addr, size_t size, int is_write, int runtime) {
    char *buffer;
    size_t offset = 0, total_bytes = 0;
    double start_time, last_report_time, current_time;
    long iterations = size / BLOCK_SIZE;

    buffer = malloc(BLOCK_SIZE);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    memset(buffer, (is_write ? 'W' : 'R'), BLOCK_SIZE);

    start_time = last_report_time = get_time_sec();

    while (running && (get_time_sec() - start_time) < runtime) {
        for (size_t i = 0; i < iterations && running; i++) {
            if (is_write) {
                memcpy((char *)addr + offset, buffer, BLOCK_SIZE);
            } else {
                memcpy(buffer, (char *)addr + offset, BLOCK_SIZE);
            }
            offset = (offset + INTER_LEAVE_SIZE) % size;
            total_bytes += BLOCK_SIZE;

            current_time = get_time_sec();
            if (current_time - last_report_time >= REPORT_INTERVAL) {
                double elapsed = current_time - start_time;
                double bw = total_bytes / (1024.0 * 1024.0) / elapsed;
                printf("[%6.2f s] %s Bandwidth: %.2f MB/s\n",
                       elapsed, is_write ? "Write" : "Read", bw);
                last_report_time = current_time;
            }
        }
    }

    double elapsed = get_time_sec() - start_time;
    double avg_bw = total_bytes / (1024.0 * 1024.0) / elapsed;
    printf("Final %s Bandwidth: %.2f MB/s (Total: %.2f GB in %.2f s)\n",
           is_write ? "Write" : "Read", avg_bw,
           total_bytes / (1024.0 * 1024.0 * 1024.0), elapsed);

    free(buffer);
}

void access_bindwidth(void *addr, size_t size, int is_write, int runtime)
{
    volatile uint64_t temp = 0;
    uint64_t *buff = (uint64_t *)addr;
    size_t total_accessd = 0;
    double start_time, last_report_time, current_time;
    uint64_t i = 0;
    srand(time(NULL));

    start_time = last_report_time = get_time_sec();
    while (running && (get_time_sec() - start_time) < runtime) {
        int offset = i % 8;
        //buff[offset] = rand() % 256;
        temp = buff[offset];
        total_accessd += 64;
        i++;
    }
    printf("Total accssed: %ld\n", total_accessd);
}

int main(int argc, char *argv[]) {
    const char *device = "/dev/dax0.0";
    int fd, is_write = 1, runtime = DEFAULT_RUNTIME;
    size_t size = 20 * 1024L * 1024 * 2014;
    size_t device_size = 50667192320;
    void *addr;

    signal(SIGINT, handle_signal);

    if (argc > 1) {
        runtime = atoi(argv[1]);
    }
    if (argc > 2) {
        is_write = (strcmp(argv[2], "write") == 0) ? 1 : 0;
    }

    fd = open(device, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    addr = mmap(NULL, device_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Mapped %ld MB of memory from %s\n", device_size / (1024 * 1024), device);
    printf("Running %s test for %d seconds...\n", is_write ? "write" : "read", runtime);

    //access_bindwidth(addr, 1024 * 1024 * 1024, is_write, runtime);
    bandwidth_test(addr, 1024 * 1024 * 1024, is_write, runtime);

    munmap(addr, device_size);
    close(fd);

    return 0;
}


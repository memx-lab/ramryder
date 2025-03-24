#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <signal.h>
#include "memory_resource.h"
#include "util_socket.h"
#include "qemu_agent.h"
#include "guest_agent.h"
#include "guest_monitor.h"

#define SERVER_SOCKET "/var/run/resource_manager.sock"
#define MAX_EVENTS 10

static int g_server_fd = -1;
static int g_epoll_fd = -1;

static void stop_rpc_server(void)
{
    if (g_server_fd != -1) {
        close(g_server_fd);
        unlink(SERVER_SOCKET);
    }
    if (g_epoll_fd != -1) {
        close(g_epoll_fd);
    }
}

static void start_rpc_server(void)
{
    int num_events = 0;
    char *response = NULL;
    char buffer[BUFFER_SIZE] = {0};
    struct sockaddr_un server_addr;
    struct epoll_event event, events[MAX_EVENTS];

    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("Server socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SERVER_SOCKET, sizeof(server_addr.sun_path) - 1);
    unlink(SERVER_SOCKET);

    if (bind(g_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Server bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(g_server_fd, 5) < 0) {
        perror("Server listen failed");
        exit(EXIT_FAILURE);
    }

    g_epoll_fd = epoll_create1(0);
    if (g_epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = g_server_fd;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_server_fd, &event) == -1) {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    printf("Resource Manager Server started. Waiting for client requests...\n");

    while (1) {
        num_events = epoll_wait(g_epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == g_server_fd) {
                struct sockaddr_un client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("Client accept failed");
                    continue;
                }

                recv(client_fd, buffer, BUFFER_SIZE, 0);
                printf("Received command: %s\n", buffer);

                if (strcmp(buffer, "query") == 0) {
                    response = query_vm_status();
                } else if (strcmp(buffer, "hotplug") == 0) {
                    response = hotplug_dimm();
                } else if (strcmp(buffer, "memory") == 0) {
                    response = malloc(BUFFER_SIZE);
                    get_memory_resource(response, BUFFER_SIZE);
                } else {
                    response = strdup("Unknown command");
                }

                send(client_fd, response, strlen(response), 0);
                free(response);
                close(client_fd);
            }
        }
    }
}

static void handle_signal(int signum __attribute__((unused)))
{
    printf("\nResource Manager shutting down...\n");

    stop_rpc_server();
    guest_agent_cleanup();
    stop_guest_monitor();
    exit(0);
}

int main()
{
    signal(SIGINT, handle_signal);

    if (memory_manager_init() != 0) {
        exit(EXIT_FAILURE);
    }

    /* Only init guest agent as it uses long-lived connection */
    if (guest_agent_init() != 0) {
        exit(EXIT_FAILURE);
    }

    if (start_guest_monitor() != 0) {
        exit(EXIT_FAILURE);
    }

    start_rpc_server();

    return 0;
}

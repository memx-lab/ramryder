#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <signal.h>
#include <assert.h>
#include <json-c/json.h>
#include "memory_resource.h"

#define QGA_SOCKET "/var/run/qga-sock-1"
#define QMP_SOCKET "/var/run/qmp-sock-1"
#define SERVER_SOCKET "/var/run/resource_manager.sock"
#define BUFFER_SIZE 1024
#define MAX_EVENTS 10

static int g_guest_agent_fd = -1;
static int g_server_fd = -1;
static int g_epoll_fd = -1;

static void handle_signal(int signum __attribute__((unused)))
{
    if (g_guest_agent_fd != -1) {
        close(g_guest_agent_fd);
    }
    if (g_server_fd != -1) {
        close(g_server_fd);
        unlink(SERVER_SOCKET);
    }
    if (g_epoll_fd != -1) {
        close(g_epoll_fd);
    }
    printf("\nResource Manager Server shutting down...\n");
    exit(0);
}

static int connect_to_socket(const char *socket_path)
{
    int sockfd;
    struct sockaddr_un addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Socket connection failed");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

static char *send_command_qemu(const char *command)
{
    int sockfd = connect_to_socket(QMP_SOCKET);
    if (sockfd < 0) return NULL;

    send(sockfd, command, strlen(command), 0);
    char *response = malloc(BUFFER_SIZE);
    recv(sockfd, response, BUFFER_SIZE, 0);
    close(sockfd);
    return response;
}

/*
 * Set up long-live connection for guest agent.
 */
static char *send_command_guest(const char *command)
{
    if (g_guest_agent_fd < 0) {
        g_guest_agent_fd = connect_to_socket(QGA_SOCKET);
    }

    assert(g_guest_agent_fd >= 0);

    send(g_guest_agent_fd, command, strlen(command), 0);
    char *response = malloc(BUFFER_SIZE);
    recv(g_guest_agent_fd, response, BUFFER_SIZE, 0);
    return response;
}

static char *query_vm_status(void)
{
    json_object *cmd = json_object_new_object();
    json_object_object_add(cmd, "execute", json_object_new_string("guest-get-meminfo"));
    const char *command = json_object_to_json_string(cmd);
    char *response = send_command_guest(command);
    json_object_put(cmd);
    return response;
}

static char *hotplug_dimm(int size_mb __attribute__((unused)))
{
    json_object *cmd = json_object_new_object();
    json_object *args = json_object_new_object();
    
    json_object_object_add(args, "driver", json_object_new_string("pc-dimm"));
    json_object_object_add(args, "id", json_object_new_string("dimm1"));
    json_object_object_add(args, "memdev", json_object_new_string("mem1"));
    
    json_object_object_add(cmd, "execute", json_object_new_string("device_add"));
    json_object_object_add(cmd, "arguments", args);
    
    const char *command = json_object_to_json_string(cmd);
    char *response = send_command_qemu(command);
    json_object_put(cmd);
    return response;
}

static void start_server(void)
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
                    response = hotplug_dimm(512);
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

int main() {
    signal(SIGINT, handle_signal);

    memory_manager_init();

    start_server();

    return 0;
}

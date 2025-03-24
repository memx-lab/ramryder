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
#include "guest_agent.h"
#include "util_socket.h"

#define QGA_SOCKET "/var/run/qga-sock-1"
static int g_guest_agent_fd = -1;

/*
 * Use long-live connection for guest agent.
 */
int guest_agent_init(void)
{
    if (g_guest_agent_fd < 0) {
        g_guest_agent_fd = connect_to_socket(QGA_SOCKET);
        if (g_guest_agent_fd < 0) {
            printf("Failed to connect to guest agent, socket: %d\n", g_guest_agent_fd);
            return -1;
        }
    }

    return 0;
}

void guest_agent_cleanup(void)
{
    if (g_guest_agent_fd != -1) {
        close(g_guest_agent_fd);
    }
}

static char *send_command_guest(const char *command)
{
    assert(g_guest_agent_fd >= 0);

    send(g_guest_agent_fd, command, strlen(command), 0);
    char *response = malloc(BUFFER_SIZE);
    recv(g_guest_agent_fd, response, BUFFER_SIZE, 0);
    return response;
}

char *query_vm_status(void)
{
    json_object *cmd = json_object_new_object();
    json_object_object_add(cmd, "execute", json_object_new_string("guest-get-meminfo"));
    const char *command = json_object_to_json_string(cmd);
    char *response = send_command_guest(command);
    json_object_put(cmd);
    return response;
}
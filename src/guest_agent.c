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
#include <stdbool.h>
#include <json-c/json.h>
#include "guest_agent.h"
#include "util_socket.h"

#define QGA_SOCKET_PREFIX "/var/run/qga-sock-"
#define MAX_SOCKET_PATH 256

struct guest_agent {
    char socket_path[MAX_SOCKET_PATH];
    int agent_fd;
    bool initialized;
};

struct guest_agent_manager {
    struct guest_agent guest_agents[MAX_NUM_VM];
    int count;
};

static struct guest_agent_manager g_agent_manager;

/*
 * Use long-live connection for guest agent.
 */
int guest_agent_init(int vm_id)
{
    int agent_fd = -1;

    if (g_agent_manager.guest_agents[vm_id].initialized) {
        return 0;
    }

    if (g_agent_manager.count >= MAX_NUM_VM) {
        fprintf(stderr, "Cannot create more agents (maximum is %d)\n", MAX_NUM_VM);
        return -1;
    }

    snprintf(g_agent_manager.guest_agents[vm_id].socket_path, 
            MAX_SOCKET_PATH, "%s%d", QGA_SOCKET_PREFIX, vm_id);
    agent_fd = connect_to_socket(g_agent_manager.guest_agents[vm_id].socket_path);
    if (agent_fd < 0) {
        fprintf(stderr, "Failed to connect to guest agent, socket: %s\n", 
                g_agent_manager.guest_agents[vm_id].socket_path);
        return -1;
    }

    g_agent_manager.guest_agents[vm_id].agent_fd = agent_fd;
    g_agent_manager.guest_agents[vm_id].initialized = true;
    g_agent_manager.count++;

#ifdef ENABLE_DEBUG
    printf("Create agent for vm id: %d, sockect path: %s, fd: %d\n",
            vm_id, g_agent_manager.guest_agents[vm_id].socket_path,
            g_agent_manager.guest_agents[vm_id].agent_fd);
#endif

    return 0;
}

void guest_agent_cleanup(int vm_id)
{
    if (g_agent_manager.guest_agents[vm_id].initialized) {
        // TODO: handle return
        close_sockect(g_agent_manager.guest_agents[vm_id].agent_fd);
        g_agent_manager.guest_agents[vm_id].initialized = false;
        g_agent_manager.count--;
    }
}

static char *send_guest_cmd(int vm_id, const char *command)
{
    char *response;

    if (!g_agent_manager.guest_agents[vm_id].initialized) {
        fprintf(stderr, "VM agent did not init\n");
        return NULL;
    }

    // TODO: check return
    send(g_agent_manager.guest_agents[vm_id].agent_fd, command, strlen(command), 0);
    response = malloc(BUFFER_SIZE);
    recv(g_agent_manager.guest_agents[vm_id].agent_fd, response, BUFFER_SIZE, 0);

    return response;
}

char *guest_agent_get_meminfo(int vm_id)
{
    json_object *cmd_obj;
    const char *command;
    char *response;

    // TODO: check return
    cmd_obj = json_object_new_object();
    json_object_object_add(cmd_obj, "execute", json_object_new_string("guest-get-meminfo"));
    command = json_object_to_json_string(cmd_obj);
    response = send_guest_cmd(vm_id, command);
    json_object_put(cmd_obj);

    return response;
}
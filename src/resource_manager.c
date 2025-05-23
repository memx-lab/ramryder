#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <signal.h>
#include "memory_pool.h"
#include "util_socket.h"
#include "qemu_agent.h"
#include "guest_agent.h"
#include "guest_monitor.h"
#include "vm_manager.h"
#include "util_common.h"

#define CONFIG_FILE "elasticmm.conf"
#define SERVER_SOCKET "/var/run/resource_manager.sock"
#define MAX_EVENTS 10

static int g_server_fd = -1;
static int g_epoll_fd = -1;

static void rpc_server_stop(void)
{
    if (g_server_fd != -1) {
        close(g_server_fd);
        unlink(SERVER_SOCKET);
    }
    if (g_epoll_fd != -1) {
        close(g_epoll_fd);
    }
}

static char *rpc_handle_get_mem_info(char *args)
{
    int vm_id;
    char *response = NULL;

    if (sscanf(args, "vid=%d", &vm_id) == 1) {
        response = guest_agent_get_meminfo(vm_id);
    } else {
        response = strdup("Invalid args");
    }

    return response;
}

static char *rpc_handle_get_mem_pool(void)
{
    char *response = NULL;

    response = malloc(BUFFER_SIZE);
    memory_pool_get_usage(response, BUFFER_SIZE);

    return response;
}

static char *rpc_handle_get_num_nodes(void)
{
    char *response = NULL;

    response = malloc(BUFFER_SIZE);
    // Number of nodes is same as number of devices since
    // we constrct a node for each memory device
    snprintf(response, BUFFER_SIZE, "%d", memory_pool_get_num_devs());

    return response;
}

static char *rpc_handle_get_node_info(char *args)
{
    int ret;
    int node_id;
    char *response = NULL;
    struct memory_node_info node_info;

    if (sscanf(args, "nid=%d", &node_id) == 1) {
        response = malloc(BUFFER_SIZE);
        ret = memory_pool_get_node_info(node_id, &node_info);
        if (ret == 0) {
            snprintf(response, BUFFER_SIZE, "nodeid=%d,tier-id=%d,dax-id=%d",
                node_id, node_info.tier_id, node_info.dax_id);
        } else {
            snprintf(response, BUFFER_SIZE, "Node %d does not exist", node_id);
        }
    } else {
        response = strdup("Invalid args");
    }

    return response;
}

static char *rpc_handle_alloc_mem(char *args)
{
    int ret;
    char *response = NULL;
    int tid = -1, did = -1, vid = -1, size_mb = -1;
    struct vm_mem_req vm_mem_req;

    ret = sscanf(args, "tid=%d did=%d vid=%d size=%d", &tid, &did, &vid, &size_mb);
    if (ret != 4 || tid < 0 || did < 0|| vid < 0 || size_mb <= 0) {
        response = strdup("Invalid args");
        return response;
    }

    response = malloc(BUFFER_SIZE);
    ret = vm_mngr_instance_alloc_mem(tid, did, vid, size_mb, &vm_mem_req);
    if (ret == 0) {
        // DON't change format here which is used by QEMU
        snprintf(response, BUFFER_SIZE, "id=mem%d,mem-path=%s,size=%dM,align=%dM,offset=%dM",
            vm_mem_req.memdev_idx, vm_mem_req.dev_path, vm_mem_req.size_mb,
            vm_mem_req.align_mb, vm_mem_req.offset_mb);
    } else {
        snprintf(response, BUFFER_SIZE, "Allocate failed");
    }

    return response;
}

static char *rpc_handle_free_mem(char *args)
{
    int ret;
    char *response = NULL;
    int vid = -1, memid = -1;

    ret = sscanf(args, "vid=%d memid=%d", &vid, &memid);
    if (ret != 2 || vid < 0 || memid < 0) {
        response = strdup("Invalid args");
        return response;
    }

    response = malloc(BUFFER_SIZE);
    ret = vm_mngr_instance_free_mem(vid, memid);
    if (ret == 0) {
        snprintf(response, BUFFER_SIZE, "Release success");
    } else {
        snprintf(response, BUFFER_SIZE, "Release failed");
    }

    return response;
}

static char *rpc_handle_attach_mem(char *args)
{
    int ret;
    char *response = NULL;
    int memid = -1, vid = -1, nid = -1;
    struct memory_request *mem_req;
    struct hotplug_request hotplug_req;

    ret = sscanf(args, "memid=%d vid=%d nid=%d", &memid, &vid, &nid);
    if (ret != 3 || memid < 0 || vid < 0) {
        response = strdup("Invalid args");
        return response;
    }

    mem_req = vm_mngr_instance_get_mem(vid, memid);
    if (mem_req == NULL) {
         snprintf(response, BUFFER_SIZE, "Find not find memory %d\n", memid);
         return response;
    }

    snprintf(hotplug_req.memdev_id, STRING_ID_LEN, "mem%d", memid);
    // use memid to construct dimm id as memid is an unique number
    snprintf(hotplug_req.dimm_id, STRING_ID_LEN, "dimm%d", memid);
    snprintf(hotplug_req.dev_path, DEV_PATH_LEN, "%s", mem_req->dev_path);
    hotplug_req.size_bytes = MB_TO_BYTES(mem_req->size_mb);
    hotplug_req.align_bytes = MB_TO_BYTES(mem_req->align_mb);
    hotplug_req.share = true; // always true currently
    hotplug_req.numa_node = nid < 0 ? 0 : nid; // TODO: find optimal node

    response = malloc(BUFFER_SIZE);
    ret = qemu_agent_hotplug_memory(vid, &hotplug_req);
    if (ret == 0) {
        snprintf(response, BUFFER_SIZE, "Hotplug memory success");
    } else {
        snprintf(response, BUFFER_SIZE, "Hotplug memory failed");
    }

    return response;
}

static char *rpc_handle_create_vm(char *args)
{
    int ret;
    int vid = -1;
    char *response = NULL;
    char coreset_str[BUFFER_SIZE] = {0};

    ret = sscanf(args, "vid=%d coreset=%[^\n]", &vid, coreset_str);
    if (ret != 2 || vid < 0) {
        response = strdup("Invalid args");
        return response;
    }

    char *start = strchr(coreset_str, '[');
    char *end = strchr(coreset_str, ']');
    if (!start || !end || start >= end) {
        response = strdup("Invalid command");
        return response;
    }
    start++;
    *end = '\0';

    response = malloc(BUFFER_SIZE);
    ret = vm_mngr_instance_create(vid, start);
    if (ret == 0) {
        snprintf(response, BUFFER_SIZE, "Create VM %d success, coreset: [%s]", vid, start);
    } else {
        snprintf(response, BUFFER_SIZE, "Create VM %d failed, coreset: [%s]", vid, start);
    }

    return response;
}

static char *rpc_handle_destroy_vm(char *args)
{
    int ret;
    int vid;
    char *response = NULL;

    ret = sscanf(args, "vid=%d", &vid);
    if (ret != 1 || vid < 0) {
        response = strdup("Invalid args");
        return response;
    }
    response = malloc(BUFFER_SIZE);
    ret = vm_mngr_instance_destroy(vid);
    if (ret == 0) {
        snprintf(response, BUFFER_SIZE, "Destroy VM %d success", vid);
    } else {
        snprintf(response, BUFFER_SIZE, "Destroy VM %d failed", vid);
    }

    return response;
}

static char *rpc_handle_start_vm(char *args)
{
    int ret;
    int vid;
    char *response = NULL;

    ret = sscanf(args, "vid=%d", &vid);
    if (ret != 1 || vid < 0) {
        response = strdup("Invalid args");
        return response;
    }
    response = malloc(BUFFER_SIZE);
    ret = vm_mngr_instance_start(vid);
    if (ret == 0) {
        snprintf(response, BUFFER_SIZE, "Start VM %d success", vid);
    } else {
        snprintf(response, BUFFER_SIZE, "Start VM %d failed", vid);
    }

    return response;
}

static char *rpc_handle_stop_vm(char *args)
{
    int ret;
    int vid;
    char *response = NULL;

    ret = sscanf(args, "vid=%d", &vid);
    if (ret != 1 || vid < 0) {
        response = strdup("Invalid args");
        return response;
    }
    response = malloc(BUFFER_SIZE);
    ret = vm_mngr_instance_stop(vid);
    if (ret == 0) {
        snprintf(response, BUFFER_SIZE, "Stop VM %d success", vid);
    } else {
        snprintf(response, BUFFER_SIZE, "Stop VM %d failed", vid);
    }

    return response;
}

static void rpc_server_start(void)
{
    int num_events = 0;
    char *response = NULL;
    char buffer[BUFFER_SIZE] = {0};
    char cmd[64];
    char args[BUFFER_SIZE];
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

    printf("Resource Manager RPC Server started. Waiting for client requests...\n");

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

                memset(buffer, 0, BUFFER_SIZE);
                recv(client_fd, buffer, BUFFER_SIZE, 0);
                printf("Received command: %s\n", buffer);
                if (sscanf(buffer, "%63s %[^\n]", cmd, args) < 1) {
                    response = strdup("Invalid command");
                    goto end;
                }

                if (strcmp(cmd, "get-mem-info") == 0) {
                    response = rpc_handle_get_mem_info(args);
                } else if (strcmp(cmd, "get-mem-pool") == 0) {
                    response = rpc_handle_get_mem_pool();
                } else if (strcmp(cmd, "get-num-nodes") == 0) {
                    response = rpc_handle_get_num_nodes();
                } else if (strcmp(cmd, "get-node-info") == 0) {
                    response = rpc_handle_get_node_info(args);
                } else if (strcmp(cmd, "alloc-mem") == 0) {
                    response = rpc_handle_alloc_mem(args);
                } else if (strcmp(cmd, "free-mem") == 0) {
                    response = rpc_handle_free_mem(args);
                } else if (strcmp(cmd, "attach-mem") == 0) {
                    response = rpc_handle_attach_mem(args);
                } else if (strcmp(cmd, "create-vm") == 0) {
                    response = rpc_handle_create_vm(args);
                } else if (strcmp(cmd, "destroy-vm") == 0) {
                    response = rpc_handle_destroy_vm(args);
                } else if (strcmp(cmd, "start-vm") == 0) {
                    response = rpc_handle_start_vm(args);
                } else if (strcmp(cmd, "stop-vm") == 0) {
                    response = rpc_handle_stop_vm(args);
                } else {
                    response = strdup("Invalid command");
                }

                if (response == NULL) {
                    response = strdup("Failed to get results\n");
                }
end:
                send(client_fd, response, strlen(response), 0);
                if (response) {
                    free(response);
                }
                close(client_fd);
            }
        }
    }
}

static void __vm_destroy(struct vm_instance *VM, void *arg __attribute__((unused)))
{
    vm_mngr_instance_destroy(VM->vm_id);
}

static void __vm_stop(struct vm_instance *VM, void *arg __attribute__((unused)))
{
    vm_mngr_instance_stop(VM->vm_id);
}

static void handle_signal(int signum __attribute__((unused)))
{
    printf("\nResource Manager shutting down...\n");

    rpc_server_stop();
    guest_monitor_server_stop();
    // stop all running VMs before desroying
    vm_mngr_for_each_vm_running(__vm_stop, NULL);
    // destroy all created VMs
    vm_mngr_for_each_vm(__vm_destroy, NULL);
    exit(0);
}

int main()
{
    signal(SIGINT, handle_signal);

    if (memory_pool_init(CONFIG_FILE) != 0) {
        exit(EXIT_FAILURE);
    }

    if (guest_monitor_server_start(CONFIG_FILE) != 0) {
        exit(EXIT_FAILURE);
    }

    rpc_server_start();

    return 0;
}

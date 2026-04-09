#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <json-c/json.h>
#include "util_socket.h"

#define SERVER_SOCKET "/var/run/resource_manager.sock"

static void print_results(char* results)
{
    json_object *parsed_json;

    parsed_json = json_tokener_parse(results);
    if (parsed_json) {
        printf("%s\n", json_object_to_json_string_ext(parsed_json, JSON_C_TO_STRING_PRETTY));
        json_object_put(parsed_json);
    } else {
        printf("%s\n", results);
    }
}

static void send_command(const char *command)
{
    int sockfd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SERVER_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Socket connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    send(sockfd, command, strlen(command), 0);
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    print_results(buffer);
    
    close(sockfd);
}

static void print_usage(const char *progname)
{
    fprintf(stderr, "Resource Manager Client\n");
    fprintf(stderr, "Send management commands to the resource manager over %s.\n\n",
            SERVER_SOCKET);

    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <command> [arguments]\n\n", progname);

    fprintf(stderr, "Query Commands:\n");
    fprintf(stderr, "  %-60s %s\n", "get-mem-info vid=<vm_id>",
            "Show memory information for a VM.");
    fprintf(stderr, "  %-60s %s\n", "get-mem-pool",
            "Show the current memory pool state.");
    fprintf(stderr, "  %-60s %s\n", "get-num-nodes",
            "Show the number of available NUMA nodes.");
    fprintf(stderr, "  %-60s %s\n\n", "get-node-info nid=<node_id>",
            "Show details for a C-NUMA node.");

    fprintf(stderr, "Memory Management Commands:\n");
    fprintf(stderr, "  %-60s %s\n", "alloc-mem tid=<tid> did=<dev_id> vid=<vm_id> size=<mb>",
            "Allocate memory for a VM.");
    fprintf(stderr, "  %-60s %s\n", "free-mem vid=<vm_id> memid=<memdev_id>",
            "Release an allocated memory device.");
    fprintf(stderr, "  %-60s %s\n", "attach-mem memid=<memory_id> vid=<vm_id> nid=<node_id>",
            "Attach memory to a VM on a NUMA node.");
    fprintf(stderr, "  %-60s %s\n\n", "detach-mem memid=<memory_id> vid=<vm_id>",
            "Detach memory from a VM.");

    fprintf(stderr, "VM Lifecycle Commands:\n");
    fprintf(stderr, "  %-60s %s\n", "create-vm vid=<vm_id> coreset=[start-end,...]",
            "Register a VM and its CPU core set.");
    fprintf(stderr, "  %-60s %s\n", "destroy-vm vid=<vm_id>",
            "Remove a VM from the manager.");
    fprintf(stderr, "  %-60s %s\n", "start-vm vid=<vm_id>",
            "Mark a VM as started.");
    fprintf(stderr, "  %-60s %s\n\n", "stop-vm vid=<vm_id>",
            "Mark a VM as stopped.");

    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s get-mem-info vid=3\n", progname);
    fprintf(stderr, "  %s alloc-mem tid=1 did=0 vid=3 size=1024\n", progname);
    fprintf(stderr, "  %s create-vm vid=3 coreset=[20-30,50-60]\n", progname);
}

int main(int argc, char *argv[])
{
    const char *cmd_action;
    char cmd_full[BUFFER_SIZE] = {0};

    if (argc < 2) {
        print_usage(argv[0]);
        return -1;
    }

    cmd_action = argv[1];

    if (strcmp(cmd_action, "get-mem-info") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s", cmd_action, argv[2]);
    } else if (strcmp(cmd_action, "get-mem-pool") == 0) {
        if (argc != 2) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s", cmd_action);
    } else if (strcmp(cmd_action, "get-num-nodes") == 0) {
        if (argc != 2) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s", cmd_action);
    } else if (strcmp(cmd_action, "get-node-info") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s", cmd_action, argv[2]);
    } else if (strcmp(cmd_action, "alloc-mem") == 0) {
        if (argc != 6) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s %s %s %s",
                    cmd_action, argv[2], argv[3], argv[4], argv[5]);
    } else if (strcmp(cmd_action, "free-mem") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s %s",
                    cmd_action, argv[2], argv[3]);
    } else if (strcmp(cmd_action, "attach-mem") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        // e.g., attach-mem mid=<memory id> vid=<VM id> nid=<NUMA node id>
        snprintf(cmd_full, sizeof(cmd_full), "%s %s %s %s",
                    cmd_action, argv[2], argv[3], argv[4]);
    } else if (strcmp(cmd_action, "detach-mem") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        // e.g., detach-mem memid=<memory id> vid=<VM id>
        snprintf(cmd_full, sizeof(cmd_full), "%s %s %s",
                    cmd_action, argv[2], argv[3]);
    } else if (strcmp(cmd_action, "create-vm") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s %s", cmd_action, argv[2], argv[3]);
    } else if (strcmp(cmd_action, "destroy-vm") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s", cmd_action, argv[2]);
    } else if (strcmp(cmd_action, "start-vm") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s", cmd_action, argv[2]);
    } else if (strcmp(cmd_action, "stop-vm") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Invalid usage\n");
            print_usage(argv[0]);
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "%s %s", cmd_action, argv[2]);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd_action);
        print_usage(argv[0]);
        return -1;
    }

    send_command(cmd_full);
    return 0;
}

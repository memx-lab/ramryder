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

    printf("Server Response:\n");
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

static void print_usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "   get-mem-info <vm_id>\n");
    fprintf(stderr, "   add-mem size=<mb>\n");
    fprintf(stderr, "   get-mem-bw\n");
}

int main(int argc, char *argv[])
{
    const char *cmd_action;
    char cmd_full[BUFFER_SIZE] = {0};

    if (argc < 2) {
        print_usage();
        return -1;
    }

    cmd_action = argv[1];

    if (strcmp(cmd_action, "get-mem-info") == 0) {
        if (argc != 3 || atoi(argv[2]) < 0) {
            fprintf(stderr, "Invalid usage: get-meminfo <vm_id>\n");
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "get-mem-info %d", atoi(argv[2]));
    } else if (strcmp(cmd_action, "add-mem") == 0) {
        if (argc != 3 || strncmp(argv[2], "size=", 5) != 0 || atoi(argv[2] + 5) <= 0) {
            fprintf(stderr, "Invalid usage: add-mem size=<mb>\n");
            return -1;
        }
        // TODO: implementation
        snprintf(cmd_full, sizeof(cmd_full), "add-mem %s", argv[2]);
    } else if (strcmp(cmd_action, "get-mem-bw") == 0) {
        if (argc != 2) {
            fprintf(stderr, "Invalid usage: get-mem-bw (no arguments required)\n");
            return -1;
        }
        snprintf(cmd_full, sizeof(cmd_full), "get-mem-bw");
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd_action);
        return -1;
    }

    send_command(cmd_full);
    return 0;
}

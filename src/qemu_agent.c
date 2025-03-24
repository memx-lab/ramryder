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
#include "qemu_agent.h"
#include "util_socket.h"

#define QMP_SOCKET "/var/run/qmp-sock-1"

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

char *hotplug_dimm(void)
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
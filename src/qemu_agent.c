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
#include "util_common.h"

#define QMP_SOCKET_PREFIX "/var/run/qmp-sock-"

static int init_qemu_qmp_session(int fd)
{
    int ret;
    char response[BUFFER_SIZE];

    json_object *cmd, *response_json, *ret_obj;
    const char *cmd_str;

    // Step 1: Receive initial QMP greeting
    ret = recv(fd, response, BUFFER_SIZE - 1, 0);
    if (ret <= 0) {
        fprintf(stderr, "Failed to receive QMP greeting.\n");
        return -1;
    }
    response[ret] = '\0';
#ifdef ENABLE_DEBUG
    // do not need line feed for response from qemu which already includes that
    printf("QMP greating: %s", response);
#endif

    // Step 2: Build {"execute": "qmp_capabilities"} using json-c
    cmd = json_object_new_object();
    if (!cmd) {
        fprintf(stderr, "Failed to create JSON object.\n");
        return -1;
    }
    json_object_object_add(cmd, "execute", json_object_new_string("qmp_capabilities"));
    cmd_str = json_object_to_json_string(cmd);

    // Step 3: Send command
    ret = send(fd, cmd_str, strlen(cmd_str), 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to send qmp_capabilities command.\n");
        json_object_put(cmd);
        return -1;
    }

    // Step 4: Receive response
    ret = recv(fd, response, BUFFER_SIZE - 1, 0);
    if (ret <= 0) {
        fprintf(stderr, "Failed to receive response to qmp_capabilities.\n");
        json_object_put(cmd);
        return -1;
    }
    response[ret] = '\0';
#ifdef ENABLE_DEBUG
    // do not need line feed for response from qemu which already includes that
    printf("QMP capabilities response: %s", response);
#endif

    // Step 5: Parse response and check "return" key
    response_json = json_tokener_parse(response);
    if (!response_json) {
        fprintf(stderr, "Invalid JSON received in qmp_capabilities response.\n");
        json_object_put(cmd);
        return -1;
    }

    // only check "return" key to get whether the command is successfully executed
    if (!json_object_object_get_ex(response_json, "return", &ret_obj)) {
        fprintf(stderr, "QMP capabilities negotiation failed. Response: %s\n", response);
        json_object_put(response_json);
        json_object_put(cmd);
        return -1;
    }

    // Clean up
    json_object_put(response_json);
    json_object_put(cmd);

    return 0;
}

static int send_qemu_cmd(int vm_id, const char *command, char *response)
{
    int ret;
    int agent_fd = -1;
    char socket_path[MAX_SOCKET_PATH];

    printf("--------------------------- start QMP process ---------------------------\n");
    printf("Qemu agent command: %s\n", command);

    snprintf(socket_path, MAX_SOCKET_PATH, "%s%d", QMP_SOCKET_PREFIX, vm_id);
    agent_fd = connect_to_socket(socket_path);
    if (agent_fd < 0) {
        fprintf(stderr, "Failed to connect to qemu agent, socket: %s\n", socket_path);
        goto fail;
    }

    ret = init_qemu_qmp_session(agent_fd);
    if (ret < 0) {
        fprintf(stderr, "QMP handshake failed.\n");
        goto fail;
    }

    ret = send(agent_fd, command, strlen(command), 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to send command %s, agentfd: %d\n", command, agent_fd);
        goto fail;
    }

    ret = recv(agent_fd, response, BUFFER_SIZE, 0);
    if (ret <= 0) {
        fprintf(stderr, "Failed to receive, command: %s, agentfd: %d\n", command, agent_fd);
        goto fail;
    }
    response[ret] = '\0';

    ret = close_sockect(agent_fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to close agentfd: %d", agent_fd);
        // do not go to fail here since the response succeeded
    }

    // Since the response might be differernt, we let callers to check whether it is valid
    printf("Response: %s", response);
    printf("---------------------------- end QMP process ----------------------------\n");
    return 0;

fail:
    if (agent_fd > 0)
        close_sockect(agent_fd);
    printf("---------------------------- end QMP process ----------------------------\n");
    return -1;
}

static int qemu_agent_create_object(int vm_id, struct hotplug_request *request)
{
    int ret;
    json_object *obj_add_cmd, *obj_add_args;
    const char *obj_add_str;
    char response[BUFFER_SIZE];

    obj_add_args = json_object_new_object();
    json_object_object_add(obj_add_args, "qom-type", json_object_new_string("memory-backend-file"));
    json_object_object_add(obj_add_args, "id", json_object_new_string(request->memdev_id));
    json_object_object_add(obj_add_args, "mem-path", json_object_new_string(request->dev_path));
    json_object_object_add(obj_add_args, "size", json_object_new_int64(request->size_bytes));
    json_object_object_add(obj_add_args, "share", json_object_new_boolean(request->share));
    json_object_object_add(obj_add_args, "align", json_object_new_int64(request->align_bytes));

    obj_add_cmd = json_object_new_object();
    json_object_object_add(obj_add_cmd, "execute", json_object_new_string("object-add"));
    json_object_object_add(obj_add_cmd, "arguments", obj_add_args);

    obj_add_str = json_object_to_json_string(obj_add_cmd);
    ret = send_qemu_cmd(vm_id, obj_add_str, response);
    if (ret < 0) {
        fprintf(stderr, "Failed to create object: %s\n", obj_add_str);
        return -1;
    }
    // TODO: check whether response is valid
    // Note: releasing root json object will release all objects automatically
    json_object_put(obj_add_cmd);

    return 0;
}

static int qemu_agent_free_object(int vm_id, struct hotunplug_request *request)
{
    int ret;
    json_object *obj_free_cmd, *obj_free_args;
    const char *obj_free_str;
    char response[BUFFER_SIZE];

    // e.g., { "execute": "object_del", "arguments": { "id": "mem2" } }
    obj_free_args = json_object_new_object();
    json_object_object_add(obj_free_args, "id", json_object_new_string(request->memdev_id));

    obj_free_cmd = json_object_new_object();
    json_object_object_add(obj_free_cmd, "execute", json_object_new_string("object-del"));
    json_object_object_add(obj_free_cmd, "arguments", obj_free_args);

    obj_free_str = json_object_to_json_string(obj_free_cmd);
    ret = send_qemu_cmd(vm_id, obj_free_str, response);
    if (ret < 0) {
        fprintf(stderr, "Failed to create object: %s\n", obj_free_str);
        return -1;
    }
    // TODO: check whether response is valid
    // Note: releasing root json object will release all objects automatically
    json_object_put(obj_free_cmd);

    return 0;
}

static int qemu_agent_add_device(int vm_id, struct hotplug_request *request)
{
    int ret;
    json_object *dev_add_cmd, *dev_add_args;
    const char *dev_add_str;
    char response[BUFFER_SIZE];

    dev_add_args = json_object_new_object();
    json_object_object_add(dev_add_args, "driver", json_object_new_string("pc-dimm"));
    json_object_object_add(dev_add_args, "id", json_object_new_string(request->dimm_id));
    json_object_object_add(dev_add_args, "memdev", json_object_new_string(request->memdev_id));
    json_object_object_add(dev_add_args, "node", json_object_new_int(request->numa_node));

    dev_add_cmd = json_object_new_object();
    json_object_object_add(dev_add_cmd, "execute", json_object_new_string("device_add"));
    json_object_object_add(dev_add_cmd, "arguments", dev_add_args);

    dev_add_str = json_object_to_json_string(dev_add_cmd);

    ret = send_qemu_cmd(vm_id, dev_add_str, response);
     if (ret < 0) {
        fprintf(stderr, "Failed to add device %s\n", dev_add_str);
        return -1;
    }
    // TODO: check whether response is valid
    // Note: releasing root json object will release all objects automatically
    json_object_put(dev_add_cmd);

    return 0;
}

static int qemu_agent_del_device(int vm_id, struct hotunplug_request *request)
{
    int ret;
    json_object *dev_del_cmd, *dev_del_args;
    const char *dev_del_str;
    char response[BUFFER_SIZE];

    // e.g., { "execute": "device_del", "arguments": { "id": "dimm0" } }
    dev_del_args = json_object_new_object();
    json_object_object_add(dev_del_args, "id", json_object_new_string(request->dimm_id));

    dev_del_cmd = json_object_new_object();
    json_object_object_add(dev_del_cmd, "execute", json_object_new_string("device_del"));
    json_object_object_add(dev_del_cmd, "arguments", dev_del_args);

    dev_del_str = json_object_to_json_string(dev_del_cmd);

    ret = send_qemu_cmd(vm_id, dev_del_str, response);
     if (ret < 0) {
        fprintf(stderr, "Failed to delete device %s\n", dev_del_str);
        return -1;
    }
    // TODO: check whether response is valid
    // Note: releasing root json object will release all objects automatically
    json_object_put(dev_del_cmd);

    return 0;
}

int qemu_agent_hotplug_memory(int vm_id, struct hotplug_request *request)
{
    int ret;

    // Step 1: we need to create a memory object before attaching to VMs
    ret = qemu_agent_create_object(vm_id, request);
    if (ret < 0) {
        fprintf(stderr, "Failed to hotplug memory to VM %d\n", vm_id);
        return -1;
    }

    // Step 2: hot-plug memory device
    ret = qemu_agent_add_device(vm_id, request);
    if (ret < 0) {
        fprintf(stderr, "Failed to hotplug memory to VM %d\n", vm_id);
        return -1;
    }

    return 0;
}

int qemu_agent_hotunplug_memory(int vm_id, struct hotunplug_request *request)
{
    int ret;

    // Step 1: hot-unplug memory device from VM
    ret = qemu_agent_del_device(vm_id, request);
    if (ret < 0) {
        fprintf(stderr, "Failed to hotunplug memory from VM %d\n", vm_id);
        return -1;
    }

    /* Step 2: wait QEMU clear the device before deteling the object */
    usleep(US_PER_SECOND);

    // Step 3: detele memory object
    ret = qemu_agent_free_object(vm_id, request);
    if (ret < 0) {
        fprintf(stderr, "Failed to hotunplug memory from VM %d\n", vm_id);
        return -1;
    }

    return 0;
}
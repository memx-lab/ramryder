#!/bin/bash
# common.sh - Common settings for scripts

# Find the directory where common.sh itself is located
COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Project root = common.sh directory's parent
PROJECT_ROOT="$(cd "$COMMON_DIR/.." && pwd)"

# Socket path for qemu and guest agents
SOCK_PATH=/var/run
# image directory
IMGDIR=$HOME/images
# Resource client
RPC_CLIENT_BIN="sudo $PROJECT_ROOT/src/resource_client"
# qemu binary
QEMU_BIN="$PROJECT_ROOT/qemu/build/qemu-system-x86_64"

function create_vm_instance() {
    local vm_id=$1 core_set=$2

    # format: resouece_client create-vm vid=<vm id> coreset=[20-30,50-60]
    response="$($RPC_CLIENT_BIN create-vm vid=$vm_id coreset=[$core_set])"
    echo "$response"
}

function allocate_memory_object() {
    local tier_id=$1 dax_id=$2 vm_id=$3 size_mb=$4
    local response

    # format: resouece_client allocate-mem tid=<tid> did=<dev id> vid=<VM id> size=<mb>
    response="$($RPC_CLIENT_BIN allocate-mem tid=$tier_id did=$dax_id vid=$vm_id size=$size_mb)"
    echo "-object memory-backend-file,share=on,$response"
}

function allocate_numa_node() {
    local node_id=$1
    local response

    # format: get-node-info nid=<node_id>
    response="$($RPC_CLIENT_BIN get-node-info nid=$node_id)"
    echo "-numa node,$response,seg-id=0"
}
#!/bin/bash
# common.sh - Common settings for scripts

# Find the directory where common.sh itself is located
COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Project root = common.sh directory's parent
PROJECT_ROOT="$(cd "$COMMON_DIR/.." && pwd)"

# Socket path for qemu and guest agents
SOCK_PATH=/var/run
# Resource client
RPC_CLIENT_BIN="sudo $PROJECT_ROOT/src/resource_client"

# qemu related
function allocate_memory_object() {
    local mem_id=$1 tier_id=$2 dax_id=$3 vm_id=$4 size_mb=$5
    local response

    response="$($RPC_CLIENT_BIN allocate-mem tid=$tier_id did=$dax_id vid=$vm_id size=$size_mb)"
    echo "-object memory-backend-file,id=mem$mem_id,share=on,$response"
}

function allocate_numa_node() {
    local node_id=$1
    local response

    response="$($RPC_CLIENT_BIN get-node-info nid=$node_id)"
    echo "-numa node,$response,seg-id=0"
}
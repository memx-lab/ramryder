#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

$RPC_CLIENT_BIN create-vm vid=1 coreset=[0-19,40-59]

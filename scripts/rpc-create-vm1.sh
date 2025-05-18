#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

$RPC_CLIENT_BIN create-vm vid=0 coreset=[20-39,60-79]

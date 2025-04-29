#!/bin/bash
# common.sh - Common settings for scripts

# Find the directory where common.sh itself is located
COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Project root = common.sh directory's parent
PROJECT_ROOT="$(cd "$COMMON_DIR/.." && pwd)"

# Socket path for qemu and guest agents
SOCK_PATH=/var/run
#!/bin/bash
# pkgdep.sh - Install required packages based on Linux distribution

set -e

# Detect platform
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO_ID=$ID
    else
        echo "[ERROR] Cannot detect OS distribution. /etc/os-release not found."
        exit 1
    fi
}

# Install libcurl development package
install_libcurl() {
    echo "[INFO] Installing libcurl development package for $DISTRO_ID..."

    case "$DISTRO_ID" in
        ubuntu|debian)
            sudo apt update
            sudo apt install -y libcurl4-openssl-dev
            ;;
        centos|rhel)
            sudo yum install -y libcurl-devel
            ;;
        fedora)
            sudo dnf install -y libcurl-devel
            ;;
        *)
            echo "[ERROR] Unsupported distribution: $DISTRO_ID"
            exit 1
            ;;
    esac

    echo "[INFO] libcurl installation completed."
}

# Main
detect_distro
install_libcurl

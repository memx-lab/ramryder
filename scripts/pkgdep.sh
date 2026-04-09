#!/bin/bash
# pkgdep.sh - Install required packages based on Linux distribution

set -e

# Detect platform
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO_ID=$ID
    else
        echo "[ERROR] Cannot detect OS distribution. /etc/os-release not found."
        exit 1
    fi
}

# Install python related libs
install_python_lib() {
    if python3 -c 'import tomllib' >/dev/null 2>&1; then
        echo "[INFO] Python 3.11+ detected, built-in tomllib is available. Skipping tomli installation."
        return
    fi

    if python3 -c 'import tomli' >/dev/null 2>&1; then
        echo "[INFO] Python package tomli is already available."
        return
    fi

    echo "[INFO] Installing Python package tomli for $DISTRO_ID..."

    case "$DISTRO_ID" in
        ubuntu|debian)
            sudo apt update
            sudo apt install -y python3-tomli
            ;;
        centos|rhel)
            sudo yum install -y python3-tomli
            ;;
        fedora)
            sudo dnf install -y python3-tomli
            ;;
        *)
            echo "[ERROR] Unsupported distribution for tomli installation: $DISTRO_ID"
            exit 1
            ;;
    esac
}

# Install libcurl development package
install_sys_lib() {
    echo "[INFO] Installing libcurl and libpfm development packages for $DISTRO_ID..."

    case "$DISTRO_ID" in
        ubuntu|debian)
            sudo apt install -y libcurl4-openssl-dev
            sudo apt install -y libpfm4-dev
            ;;
        centos|rhel)
            sudo yum install -y libcurl-devel
            sudo yum install -y libpfm-devel
            ;;
        fedora)
            sudo dnf install -y libcurl-devel
            sudo dnf install -y libpfm-devel
            ;;
        *)
            echo "[ERROR] Unsupported distribution: $DISTRO_ID"
            exit 1
            ;;
    esac

    echo "[INFO] System library installation completed."
}

# Main
detect_os
install_python_lib
install_sys_lib

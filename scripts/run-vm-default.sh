#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s.qcow2
# qemu binary
QEMU_BIN="$PROJECT_ROOT/qemu/build/qemu-system-x86_64"

sudo $QEMU_BIN \
    -name "Test-VM" \
    -enable-kvm \
    -cpu host \
    -smp 16 \
    -m 8G \
    -object memory-backend-file,id=mem0,size=8G,mem-path=/dev/hugepages,share=on,prealloc=yes \
    -numa node,memdev=mem0 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2806-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s2.qcow2
# qemu binary
QEMU_BIN="$PROJECT_ROOT/qemu/build/qemu-system-x86_64"

#CPU_BIND_12='taskset -c 26-31,66-71'
CPU_BIND_20='taskset -c 30-39,70-79'
#CPU_BIND_24='taskset -c 28-39,68-79'

sudo $CPU_BIND_20 $QEMU_BIN \
    -name "VM-T2" \
    -enable-kvm \
    -cpu host \
    -smp 20 \
    -m 20G \
    -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax0.0,size=20G,align=2M \
    -numa node,memdev=mem1 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2807-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

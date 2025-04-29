#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# unique ID for each VM
VMID=0

NAME=VM-NUMA-$VMID
QMP_SOCK=$SOCK_PATH/qmp-sock-$VMID
QGA_SOCK=$SOCK_PATH/qga-sock-$VMID

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s.qcow2
# qemu binary
QEMU_BIN="$PROJECT_ROOT/qemu/build/qemu-system-x86_64"

CPU_BIND_12='taskset -c 20-25,60-65'
CPU_BIND_16='taskset -c 20-27,60-67'
CPU_BIND_20='taskset -c 20-29,60-69'
CPU_BIND_24='taskset -c 20-31,60-71'
CPU_BIND_40='taskset -c 20-39,60-79'

sudo $CPU_BIND_40 $QEMU_BIN \
    -name $NAME \
    -enable-kvm \
    -cpu host \
    -smp 40 \
    -m 40G,slots=256,maxmem=512G \
    -object memory-backend-file,id=mem0,share=on,mem-path=/dev/dax0.0,size=20G,align=2M \
    -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax1.0,size=20G,align=2M \
    -object memory-backend-file,id=mem2,share=on,mem-path=/dev/dax1.0,size=20G,align=2M,offset=20G \
    -numa node,nodeid=0,memdev=mem0,cpus=0-39,tier-id=0,dax-id=0,seg-id=0 \
    -numa node,nodeid=1,memdev=mem1,tier-id=0,dax-id=1,seg-id=0 \
    -device pc-dimm,id=dimm0,memdev=mem2,node=1 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2806-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:$QMP_SOCK,server,nowait \
    -chardev socket,path=$QGA_SOCK,server=on,wait=off,id=qga0 \
    -device virtio-serial \
    -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0 \
    2>&1 | tee log




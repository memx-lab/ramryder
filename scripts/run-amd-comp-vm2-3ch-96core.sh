#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# unique ID for each VM
VMID=2
# VM OS image
OSIMGF=$IMGDIR/nvcloud-image2.qcow2
# VM disk image
DISK=$IMGDIR/mydisk2.img

NAME=VM-NUMA-$VMID
QMP_SOCK=$SOCK_PATH/qmp-sock-$VMID
QGA_SOCK=$SOCK_PATH/qga-sock-$VMID

# pre-define some core sets for convinience
CPU_SET_32="16-31,144-159"
CPU_SET_64="32-63,160-191"
CPU_SET_96="16-63,144-191"
CPU_SET="$CPU_SET_96"

# must create VM instance before allocating
create_vm_instance $VMID $CPU_SET
mem0=$(allocate_memory_object 0 3 $VMID 8192)
mem1=$(allocate_memory_object 0 4 $VMID 8192)
mem2=$(allocate_memory_object 0 5 $VMID 8192)
node0=$(allocate_numa_node 0)
node1=$(allocate_numa_node 1)
node2=$(allocate_numa_node 2)
node3=$(allocate_numa_node 3)

# must use memX as *memdev* id
sudo taskset -c $CPU_SET $QEMU_BIN \
    -name $NAME \
    -enable-kvm \
    -cpu host \
    -smp 96 \
    -m 24G,slots=256,maxmem=1024G \
    $mem0 \
    $mem1 \
    $mem2 \
    $node0,memdev=mem0,cpus=0-95 \
    $node1,memdev=mem1 \
    $node2,memdev=mem2 \
    $node3 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -drive file=$DISK,format=qcow2,if=virtio \
    -net user,hostfwd=tcp::2807-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:$QMP_SOCK,server,nowait \
    -chardev socket,path=$QGA_SOCK,server=on,wait=off,id=qga0 \
    -device virtio-serial \
    -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0 \
    2>&1 | tee log
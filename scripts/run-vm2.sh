#!/bin/bash

# Load common settings
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# unique ID for each VM
VMID=2
# VM OS image
OSIMGF=$IMGDIR/nvcloud-image-2.qcow2

NAME=VM-NUMA-$VMID
QMP_SOCK=$SOCK_PATH/qmp-sock-$VMID
QGA_SOCK=$SOCK_PATH/qga-sock-$VMID

#CPU_SET_40="10-19,30-39,50-59,70-79"
CPU_SET_20="10-19,50-59"
CPU_SET="$CPU_SET_20"

# must create VM instance before allocating
create_vm_instance $VMID $CPU_SET
mem0=$(allocate_memory_object 0 0 $VMID 4096)
node0=$(allocate_numa_node 0)
node1=$(allocate_numa_node 1)
node2=$(allocate_numa_node 2)
node3=$(allocate_numa_node 3)

# must use memX as *memdev* id
sudo taskset -c $CPU_SET $QEMU_BIN \
    -name $NAME \
    -enable-kvm \
    -cpu host \
    -smp 20 \
    -m 4G,slots=256,maxmem=1024G \
    $mem0 \
    $node0,memdev=mem0,cpus=0-19 \
    $node1 \
    $node2 \
    $node3 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2807-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:$QMP_SOCK,server,nowait \
    -chardev socket,path=$QGA_SOCK,server=on,wait=off,id=qga0 \
    -device virtio-serial \
    -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0 \
    2>&1 | tee log
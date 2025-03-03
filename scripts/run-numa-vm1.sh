#!/bin/bash

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s.qcow2
# qemu binary
QEMU_BIN=/home/yaz093/qemu/build/qemu-system-x86_64
CPU_BIND_12='taskset -c 20-25,60-65'
CPU_BIND_16='taskset -c 20-27,60-67'
CPU_BIND_20='taskset -c 20-29,60-69'
CPU_BIND_24='taskset -c 20-31,60-71'
CPU_BIND_40='taskset -c 20-39,60-79'

sudo $CPU_BIND_24 $QEMU_BIN \
    -name "VM-NUMA-1" \
    -enable-kvm \
    -cpu host \
    -smp 40 \
    -m 60G \
    -object memory-backend-file,id=mem0,share=on,mem-path=/dev/dax0.0,size=20G,align=2M \
    -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax1.0,size=20G,align=2M \
    -object memory-backend-file,id=mem2,share=on,mem-path=/dev/dax1.0,size=20G,align=2M,offset=20G \
    -numa node,nodeid=0,memdev=mem0,cpus=0-23,tier-id=0,dax-id=0,seg-id=1 \
    -numa node,nodeid=1,memdev=mem1,tier-id=0,dax-id=1,seg-id=1 \
    -numa node,nodeid=2,memdev=mem2,tier-id=0,dax-id=1,seg-id=1 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2806-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

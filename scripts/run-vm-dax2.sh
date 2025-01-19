#!/bin/bash

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s2.qcow2
# qemu binary
QEMU_BIN=/home/yaz093/qemu/build/qemu-system-x86_64
CPU_BIND='taskset -c 40-59' 

sudo $CPU_BIND $QEMU_BIN \
    -name "VM-T2" \
    -enable-kvm \
    -cpu host \
    -smp 20 \
    -m 36G \
    -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax0.0,size=36G,align=2M,offset=8G \
    -numa node,memdev=mem1 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2807-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

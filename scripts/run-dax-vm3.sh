#!/bin/bash

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s3.qcow2
# qemu binary
QEMU_BIN=/home/yaz093/qemu/build/qemu-system-x86_64
CPU_BIND='taskset -c 32-37,72-77'

sudo $CPU_BIND $QEMU_BIN \
    -name "VM-T3" \
    -enable-kvm \
    -cpu host \
    -smp 12 \
    -m 36G \
    -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax0.0,size=36G,align=2M,offset=8G \
    -numa node,memdev=mem1 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2808-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

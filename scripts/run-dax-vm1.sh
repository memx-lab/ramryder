#!/bin/bash

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/u20s.qcow2
# qemu binary
QEMU_BIN=/home/yaz093/qemu/build/qemu-system-x86_64
CPU_BIND='taskset -c 20-25,60-65' 

sudo $CPU_BIND $QEMU_BIN \
    -name "VM-T1" \
    -enable-kvm \
    -cpu host \
    -smp 12 \
    -m 44G \
    -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax1.0,size=44G,align=2M \
    -numa node,memdev=mem1 \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::2806-:22 \
    -net nic,model=virtio \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

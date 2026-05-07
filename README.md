<p align="center">
  <img src="logos/logo_small_trim.png" alt="RamRyder Logo" width="80%" />
</p>

# About

RamRyder is a software-defined elastic memory system for cloud virtual machines. Its core idea is to manage and allocate memory channels in software, allowing users to control the memory capacity and bandwidth of each virtual machine based on application demands.

The main components of RamRyder include a user-space resource manager, a hypervisor extended from QEMU, and a guest Linux kernel.

# Quick Start

This page describes how to build RamRyder, configure the resource manager, and manage VMs with `ramryder_cli`.

## Get source code

```bash
git clone --recurse-submodules git@github.com:memx-lab/ramryder.git
```

## Build project

From the RamRyder directory:

### Install dependency
```bash
./scripts/pkgdep.sh
```

### Build resource manager
```bash
# use --arch-cpu-amd to configure if you run on AMD server. Default: Intel
./configure [--arch-cpu-amd]
make
```

### Build QEMU
```bash
cd qemu
mkdir -p build
cd build
../configure --enable-kvm --target-list=x86_64-softmmu --enable-slirp
make -j$(nproc)
cd ../..
```

## Configure hardware resource

### Configure hardware

RamRyder supports multiple memory hardware types (for example DIMM, PMEM, and CXL). Before configuring resource manager, first prepare and expose your memory devices correctly on the host. For hardware-specific setup steps, see [Document - Hardware Support](https://memx-lab.github.io/docs/hardware-support/overview).

### Set configuration file

Create `src/elesticmm.conf` (or copy from `src/elasticmm_default.conf`) and
configure your memory devices:

```ini
[global]
# size in MB
segment_size_mb 128
monitor_interval_second 1

[devices]
# the combination of tier id and dev id should be unique
dev path=/dev/dax0.0 size_mb=20480 tier_id=0 dax_id=0
dev path=/dev/dax1.0 size_mb=20480 tier_id=0 dax_id=1

[clouddb]
enable_clouddb false
influxdb_url <url>
influxdb_token <token>
use_proxy false
proxy_addr <proxy addr>
```

The minimum required configuration is the `[devices]` section. Each memory
device should be exposed as a DAX device under `/dev` so it can be managed in
userspace.

For each DAX device:
- use `tier_id=0` for local memory (DIMM)
- use `tier_id=1` for CXL memory
- keep `dax_id` unique within the same `tier_id`

## Start resource manager

```bash
cd src
sudo ./resource_manager
```

`sudo` is required because resource manager reads host performance counters.

## Get VM image

We provide a clean Ubuntu VM image: [Download Link](https://drive.google.com/file/d/1DASrFSRzh7dV2UX0fINgHhx10W13yZdz/view?usp=sharing).

```bash
tar -xf nvcloud-image-clean.tar.xz
```

Then refer to `readme.txt` inside the package for login information.

## Create VM
All VM operations are managed by `admin/ramryder_cli`. You can use this tool to query resource allocations, allocate resources, and create VMs. Use `ramryder_cli --help` to check usage.

Create a VM with DIMM + CXL memory:
```bash
./admin/ramryder_cli create-vm \
  --cpu-set 0-9,20-29 \
  --memory 100G \
  --channels 4 \
  --cxl-memory 50G \
  --cxl-channels 2 \
  --image /path/to/nvcloud-image-clean.qcow2
```

Important behavior:
- `--memory/--channels` are for local memory (DIMM, tier 0).
- `--cxl-memory/--cxl-channels` are for CXL memory (tier 1).
- `--image` sets the VM qcow2 path (default: `~/images/nvcloud-image-clean.qcow2`).
- SSH forwarding port is generated automatically.
- use `ssh -p <port> <user>@localhost` to log into the VM.

## Update guest kernel
After VM is ready, log into VM and then update guest kernel as follows.

### Get source code

```bash
git clone git@github.com:memx-lab/ramos.git
cd ramos
```

### Prepare
Install the required build dependencies first.

Run all remaining commands in the kernel source tree:

```bash
sudo apt-get update
sudo apt-get install build-essential libncurses5 libncurses5-dev bin86 kernel-package libssl-dev bison flex libelf-dev dwarves
```

### Configure kernel

Start from the current system configuration:

```bash
cp /boot/config-$(uname -r) .config
make olddefconfig
```

Then open the configuration menu:

```bash
make menuconfig
```

In `make menuconfig`, enable the following RAMOS options under `General setup`:  
- `RAMOS NUMA abstraction support`
- `RAMOS debug mode` (optional, for more verbose log output)

### Build and install
Use the following commands for a full kernel build and installation:

```bash
make -j$(nproc)
make -j$(nproc) modules
sudo make INSTALL_MOD_STRIP=1 modules_install
sudo make install
```

Then reboot the VM and select new kernel `Linux 6.3.0-ramos+`.

Note that `INSTALL_MOD_STRIP=1` removes debug symbols from kernel modules. This reduces
build time and saves storage space, but you may want to keep debug symbols if
you plan to use `gdb`.

### Boot updates

These steps are optional because `make install` already handles the required
boot updates in most cases. The commands below are kept here for reference in
case the new kernel does not appear after reboot.

```bash
update-initramfs -c -k [kernel version full name]
update-grub
```

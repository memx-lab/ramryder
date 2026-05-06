<p align="center">
  <img src="logos/logo_large_dark_bg.png" alt="RamRyder Logo" />
</p>

# About

RamRyder is a software-defined elastic memory system for cloud virtual machines. Its core idea is to manage and allocate memory channels in software, allowing users to control the memory capacity and bandwidth of each virtual machine based on application demands.

The main components of RamRyder include a user-space resource manager, a hypervisor extended from QEMU, and a guest Linux kernel.

# Get Started

This page describes how to build RamRyder, configure the resource manager, and
manage VMs with `ramryder_cli`.

### Get source code

```bash
git clone --recurse-submodules git@github.com:memx-lab/ramryder.git
```

If you already cloned the repository without `--recurse-submodules`, run:

```bash
git submodule update --init --recursive
```

### Build project

From the RamRyder directory:

#### Build resource manager

```bash
./scripts/pkgdep.sh
# use --arch-cpu-amd to configure if you run on AMD server. Default: Intel
./configure [--arch-cpu-amd]
make
```

#### Build QEMU

```bash
cd qemu
mkdir -p build
cd build
../configure --enable-kvm --target-list=x86_64-softmmu --enable-slirp
make -j$(nproc)
cd ../..
```

### Configure hardware resource

#### 1) Configure hardware

RamRyder supports multiple memory hardware types (for example DIMM, PMEM, and CXL). Before configuring resource manager, first prepare and expose your memory devices correctly on the host.

For hardware-specific setup steps, see [Document - Hardware Support](https://memx-lab.github.io/docs/hardware-support/overview).

#### 2) Configure resource manager file

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

For details, refer to [Document - Get Started](https://memx-lab.github.io/docs/memx-ramryder/build)

### Start resource manager

```bash
cd src
sudo ./resource_manager
```

`sudo` is required because resource manager reads host performance counters.

### Get VM image

We provide a clean Ubuntu VM image: [Download Link](https://drive.google.com/file/d/1DASrFSRzh7dV2UX0fINgHhx10W13yZdz/view?usp=sharing).

```bash
tar -xf nvcloud-image-clean.tar.xz
```

Then refer to `readme.txt` inside the package for login information.

### VM management

All VM operations are managed by `admin/ramryder_cli`. You can use
`ramryder_cli` to query resource allocations, allocate resources, and create
VMs.

```bash
cd /path/to/ramryder
./admin/ramryder_cli --help
```

#### 1) Query resource status

Memory pool:

```bash
./admin/ramryder_cli query --type mempool
```

VM allocation status:

```bash
./admin/ramryder_cli query --type vm
```

#### 2) Create VM

Create a VM with local memory (DIMM):

```bash
./admin/ramryder_cli create-vm \
  --cpu-set 0-9,20-29 \
  --memory 150G \
  --channels 6
```

Create a VM with DIMM + CXL memory:

```bash
./admin/ramryder_cli create-vm \
  --cpu-set 0-9,20-29 \
  --memory 100G \
  --channels 4 \
  --cxl-memory 50G \
  --cxl-channels 2
```

Important behavior:
- `--memory/--channels` are for local memory (DIMM, tier 0).
- `--cxl-memory/--cxl-channels` are for CXL memory (tier 1).
- memory size supports `M` or `G` units (for example `1024M`, `150G`).
- default SSH forwarding port is `2806 + VMID`.
- if that port is occupied, CLI chooses another available port.
- vCPU pinning is enabled by default; disable with `--disable-vcpu-pin`.

Dry-run example (execute RM allocation flow and auto cleanup without launching
VM):

```bash
./admin/ramryder_cli create-vm \
  --cpu-set 0-9,20-29 \
  --memory 150G \
  --channels 6 \
  --dry-run
```

#### 3) Dynamically add memory to a running VM

Add DIMM memory from new channels:

```bash
./admin/ramryder_cli attach-mem \
  --vmid 0 \
  --memory 20G \
  --channels 2
```

Add DIMM memory only from channels already allocated to this VM:

```bash
./admin/ramryder_cli attach-mem \
  --vmid 0 \
  --memory 10G \
  --channels 0
```

Add CXL memory:

```bash
./admin/ramryder_cli attach-mem \
  --vmid 0 \
  --memory 0M \
  --channels 0 \
  --cxl-memory 20G \
  --cxl-channels 1
```

`attach-mem` prints allocated `memid` values. Keep them for detach operations.

#### 4) Dynamically remove memory

```bash
./admin/ramryder_cli detach-mem --vmid 0 --memid 3
```

`memid` is numeric (`mem3 -> 3`).

#### 5) Destroy VM

```bash
./admin/ramryder_cli destroy-vm --vmid 0
```

This command kills the VM process and releases all resource-manager allocations.

### Update guest kernel

After VM is ready, log into VM and then follow [RAMOS Get Started](https://memx-lab.github.io/docs/memx-ramos/build) to update guest kernel.

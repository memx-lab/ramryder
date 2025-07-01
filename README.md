# Project Overview

Project Elastic Memory

## Repository Structure

```
project/
├── src/           # Project source code
├── test/          # Test cases 
├── qemu/          # Submodule: QEMU with elastic memory support
├── include/       # Third-party Header files
├── lib/           # Third-party Libs
├── scripts/       # Project scripts
├── analysis/      # Data analysis scripts
├── ...
```

## Build Project
#### Get source code
```bash
git clone --recurse-submodules git@github.com:yanbozyb/elastic-memory.git
```

If you already cloned the repository without `--recurse-submodules`, run:

```bash
git submodule update --init --recursive
```

#### Build main project

1. Make sure all submodules are properly pulled (see above).

2. Compile the project:

```bash
cd src
make
```

#### Build Qemu
```bash
cd qemu
mkdir build
cd build
../configure --enable-kvm --target-list=x86_64-softmmu --enable-slirp
make -j$(nproc)
```

## Quick Start
#### Get NVSL Cloud Image
Please reach out to the maintainer (yaz093@ucsd.edu) to get image and refer to readme.txt in the image package to get login information.
```bash
tar -xf nvcloud-image-clean.tar.xz
```

#### Launch VM instance
```bash
run-vm.sh
```

## Notes on Submodules

- **Submodules are not automatically updated** when you `git pull`.  
  After pulling updates to the main repository, always run:

```bash
git submodule update --recursive
```

- To update the submodule to the latest commit (e.g., latest `main` branch):

```bash
cd qemu
git checkout main
git pull origin main
cd ..
git add qemu
git commit -m "Update qemu submodule to latest commit"
git push
```


#!/usr/bin/env python3
import argparse
import os
import random
import shlex
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import List, Tuple

BASE_PORT = 2806
VM_NAME_PREFIX = "RAMRYDER-VM"


@dataclass
class VmConfig:
    memory_mb: int
    channels: int
    cpu_set: str
    image: str
    hostfwd_port: int
    dry_run: bool


def parse_memory_to_mb(raw: str) -> int:
    s = raw.strip().lower()
    if s.endswith("gb"):
        return int(s[:-2]) * 1024
    if s.endswith("g"):
        return int(s[:-1]) * 1024
    if s.endswith("mb"):
        return int(s[:-2])
    if s.endswith("m"):
        return int(s[:-1])
    return int(s)


def parse_cpu_set(cpu_set: str) -> List[int]:
    cpus = set()
    for part in cpu_set.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_s, end_s = part.split("-", 1)
            start = int(start_s)
            end = int(end_s)
            if end < start:
                raise ValueError(f"Invalid cpu range: {part}")
            cpus.update(range(start, end + 1))
        else:
            cpus.add(int(part))
    if not cpus:
        raise ValueError("cpu-set resolves to empty CPU list")
    return sorted(cpus)


def split_memory(total_mb: int, channels: int) -> List[int]:
    base = total_mb // channels
    rem = total_mb % channels
    result = [base] * channels
    for i in range(rem):
        result[i] += 1
    return result


def run_cmd(cmd: List[str], dry_run: bool) -> str:
    printable = " ".join(shlex.quote(x) for x in cmd)
    print(f"[cmd] {printable}")
    if dry_run:
        return ""
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        if proc.stdout:
            print(proc.stdout.strip(), file=sys.stderr)
        if proc.stderr:
            print(proc.stderr.strip(), file=sys.stderr)
        raise RuntimeError(f"Command failed ({proc.returncode}): {printable}")
    return proc.stdout.strip()


def run_cmd_capture(cmd: List[str]) -> str:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    out = (proc.stdout or "").strip()
    err = (proc.stderr or "").strip()
    return out if out else err


def find_qemu_pids_by_vmid(vmid: int) -> List[int]:
    pattern = f"qemu-system-x86_64.*-name {VM_NAME_PREFIX}-{vmid}"
    proc = subprocess.run(["pgrep", "-f", pattern], capture_output=True, text=True)
    if proc.returncode != 0:
        return []
    pids: List[int] = []
    for line in (proc.stdout or "").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            pids.append(int(line))
        except ValueError:
            continue
    return pids


def get_used_vm_ids(rpc_client: str) -> List[int]:
    output = run_cmd_capture(["sudo", rpc_client, "get-vms"])
    used = set()
    for line in output.splitlines():
        line = line.strip()
        if not line.startswith("VM id="):
            continue
        token = line.split()[1]
        if not token.startswith("id="):
            continue
        try:
            used.add(int(token.split("=", 1)[1]))
        except ValueError:
            continue
    return sorted(used)


def get_num_nodes(rpc_client: str) -> int:
    output = run_cmd_capture(["sudo", rpc_client, "get-num-nodes"]).strip()
    try:
        num_nodes = int(output)
    except ValueError as exc:
        raise RuntimeError(f"failed to parse get-num-nodes output: {output}") from exc
    if num_nodes <= 0:
        raise RuntimeError(f"invalid node count from resource manager: {num_nodes}")
    return num_nodes


def build_paths() -> Tuple[str, str, str]:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    qemu_bin = os.path.join(project_root, "qemu", "build", "qemu-system-x86_64")
    rpc_client = os.path.join(project_root, "src", "resource_client")
    default_img = os.path.join(os.path.expanduser("~"), "images", "nvcloud-image-clean.qcow2")
    return qemu_bin, rpc_client, default_img


def to_m_arg(memory_mb: int) -> str:
    if memory_mb % 1024 == 0:
        return f"{memory_mb // 1024}G"
    return f"{memory_mb}M"


def is_tcp_port_in_use(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.bind(("0.0.0.0", port))
            return False
        except OSError:
            return True


def build_parser(default_img: str) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="RamRyder VM CLI")
    subparsers = parser.add_subparsers(dest="command", required=True)

    create_parser = subparsers.add_parser("create-vm", help="Create and launch a VM")
    create_parser.add_argument("--memory", required=True, help="Total memory (MB or with suffix like 150G)")
    create_parser.add_argument("--channels", required=True, type=int, help="Number of channels to allocate")
    create_parser.add_argument("--cpu-set", required=True, help="Host CPU set, e.g. 0-9,20-29")
    create_parser.add_argument("--image", default=default_img, help="VM image path")
    create_parser.add_argument("--hostfwd-port", type=int, help="Host forwarded SSH port (default: base port (2806) + VMID)")
    create_parser.add_argument("--dry-run", action="store_true", help="Execute RPC steps but do not launch final QEMU process")

    destroy_parser = subparsers.add_parser("destroy-vm", help="Destroy a VM and release resources")
    destroy_parser.add_argument("--vmid", required=True, type=int, help="VM ID to destroy")
    return parser


def select_vmid(rpc_client: str) -> int:
    existing_vmids = get_used_vm_ids(rpc_client)
    if existing_vmids:
        print(f"[info] existing vm ids: {','.join(str(v) for v in existing_vmids)}")
    else:
        print("[info] existing vm ids: (none detected)")

    used = set(existing_vmids)
    vmid = 0
    while vmid in used:
        vmid += 1
    return vmid


def select_hostfwd_port(requested: int, vmid: int) -> int:
    if requested is not None:
        return requested

    port = BASE_PORT + vmid
    if not is_tcp_port_in_use(port):
        return port

    for _ in range(128):
        candidate = port + random.randint(1, 2000)
        if not is_tcp_port_in_use(candidate):
            return candidate
    raise RuntimeError("failed to find a free hostfwd port")


def build_qemu_cmd(
    qemu_bin: str,
    cfg: VmConfig,
    vmid: int,
    smp: int,
    node0_vcpu_range: str,
    hostfwd_port: int,
    rpc_client: str,
    total_nodes: int,
) -> List[str]:
    name = f"{VM_NAME_PREFIX}-{vmid}"
    sock_path = "/var/run"
    qmp_sock = f"{sock_path}/qmp-sock-{vmid}"
    qga_sock = f"{sock_path}/qga-sock-{vmid}"

    per_channel_mb = split_memory(cfg.memory_mb, cfg.channels)
    selected_nodes = list(range(cfg.channels))
    all_nodes = list(range(total_nodes))

    mem_args: List[str] = []
    node_args: List[str] = []

    for mem_idx, node_id in enumerate(selected_nodes):
        size_mb = per_channel_mb[mem_idx]
        out = run_cmd(
            ["sudo", rpc_client, "alloc-mem", f"nid={node_id}", f"vid={vmid}", f"size={size_mb}"],
            False,
        )
        if not out:
            raise RuntimeError("alloc-mem returned empty output; cannot build memdev argument")
        mem_args.append(f"-object memory-backend-file,share=on,{out}")

    for node_id in all_nodes:
        out = run_cmd(["sudo", rpc_client, "get-node-info", f"nid={node_id}"], False)
        if not out:
            raise RuntimeError("get-node-info returned empty output; cannot build numa node argument")
        base = f"-numa node,{out},seg-id=0"
        if node_id < cfg.channels:
            mem_idx = node_id
            if mem_idx == 0:
                node_args.append(f"{base},memdev=mem{mem_idx},cpus={node0_vcpu_range}")
            else:
                node_args.append(f"{base},memdev=mem{mem_idx}")
        else:
            node_args.append(base)

    qemu_cmd: List[str] = [
        "sudo",
        "taskset",
        "-c",
        cfg.cpu_set,
        qemu_bin,
        "-name",
        name,
        "-enable-kvm",
        "-cpu",
        "host",
        "-smp",
        str(smp),
        "-m",
        to_m_arg(cfg.memory_mb),
    ]

    for item in mem_args:
        qemu_cmd.extend(shlex.split(item))
    for item in node_args:
        qemu_cmd.extend(shlex.split(item))

    qemu_cmd.extend(
        [
            "-device",
            "virtio-scsi-pci,id=scsi0",
            "-device",
            "scsi-hd,drive=hd0",
            "-drive",
            f"file={cfg.image},if=none,aio=native,cache=none,format=qcow2,id=hd0",
            "-net",
            f"user,hostfwd=tcp::{hostfwd_port}-:22",
            "-net",
            "nic,model=virtio",
            "-nographic",
            "-qmp",
            f"unix:{qmp_sock},server,nowait",
            "-chardev",
            f"socket,path={qga_sock},server=on,wait=off,id=qga0",
            "-device",
            "virtio-serial",
            "-device",
            "virtserialport,chardev=qga0,name=org.qemu.guest_agent.0",
        ]
    )
    return qemu_cmd


def handle_destroy_vm(args: argparse.Namespace, rpc_client: str) -> int:
    vmid = args.vmid
    pids = find_qemu_pids_by_vmid(vmid)

    for pid in pids:
        try:
            os.kill(pid, 15)
        except ProcessLookupError:
            continue

    if pids:
        time.sleep(1.0)
        for pid in find_qemu_pids_by_vmid(vmid):
            try:
                os.kill(pid, 9)
            except ProcessLookupError:
                continue

    destroy_cmd = ["sudo", rpc_client, "destroy-vm", f"vid={vmid}"]
    print(f"[cmd] {' '.join(shlex.quote(x) for x in destroy_cmd)}")
    out = run_cmd_capture(destroy_cmd)
    if "success" not in out.lower():
        raise RuntimeError(f"destroy-vm failed for vid={vmid}: {out}")

    print(f"destroy success: vmid={vmid} qemu_killed={len(pids)}")
    return 0


def handle_create_vm(args: argparse.Namespace, qemu_bin: str, rpc_client: str) -> int:
    memory_mb = parse_memory_to_mb(args.memory)
    if args.channels <= 0:
        raise ValueError("--channels must be > 0")
    if memory_mb < args.channels:
        raise ValueError("Total memory is too small for channel split")

    cpus = parse_cpu_set(args.cpu_set)
    smp = len(cpus)
    node0_vcpu_range = f"0-{smp - 1}"

    cfg = VmConfig(
        memory_mb=memory_mb,
        channels=args.channels,
        cpu_set=args.cpu_set,
        image=args.image,
        hostfwd_port=args.hostfwd_port,
        dry_run=args.dry_run,
    )

    total_nodes = get_num_nodes(rpc_client)
    print(f"[info] total available nodes (channels): {total_nodes}")
    if cfg.channels > total_nodes:
        raise ValueError(
            f"--channels ({cfg.channels}) must be <= managed node count ({total_nodes})"
        )

    vmid = select_vmid(rpc_client)
    create_cmd = ["sudo", rpc_client, "create-vm", f"vid={vmid}", f"coreset=[{cfg.cpu_set}]"]
    print(f"[cmd] {' '.join(shlex.quote(x) for x in create_cmd)}")
    create_out = run_cmd_capture(create_cmd)
    if "success" not in create_out.lower():
        raise RuntimeError(f"create-vm failed for vid={vmid}: {create_out}")
    print(f"[info] selected vm id: {vmid}")

    hostfwd_port = select_hostfwd_port(cfg.hostfwd_port, vmid)
    print(f"[info] hostfwd port: {hostfwd_port}")

    qemu_started = False
    log_path = f"/tmp/ramryder-vm-{vmid}.log"

    try:
        qemu_cmd = build_qemu_cmd(
            qemu_bin=qemu_bin,
            cfg=cfg,
            vmid=vmid,
            smp=smp,
        node0_vcpu_range=node0_vcpu_range,
        hostfwd_port=hostfwd_port,
        rpc_client=rpc_client,
        total_nodes=total_nodes,
    )

        if cfg.dry_run:
            run_cmd(qemu_cmd, True)
        else:
            printable_qemu = " ".join(shlex.quote(x) for x in qemu_cmd)
            print(f"[cmd] {printable_qemu}")
            with open(log_path, "ab") as logf:
                proc = subprocess.Popen(
                    qemu_cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=logf,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )

            time.sleep(1.0)
            ret = proc.poll()
            if ret is not None:
                print("QEMU launch failed.", file=sys.stderr)
                print(f"log path: {log_path}", file=sys.stderr)
                raise RuntimeError(f"qemu exited early with code {ret}")

            qemu_started = True
            qemu_pids = find_qemu_pids_by_vmid(vmid)
            qemu_pid = qemu_pids[0] if qemu_pids else proc.pid
            print("QEMU launch success.")
            print(f"pid={qemu_pid} vmid={vmid} ssh_port={hostfwd_port} log_path={log_path}")
    finally:
        if cfg.dry_run or not qemu_started:
            try:
                run_cmd(["sudo", rpc_client, "destroy-vm", f"vid={vmid}"], False)
            except Exception as cleanup_err:
                print(f"warning: failed to destroy vm {vmid}: {cleanup_err}", file=sys.stderr)

    return 0


def main() -> int:
    qemu_bin, rpc_client, default_img = build_paths()
    parser = build_parser(default_img)
    args = parser.parse_args()

    if args.command == "create-vm":
        return handle_create_vm(args, qemu_bin, rpc_client)
    elif args.command == "destroy-vm":
        return handle_destroy_vm(args, rpc_client)
    else:
        raise ValueError(f"unknown command: {args.command}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)

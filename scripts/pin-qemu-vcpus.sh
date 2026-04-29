#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  pin-qemu-vcpus.sh --qmp /var/run/qmp-sock-0 --cpus 0-17,20-37 [--dry-run]

Options:
  --qmp PATH          QMP Unix socket path.
  --cpus LIST         Host CPU list/ranges assigned to vCPU0..N, e.g. 2-5 or 0-17,20-37.
  --dry-run           Print the mapping without changing affinity.
  --emulator-cpus LIST
                      Optionally pin the QEMU main/emulator thread to this host CPU list.
  -h, --help          Show this help.

Example:
  sudo scripts/pin-qemu-vcpus.sh --qmp /var/run/qmp-sock-0 --cpus 0-17,20-37
EOF
}

qmp_sock=
cpu_list=
emulator_cpus=
dry_run=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qmp)
            qmp_sock=${2:?missing value for --qmp}
            shift 2
            ;;
        --cpus)
            cpu_list=${2:?missing value for --cpus}
            shift 2
            ;;
        --emulator-cpus)
            emulator_cpus=${2:?missing value for --emulator-cpus}
            shift 2
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$qmp_sock" || -z "$cpu_list" ]]; then
    usage >&2
    exit 2
fi

if [[ ! -S "$qmp_sock" ]]; then
    echo "QMP socket not found: $qmp_sock" >&2
    exit 1
fi

for cmd in jq nc taskset; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Missing required command: $cmd" >&2
        exit 1
    fi
done

expand_cpu_list() {
    local list=$1
    local part start end cpu
    local -a expanded=()

    IFS=',' read -ra parts <<< "$list"
    for part in "${parts[@]}"; do
        if [[ "$part" =~ ^[0-9]+$ ]]; then
            expanded+=("$part")
        elif [[ "$part" =~ ^([0-9]+)-([0-9]+)$ ]]; then
            start=${BASH_REMATCH[1]}
            end=${BASH_REMATCH[2]}
            if (( start > end )); then
                echo "Invalid CPU range: $part" >&2
                exit 2
            fi
            for ((cpu = start; cpu <= end; cpu++)); do
                expanded+=("$cpu")
            done
        else
            echo "Invalid CPU list element: $part" >&2
            exit 2
        fi
    done

    printf '%s\n' "${expanded[@]}"
}

mapfile -t host_cpus < <(expand_cpu_list "$cpu_list")

qmp_reply=$(
    printf '%s\n%s\n' \
        '{"execute":"qmp_capabilities"}' \
        '{"execute":"query-cpus-fast"}' |
    nc -U -q 1 "$qmp_sock"
)

mapfile -t vcpu_rows < <(
    jq -r '
      select(has("return") and (.return | type == "array"))
      | .return[]
      | [.["cpu-index"], .["thread-id"]] | @tsv
    ' <<< "$qmp_reply" | sort -n -k1,1
)

if (( ${#vcpu_rows[@]} == 0 )); then
    echo "Could not read vCPU thread IDs from QMP." >&2
    echo "Raw QMP reply:" >&2
    echo "$qmp_reply" >&2
    exit 1
fi

if (( ${#host_cpus[@]} < ${#vcpu_rows[@]} )); then
    echo "Not enough host CPUs: got ${#host_cpus[@]}, need ${#vcpu_rows[@]} vCPUs." >&2
    exit 1
fi

printf 'QMP: %s\n' "$qmp_sock"
printf 'Host CPU list: %s\n' "$cpu_list"
printf 'vCPU count: %d\n' "${#vcpu_rows[@]}"

for i in "${!vcpu_rows[@]}"; do
    read -r vcpu tid <<< "${vcpu_rows[$i]}"
    cpu=${host_cpus[$i]}
    printf 'vCPU %s thread %s -> host CPU %s\n' "$vcpu" "$tid" "$cpu"
    if (( dry_run == 0 )); then
        taskset -pc "$cpu" "$tid" >/dev/null
    fi
done

if [[ -n "$emulator_cpus" ]]; then
    qemu_pid=$(pgrep -f "qemu-system.*${qmp_sock}" | head -n 1 || true)
    if [[ -z "$qemu_pid" ]]; then
        echo "Could not find QEMU PID for emulator pinning." >&2
        exit 1
    fi
    printf 'QEMU emulator/main thread %s -> host CPUs %s\n' "$qemu_pid" "$emulator_cpus"
    if (( dry_run == 0 )); then
        taskset -pc "$emulator_cpus" "$qemu_pid" >/dev/null
    fi
fi

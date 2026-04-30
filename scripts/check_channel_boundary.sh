#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./scripts/check_channel_boundary.sh <os_headroom_gb>

Detect DRAM DIMM channel boundaries for the DIMM reservation workflow.

Logic:
  1. Read populated DRAM DIMMs from `dmidecode -t 17`
  2. Group DIMMs by channel
  3. Read kernel memory layout from `dmesg`
  4. Estimate the DRAM hole as
  5. Keep <os_headroom_gb> in the first DIMM for the OS
  6. Reserve only whole DIMMs after the first DIMM

The output is intended for manual review before using `memmap=`.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 1
fi

if [[ ! "$1" =~ ^[0-9]+$ ]]; then
    echo "error: <os_headroom_gb> must be an integer number of GB" >&2
    exit 1
fi

os_headroom_gb=$1
os_headroom_bytes=$(( os_headroom_gb * 1024 * 1024 * 1024 ))

if [[ "$os_headroom_gb" -lt 5 ]]; then
    echo "warning: requested OS headroom is ${os_headroom_gb} GB; keeping at least 5 GB is recommended" >&2
fi

for cmd in awk sort mktemp dmidecode dmesg sudo; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "error: required command '$cmd' is not available" >&2
        exit 1
    fi
done

run_priv() {
    if [[ $EUID -eq 0 ]]; then
        "$@"
    else
        sudo -n "$@"
    fi
}

bytes_to_gb() {
    local bytes=$1
    awk -v bytes="$bytes" 'BEGIN { printf "%.2f", bytes / 1024 / 1024 / 1024 }'
}

bytes_to_gb_int() {
    local bytes=$1
    echo $(( bytes / 1024 / 1024 / 1024 ))
}

bytes_to_hex() {
    local bytes=$1
    printf '0x%x' "$bytes"
}

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

dmi_raw="$tmpdir/dmidecode.txt"
dimm_file="$tmpdir/dimm.tsv"
channel_file="$tmpdir/channel.tsv"

run_priv dmidecode -t 17 >"$dmi_raw"

awk '
    function trim(s) {
        sub(/^[[:space:]]+/, "", s)
        sub(/[[:space:]]+$/, "", s)
        return s
    }
    function to_bytes(size,    n, unit) {
        if (size == "" || size == "No Module Installed") return 0
        split(size, parts, /[[:space:]]+/)
        n = parts[1] + 0
        unit = parts[2]
        if (unit == "GB") return n * 1024 * 1024 * 1024
        if (unit == "MB") return n * 1024 * 1024
        if (unit == "kB") return n * 1024
        return n
    }
    function locator_to_socket(locator,    m) {
        if (match(locator, /^(CPU[0-9]+)_DIMM_[A-Z]+[0-9]+$/, m)) return m[1]
        return "CPU?"
    }
    function locator_to_channel(locator,    m) {
        if (match(locator, /_DIMM_([A-Z]+)[0-9]+$/, m)) return m[1]
        return "?"
    }
    function locator_to_slot(locator,    m) {
        if (match(locator, /([0-9]+)$/, m)) return m[1] + 0
        return 0
    }
    BEGIN { RS=""; FS="\n"; OFS="\t" }
    /Memory Device/ {
        locator = ""
        size = ""
        type = ""
        detail = ""
        for (i = 1; i <= NF; i++) {
            line = trim($i)
            if (line ~ /^Locator:/) locator = trim(substr(line, index(line, ":") + 1))
            else if (line ~ /^Size:/) size = trim(substr(line, index(line, ":") + 1))
            else if (line ~ /^Type:/) type = trim(substr(line, index(line, ":") + 1))
            else if (line ~ /^Type Detail:/) detail = trim(substr(line, index(line, ":") + 1))
        }
        if (locator == "" || size == "" || size == "No Module Installed") next
        if ((type ~ /Logical non-volatile device/) || (detail ~ /Non-Volatile/)) next
        print locator_to_socket(locator), locator_to_channel(locator), locator_to_slot(locator), locator, to_bytes(size), size
    }
' "$dmi_raw" | sort -t $'\t' -k1,1V -k2,2V -k3,3n >"$dimm_file"

if [[ ! -s "$dimm_file" ]]; then
    echo "error: no populated DRAM DIMMs were detected from dmidecode" >&2
    exit 1
fi

awk -F '\t' '
    BEGIN { OFS="\t" }
    {
        key = $1 ":" $2
        socket[key] = $1
        channel[key] = $2
        size[key] += $5
        locators[key] = locators[key] ? locators[key] "," $4 : $4
        raw[key] = raw[key] ? raw[key] "," $6 : $6
        keys[key] = 1
    }
    END {
        for (key in keys) {
            print socket[key], channel[key], size[key], raw[key], locators[key]
        }
    }
' "$dimm_file" | sort -t $'\t' -k1,1V -k2,2V >"$channel_file"

total_dram_bytes=$(awk -F '\t' '{ total += $5 } END { print total + 0 }' "$dimm_file")

dmesg_raw="$(run_priv dmesg)"

boundary_anchor_bytes="$(
    awk '
        function h2d(h) {
            gsub(/^0x/, "", h)
            return strtonum("0x" h)
        }
        /ACPI: SRAT: Node [0-9]+ PXM [0-9]+ \[mem 0x[0-9a-f]+-0x[0-9a-f]+\] non-volatile/ {
            if (match($0, /\[mem (0x[0-9a-f]+)-/, m)) {
                print h2d(m[1])
                exit
            }
        }
    ' <<< "$dmesg_raw"
)"
boundary_anchor_source="srat-non-volatile"

if [[ -z "$boundary_anchor_bytes" ]]; then
    boundary_anchor_bytes="$(
        awk '
            function h2d(h) {
                gsub(/^0x/, "", h)
                return strtonum("0x" h)
            }
            /BIOS-e820: \[mem 0x[0-9a-f]+-0x[0-9a-f]+\] persistent/ {
                if (match($0, /\[mem (0x[0-9a-f]+)-/, m)) {
                    print h2d(m[1])
                    exit
                }
            }
        ' <<< "$dmesg_raw"
    )"
    boundary_anchor_source="e820-persistent"
fi

if [[ -z "$boundary_anchor_bytes" ]]; then
    boundary_anchor_bytes="$(
        awk '
            function h2d(h) {
                gsub(/^0x/, "", h)
                return strtonum("0x" h)
            }
            /BIOS-e820: \[mem 0x[0-9a-f]+-0x[0-9a-f]+\] usable/ {
                if (match($0, /\[mem 0x[0-9a-f]+-(0x[0-9a-f]+)\]/, m)) {
                    end = h2d(m[1]) + 1
                    if (end > max_end) {
                        max_end = end
                    }
                }
            }
            END {
                if (max_end > 0) {
                    print max_end
                }
            }
        ' <<< "$dmesg_raw"
    )"
    boundary_anchor_source="e820-last-usable-end"
fi

if [[ -z "$boundary_anchor_bytes" ]]; then
    echo "error: could not find a DRAM boundary anchor from kernel logs" >&2
    exit 1
fi

hole_bytes=$(( boundary_anchor_bytes - total_dram_bytes ))
if [[ "$hole_bytes" -lt 0 ]]; then
    echo "error: computed hole is negative; please validate BIOS memory layout manually" >&2
    exit 1
fi

channel_count=$(wc -l <"$channel_file")

echo "Detected DRAM channels"
echo "  populated channels : $channel_count"
echo "  total DRAM         : $(bytes_to_gb "$total_dram_bytes") GB"
echo "  hole               : $(bytes_to_gb "$hole_bytes") GB ($(bytes_to_hex "$hole_bytes"))"
echo "  boundary anchor    : $(bytes_to_gb "$boundary_anchor_bytes") GB [$boundary_anchor_source]"
echo "  first DIMM for OS  : ${os_headroom_gb} GB"
echo

printf '%-10s %-22s %-12s %-12s %-12s %-12s %s\n' \
    "Channel" "Locator" "DIMM" "Boundary" "Start" "End" "Suggested Memmap"

cum_bytes=0
idx=0
first_dimm_bytes=0
while IFS=$'\t' read -r socket channel size_bytes raw_sizes locators; do
    if [[ "$idx" -eq 0 ]]; then
        first_dimm_bytes=$size_bytes
    fi
    prev_cum=$cum_bytes
    cum_bytes=$(( cum_bytes + size_bytes ))
    boundary_bytes=$(( hole_bytes + cum_bytes ))
    start_bytes=$(( hole_bytes + prev_cum ))
    end_bytes=$(( boundary_bytes - 1 ))
    display_start="$(bytes_to_gb "$start_bytes")G"

    if [[ "$idx" -eq 0 ]]; then
        display_start="-"
        reserve_size_bytes=$(( size_bytes - os_headroom_bytes ))
        reserve_start_bytes=$(( boundary_bytes - reserve_size_bytes ))
        memmap_arg="memmap=$(bytes_to_gb_int "$reserve_size_bytes")G!$(bytes_to_gb_int "$reserve_start_bytes")G"
    else
        memmap_arg="memmap=$(bytes_to_gb_int "$size_bytes")G!$(bytes_to_gb_int "$start_bytes")G"
    fi

    printf '%-10s %-22s %-12s %-12s %-12s %-12s %s\n' \
        "${socket}_${channel}" \
        "$locators" \
        "$(bytes_to_gb "$size_bytes")G" \
        "$(bytes_to_gb "$boundary_bytes")G" \
        "$display_start" \
        "$(bytes_to_gb "$end_bytes")G" \
        "$memmap_arg"
    idx=$(( idx + 1 ))
done <"$channel_file"

if [[ "$os_headroom_bytes" -ge "$first_dimm_bytes" ]]; then
    echo
    echo "error: requested OS headroom (${os_headroom_gb} GB) must be smaller than the first DIMM size ($(bytes_to_gb "$first_dimm_bytes") GB)" >&2
    exit 1
fi

first_reserved_start_bytes=$(( hole_bytes + os_headroom_bytes ))
first_reserved_size_bytes=$(( first_dimm_bytes - os_headroom_bytes ))

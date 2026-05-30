#!/usr/bin/env bash
# test-rpc.sh — Integration tests for openmediavault-kvm RPC methods.
#
# Usage: sudo ./tests/test-rpc.sh
#
# Exercises KVM plugin RPC methods against the live OMV configuration database
# and libvirt daemon.  Creates a test backup job and a test VM (with a 1 GiB
# disk), exercises read methods against them, then deletes both on exit.
# Read-only methods are also exercised against any pre-existing VMs/pools/networks.
#
# Requirements:
#   - Run as root
#   - OMV with the kvm plugin installed
#   - libvirtd running (virsh must be functional)

set -uo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Colours / counters
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0
declare -a FAILED_TESTS=()

section() { echo -e "\n${CYAN}${BOLD}=== $* ===${NC}" >&2; }
info()    { echo -e "  ${YELLOW}»${NC} $*" >&2; }

_pass() { echo -e "  ${GREEN}PASS${NC}  $1" >&2; ((PASS++)) || true; }
_fail() {
    echo -e "  ${RED}FAIL${NC}  $1" >&2
    [ -n "${2:-}" ] && echo -e "         ${RED}→${NC} $2" >&2
    ((FAIL++)) || true
    FAILED_TESTS+=("$1")
}
_skip() { echo -e "  ${YELLOW}SKIP${NC}  $1${2:+  ($2)}" >&2; ((SKIP++)) || true; }

# ---------------------------------------------------------------------------
# RPC helpers
# ---------------------------------------------------------------------------

# Last successful RPC output — never call assert_rpc inside $() subshells as
# that prevents PASS/FAIL counter updates from propagating.
RPC_OUT=""
BG_OUT=""

assert_rpc() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'} pattern=${5:-}
    local out ec=0
    RPC_OUT=""
    out=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -ne 0 ]; then
        _fail "$desc" "$(echo "$out" | tail -3)"
        return 1
    fi
    if [ -n "$pattern" ] && ! echo "$out" | grep -q "$pattern"; then
        _fail "$desc" "Pattern '$pattern' not found in: ${out:0:300}"
        return 1
    fi
    _pass "$desc"
    RPC_OUT="$out"
    return 0
}

assert_rpc_fails() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'}
    local out ec=0
    out=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -eq 0 ] && ! echo "$out" | grep -qi "exception"; then
        _fail "$desc" "Expected failure but RPC succeeded: ${out:0:200}"
        return 1
    fi
    _pass "$desc"
    return 0
}

# Call a *BgProc method, poll for completion, report result.
# Optional 5th arg: grep pattern that must appear in the task output.
# Task output is available in $BG_OUT after the call.
assert_rpc_bg() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'} pattern=${5:-}
    local filename ec=0
    BG_OUT=""
    filename=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -ne 0 ]; then
        _fail "$desc" "Failed to start bg task: ${filename:0:200}"
        return 1
    fi
    filename=$(echo "$filename" | tr -d '"')

    local timeout=120 elapsed=0 poll_ec=0 poll_out
    while [ $elapsed -lt $timeout ]; do
        poll_out=$(omv-rpc -u admin "Exec" "getOutput" \
            "{\"filename\":\"$filename\",\"pos\":0}" 2>&1)
        poll_ec=$?
        [ $poll_ec -ne 0 ] && break
        echo "$poll_out" | grep -q '"running":true\|"running": true' || break
        sleep 2; ((elapsed += 2)) || true
    done
    if [ $elapsed -ge $timeout ]; then
        _fail "$desc" "Bg task timed out after ${timeout}s"
        return 1
    fi
    if [ $poll_ec -ne 0 ]; then
        local err
        err=$(echo "$poll_out" | python3 -c \
            "import sys,json; d=json.load(sys.stdin); e=d.get('error') or {}; print(e.get('message', str(d))[:300])" \
            2>/dev/null || echo "${poll_out:0:200}")
        _fail "$desc" "$err"
        return 1
    fi
    local content
    content=$(echo "$poll_out" | python3 -c \
        "import sys,json; d=json.load(sys.stdin); print(d.get('output',''))" \
        2>/dev/null || echo "")
    BG_OUT="$content"
    if echo "$content" | grep -q "Exception"; then
        _fail "$desc" "$(echo "$content" | grep "Exception" | head -2)"
        return 1
    fi
    if [ -n "$pattern" ] && ! echo "$content" | grep -q "$pattern"; then
        _fail "$desc" "Pattern '$pattern' not found in output"
        return 1
    fi
    _pass "$desc"
    return 0
}

json_get()  { echo "$1" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('$2',''))" 2>/dev/null; }
json_uuid() { json_get "$1" "uuid"; }

json_list_first() {
    # Extract first element's field from a paginated list response or bare array.
    local json=$1 field=$2
    echo "$json" | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
if rows:
    print(rows[0].get('$field', ''))
" 2>/dev/null || echo ""
}

json_list_count() {
    local json=$1
    echo "$json" | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
print(len(rows) if isinstance(rows, list) else 0)
" 2>/dev/null || echo "0"
}

OMV_NEW_UUID=$(grep -oP 'OMV_CONFIGOBJECT_NEW_UUID="\K[^"]+' /etc/default/openmediavault 2>/dev/null \
    || echo "fa4b1c66-ef79-11e5-87a0-0002b3a176b4")

# ---------------------------------------------------------------------------
# Test object constants
# ---------------------------------------------------------------------------
TEST_JOB_COMMENT="omvtest_kvm_job"
TEST_JOB_PATH="/tmp/omvtest_kvm_backup"
TEST_VM_NAME="omvtest_kvm_vm"
TEST_POOL_NAME="omvtest_kvm_pool"

# ---------------------------------------------------------------------------
# Tracked state — cleared on successful deletion so cleanup skips them
# ---------------------------------------------------------------------------
JOB_UUID=""
VM_CREATED=false   # set to true once setVm succeeds
FIRST_NET=""       # populated in the Networks section
POOL_CREATED=false # set to true once the test pool is defined
TEST_POOL_PATH=""  # filesystem path of the test pool (cleaned up on exit)

# ---------------------------------------------------------------------------
# Pre-cleanup: remove leftover test job from a previous failed run
# ---------------------------------------------------------------------------
pre_cleanup() {
    local uuid

    # leftover test job
    uuid=$(omv-rpc -u admin "Kvm" "getJobList" \
        '{"start":0,"limit":100,"sortfield":"vmname","sortdir":"ASC"}' 2>/dev/null \
        | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
for r in rows:
    if r.get('comment') == '$TEST_JOB_COMMENT':
        print(r['uuid'])
        break
" 2>/dev/null || echo "")
    if [ -n "$uuid" ]; then
        info "Pre-cleanup: removing leftover test job ($uuid)"
        omv-rpc -u admin "Kvm" "deleteJob" "{\"uuid\":\"$uuid\"}" >/dev/null 2>&1 || true
    fi

    # leftover test VM (use undefineplus to also remove its disk)
    if virsh domstate "$TEST_VM_NAME" &>/dev/null 2>&1; then
        info "Pre-cleanup: removing leftover test VM '$TEST_VM_NAME'"
        # force-stop if running
        virsh destroy "$TEST_VM_NAME" >/dev/null 2>&1 || true
        omv-rpc -u admin "Kvm" "doCommand" \
            "{\"name\":\"$TEST_VM_NAME\",\"command\":\"undefineplus\",\"virttype\":\"vm\",\"vncport\":\"0\",\"spiceport\":\"0\",\"hostport\":\"0\",\"hostport2\":\"0\"}" \
            >/dev/null 2>&1 || \
        virsh undefine "$TEST_VM_NAME" --managed-save --snapshots-metadata 2>/dev/null || true
    fi

    # leftover test pool
    if virsh pool-info "$TEST_POOL_NAME" &>/dev/null 2>&1; then
        info "Pre-cleanup: removing leftover test pool '$TEST_POOL_NAME'"
        virsh pool-destroy  "$TEST_POOL_NAME" >/dev/null 2>&1 || true
        virsh pool-undefine "$TEST_POOL_NAME" >/dev/null 2>&1 || true
    fi
}

# ---------------------------------------------------------------------------
# Cleanup trap — always runs on exit
# ---------------------------------------------------------------------------
cleanup() {
    section "Cleanup"
    if [ -n "$JOB_UUID" ]; then
        info "Deleting test job $JOB_UUID"
        omv-rpc -u admin "Kvm" "deleteJob" "{\"uuid\":\"$JOB_UUID\"}" >/dev/null 2>&1 || true
    fi
    if $VM_CREATED; then
        info "Deleting test VM '$TEST_VM_NAME' (undefineplus)"
        virsh destroy "$TEST_VM_NAME" >/dev/null 2>&1 || true
        omv-rpc -u admin "Kvm" "doCommand" \
            "{\"name\":\"$TEST_VM_NAME\",\"command\":\"undefineplus\",\"virttype\":\"vm\",\"vncport\":\"0\",\"spiceport\":\"0\",\"hostport\":\"0\",\"hostport2\":\"0\"}" \
            >/dev/null 2>&1 || \
        virsh undefine "$TEST_VM_NAME" --managed-save --snapshots-metadata 2>/dev/null || true
    fi
    if $POOL_CREATED; then
        info "Deleting test pool '$TEST_POOL_NAME'"
        omv-rpc -u admin "Kvm" "deletePool" "{\"name\":\"$TEST_POOL_NAME\"}" >/dev/null 2>&1 || {
            virsh pool-destroy  "$TEST_POOL_NAME" >/dev/null 2>&1 || true
            virsh pool-undefine "$TEST_POOL_NAME" >/dev/null 2>&1 || true
        }
    fi
    [ -n "$TEST_POOL_PATH" ] && rmdir "$TEST_POOL_PATH" 2>/dev/null || true
    echo "" >&2

    # Deploy pending config changes so the OMV web UI "apply changes" banner
    # does not linger after this test run. Runs detached/async so the script
    # returns promptly; --append-dirty clears the dirty-module markers (the
    # banner) once the deploy completes.
    info "Deploying pending config changes asynchronously (clears web UI banner)"
    nohup omv-salt deploy run --quiet --append-dirty >/dev/null 2>&1 &
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
section "Pre-cleanup"
# ---------------------------------------------------------------------------
pre_cleanup

# ---------------------------------------------------------------------------
section "Pre-flight"
# ---------------------------------------------------------------------------

for cmd in omv-rpc python3 virsh; do
    if command -v "$cmd" &>/dev/null; then
        _pass "command available: $cmd"
    else
        _fail "command available: $cmd" "$cmd not found"
    fi
done

if ! omv-rpc -u admin "Config" "isDirty" '{}' &>/dev/null; then
    echo -e "\n${RED}omv-rpc not functional — aborting.${NC}" >&2
    exit 1
fi
_pass "omv-rpc functional"

if virsh list --all --name &>/dev/null; then
    _pass "libvirtd reachable via virsh"
else
    _fail "libvirtd reachable via virsh" "virsh list failed — is libvirtd running?"
fi

# ===========================================================================
section "Settings"
# ===========================================================================

assert_rpc "getSettings" "Kvm" "getSettings" '{}' '"monitor_enable"'
SETTINGS_DATA="$RPC_OUT"

if [ -n "$SETTINGS_DATA" ]; then
    # Round-trip setSettings without actually changing values
    SETTINGS_UPDATE=$(echo "$SETTINGS_DATA" | python3 -c "
import sys, json
d = json.load(sys.stdin)
# strip read-only fields added by getSettings
for k in ('monitor_db_size',):
    d.pop(k, None)
print(json.dumps(d))
" 2>/dev/null)
    if [ -n "$SETTINGS_UPDATE" ]; then
        assert_rpc "setSettings (round-trip)" "Kvm" "setSettings" "$SETTINGS_UPDATE" '"monitor_enable"'
    else
        _skip "setSettings (round-trip)" "could not build update params"
    fi
else
    _skip "setSettings (round-trip)" "getSettings did not return data"
fi

# ===========================================================================
section "Networks"
# ===========================================================================

assert_rpc "getNetworkList" "Kvm" "getNetworkList" \
    '{"start":0,"limit":25,"sortfield":"netname","sortdir":"ASC"}' '"total"'
NET_COUNT=$(json_list_count "$RPC_OUT")
info "Networks found: $NET_COUNT"

if assert_rpc "enumerateNetworks" "Kvm" "enumerateNetworks" '{}'; then
    FIRST_NET=$(json_list_first "$RPC_OUT" "netname")
    [ -n "$FIRST_NET" ] && info "First network: $FIRST_NET"
fi
assert_rpc "enumerateBridges"  "Kvm" "enumerateBridges"  '{}'

# ===========================================================================
section "Pools"
# ===========================================================================

assert_rpc "getPoolList" "Kvm" "getPoolList" \
    '{"start":0,"limit":25,"sortfield":"name","sortdir":"ASC"}' '"total"'
POOL_COUNT=$(json_list_count "$RPC_OUT")
info "Pools found: $POOL_COUNT"

FIRST_POOL=""
if assert_rpc "enumeratePools" "Kvm" "enumeratePools" '{}'; then
    FIRST_POOL=$(json_list_first "$RPC_OUT" "name")
    [ -n "$FIRST_POOL" ] && info "First pool: $FIRST_POOL"
fi

# ===========================================================================
section "Pool storage-wait drop-in"
# ===========================================================================
# Verifies the libvirtd boot-ordering drop-in tracks storage pools: creating a
# pool whose target path lives on a non-root mount must add a Wants=/After=
# dependency on that mount unit to waitForPools.conf, and deleting it must
# restore the drop-in to its prior state. Each step runs 'omv-salt deploy run
# kvm' to regenerate the drop-in, exactly as applying changes in the web UI does.

DROPIN="/etc/systemd/system/libvirtd.service.d/waitForPools.conf"

# Find a writable mountpoint that is not the root filesystem to host the pool.
TEST_MOUNT=$(findmnt -rno TARGET,FSTYPE | awk '
    $1 != "/" && $2 !~ /^(proc|sysfs|cgroup|cgroup2|devtmpfs|tmpfs|devpts|mqueue|debugfs|tracefs|securityfs|pstore|bpf|configfs|fusectl|autofs|binfmt_misc|nsfs|ramfs|hugetlbfs|efivarfs|overlay|squashfs|nfsd|rpc_pipefs)$/ {print $1}' \
    | while read -r mp; do [ -w "$mp" ] && { echo "$mp"; break; }; done)

if [ -z "$TEST_MOUNT" ]; then
    _skip "setPool (drop-in test)"                      "no writable non-root mount available"
    _skip "deploy + drop-in gains pool mount"           "no writable non-root mount available"
    _skip "deletePool (drop-in test)"                   "no writable non-root mount available"
    _skip "deploy + drop-in restored after pool delete" "no writable non-root mount available"
else
    TEST_POOL_PATH="${TEST_MOUNT%/}/$TEST_POOL_NAME"
    EXPECT_UNIT=$(systemd-escape -p --suffix=mount "$TEST_MOUNT")
    info "Test pool mount: $TEST_MOUNT  ->  unit: $EXPECT_UNIT"

    # Snapshot the current drop-in (may be absent) to compare against later.
    BASELINE_DROPIN=""
    [ -f "$DROPIN" ] && BASELINE_DROPIN=$(cat "$DROPIN")

    POOL_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'name': '$TEST_POOL_NAME',
    'path': '$TEST_POOL_PATH',
    'type': 'dir',
    'hostname': '',
    'zpoolname': '',
    'sourcepath': '',
    'vg': ''
}))
")
    if assert_rpc "setPool (create dir pool, drop-in test)" "Kvm" "setPool" "$POOL_PARAMS"; then
        POOL_CREATED=true

        info "Running 'omv-salt deploy run kvm' (regenerate drop-in with pool) ..."
        if omv-salt deploy run --quiet kvm >/dev/null 2>&1; then
            _pass "omv-salt deploy run kvm (after create)"
        else
            _fail "omv-salt deploy run kvm (after create)" "deploy returned non-zero"
        fi

        if [ -f "$DROPIN" ] && grep -qF "$EXPECT_UNIT" "$DROPIN"; then
            _pass "drop-in gained pool mount unit after create"
        else
            _fail "drop-in gained pool mount unit after create" \
                "expected '$EXPECT_UNIT' in $DROPIN; got: $([ -f "$DROPIN" ] && tr '\n' ' ' < "$DROPIN" || echo '<file absent>')"
        fi
    else
        _skip "deploy + drop-in gains pool mount"           "pool was not created"
        _skip "deletePool (drop-in test)"                   "pool was not created"
        _skip "deploy + drop-in restored after pool delete" "pool was not created"
    fi

    if $POOL_CREATED; then
        if assert_rpc "deletePool (drop-in test)" "Kvm" "deletePool" "{\"name\":\"$TEST_POOL_NAME\"}"; then
            POOL_CREATED=false
        fi

        info "Running 'omv-salt deploy run kvm' (regenerate drop-in without pool) ..."
        if omv-salt deploy run --quiet kvm >/dev/null 2>&1; then
            _pass "omv-salt deploy run kvm (after delete)"
        else
            _fail "omv-salt deploy run kvm (after delete)" "deploy returned non-zero"
        fi

        # Drop-in should return to its pre-test state. (If a pre-existing pool
        # shares the same backing mount, the unit legitimately remains — the
        # baseline already contained it, so the comparison still holds.)
        CURRENT_DROPIN=""
        [ -f "$DROPIN" ] && CURRENT_DROPIN=$(cat "$DROPIN")
        if [ "$CURRENT_DROPIN" = "$BASELINE_DROPIN" ]; then
            _pass "drop-in restored to baseline after pool delete"
        else
            _fail "drop-in restored to baseline after pool delete" \
                "current: $(echo "$CURRENT_DROPIN" | tr '\n' ' ')"
        fi
    fi

    # Remove the directory setPool created on the mount.
    [ -n "$TEST_POOL_PATH" ] && rmdir "$TEST_POOL_PATH" 2>/dev/null || true
fi

# ===========================================================================
section "Volumes"
# ===========================================================================

assert_rpc "getVolumeList (disks)" "Kvm" "getVolumeList" \
    '{"start":0,"limit":50,"sortfield":"name","sortdir":"ASC","optical":false}' '"total"'
VOL_COUNT=$(json_list_count "$RPC_OUT")
info "Disk volumes found: $VOL_COUNT"

assert_rpc "getVolumeList (optical)" "Kvm" "getVolumeList" \
    '{"start":0,"limit":50,"sortfield":"name","sortdir":"ASC","optical":true}' '"total"'

assert_rpc "enumerateVolumes (disks)" "Kvm" "enumerateVolumes" \
    '{"optical":false,"opticalNone":false}'

assert_rpc "enumerateVolumes (optical)" "Kvm" "enumerateVolumes" \
    '{"optical":true,"opticalNone":true}'

# ===========================================================================
section "VMs"
# ===========================================================================

assert_rpc "getVmList" "Kvm" "getVmList" \
    '{"start":0,"limit":25,"sortfield":"vmname","sortdir":"ASC"}' '"total"'
VM_COUNT=$(json_list_count "$RPC_OUT")
info "VMs (including LXC) found: $VM_COUNT"

assert_rpc "getVmNameList" "Kvm" "getVmNameList" '{}'

assert_rpc "getVmNameStateList" "Kvm" "getVmNameStateList" \
    '{"start":0,"limit":25,"sortfield":"vmname","sortdir":"ASC"}'

assert_rpc "getLxcNameStateList" "Kvm" "getLxcNameStateList" \
    '{"start":0,"limit":25,"sortfield":"vmname","sortdir":"ASC"}'

# Discover first available VM for per-VM tests
FIRST_VM=""
FIRST_VM_STATE=""
if assert_rpc "_getVmNameStateList (discover)" "Kvm" "getVmNameStateList" \
    '{"start":0,"limit":100,"sortfield":"vmname","sortdir":"ASC"}' 2>/dev/null; then
    FIRST_VM=$(json_list_first "$RPC_OUT" "vmname")
    FIRST_VM_STATE=$(json_list_first "$RPC_OUT" "state")
fi
[ -n "$FIRST_VM" ] && info "First VM for per-VM tests: $FIRST_VM (state: $FIRST_VM_STATE)"

# ===========================================================================
section "VM lifecycle — create"
# ===========================================================================
#
# Requires at least one active storage pool and one defined network.
# Creates a 1 GiB qcow2 disk in the first available pool and defines a
# minimal VM (no ISO, no VNC).  Per-VM tests run against it next; the VM is
# deleted in the "VM lifecycle — delete" section that follows those tests.

if [ -z "$FIRST_POOL" ] || [ -z "$FIRST_NET" ]; then
    [ -z "$FIRST_POOL" ] && info "Skipping VM create — no storage pool available"
    [ -z "$FIRST_NET"  ] && info "Skipping VM create — no libvirt network available"
    _skip "setVm (create test VM)" "${FIRST_POOL:+no libvirt network available}${FIRST_POOL:-no storage pool available}"
else
    HOST_ARCH=$(dpkg --print-architecture 2>/dev/null || echo "x86_64")
    [ "$HOST_ARCH" = "amd64" ] && HOST_ARCH="x86_64"
    [ "$HOST_ARCH" = "arm64" ] && HOST_ARCH="aarch64"

    # Pick a safe OS variant present on this host
    TEST_OS=$(osinfo-query --fields=short-id os 2>/dev/null \
        | awk 'NR>2 && /^\s*(generic|linux2022|linux2020|debian12|debian11|ubuntu22\.04)\s*$/ {gsub(/ /,""); print; exit}')
    [ -z "$TEST_OS" ] && \
        TEST_OS=$(osinfo-query --fields=short-id os 2>/dev/null \
            | awk 'NR>2 {gsub(/^ +| +$/,"",$0); if ($0!="") {print; exit}}')
    [ -z "$TEST_OS" ] && TEST_OS="generic"
    info "OS variant: $TEST_OS  pool: $FIRST_POOL  network: $FIRST_NET  arch: $HOST_ARCH"

    VM_CREATE=$(python3 -c "
import json
print(json.dumps({
    'lxc': False,
    'vmname': '$TEST_VM_NAME',
    'arch': '$HOST_ARCH',
    'cpu': 'host host-passthrough',
    'otherCpu': '',
    'os': '$TEST_OS',
    'uefi': False,
    'secure': False,
    'vcpu': 1,
    'memory': 256,
    'memoryunit': 'MiB',
    'network': '$FIRST_NET',
    'model': 'virtio',
    'macaddress': '',
    'bridge': '',
    'brmodel': '',
    'voldisk': 'Create new disk',
    'volbus': 'virtio',
    'volformat': 'qcow2',
    'volname': '',
    'volpool': '$FIRST_POOL',
    'volsize': 1,
    'volunit': 'G',
    'voliso': 'none',
    'voliso2': 'none',
    'audio': False,
    'vnc': False,
    'spice': False,
    'tpm': False,
    'notes': 'omvtest vm - safe to delete'
}))
")
    if assert_rpc "setVm (create test VM)" "Kvm" "setVm" "$VM_CREATE"; then
        VM_CREATED=true

        assert_rpc "getVmList (test VM present)" "Kvm" "getVmList" \
            '{"start":0,"limit":100,"sortfield":"vmname","sortdir":"ASC"}' \
            "\"$TEST_VM_NAME\""
        if echo "$RPC_OUT" | grep -q "\"$TEST_VM_NAME\""; then
            _pass "getVmList — '$TEST_VM_NAME' present after create"
        else
            _fail "getVmList — '$TEST_VM_NAME' present after create" \
                "VM not found in list response"
        fi
    fi
fi

# ===========================================================================
section "Per-VM read-only tests"
# ===========================================================================
# Uses the freshly-created test VM when available; falls back to the first
# pre-existing VM discovered earlier.

# Prefer the test VM; it has known properties we can assert precisely.
if $VM_CREATED; then
    TARGET_VM="$TEST_VM_NAME"
    info "Using test VM '$TARGET_VM' for per-VM tests"
elif [ -n "$FIRST_VM" ]; then
    TARGET_VM="$FIRST_VM"
    info "Using pre-existing VM '$TARGET_VM' for per-VM tests"
else
    TARGET_VM=""
fi

if [ -z "$TARGET_VM" ]; then
    _skip "getVmXml"                      "no VM available"
    _skip "getVmDetails"                  "no VM available"
    _skip "getVcpu"                       "no VM available"
    _skip "getNotes"                      "no VM available"
    _skip "enumerateVmNic"                "no VM available"
    _skip "enumerateUsbByVm"              "no VM available"
    _skip "enumeratePciByVm"              "no VM available"
    _skip "enumerateFsPassByVm"           "no VM available"
    _skip "enumerateVolumesByVm (disk)"   "no VM available"
    _skip "enumerateVolumesByVm (cdrom)"  "no VM available"
    _skip "enumerateSnapshots"            "no VM available"
else
    VM_PARAMS=$(python3 -c "import json; print(json.dumps({'vmname':'$TARGET_VM','virttype':'vm'}))")

    assert_rpc "getVmXml" "Kvm" "getVmXml" "$VM_PARAMS" '"vmxml"'
    if echo "$RPC_OUT" | grep -q "$TARGET_VM"; then
        _pass "getVmXml — vmname present in XML"
    else
        _fail "getVmXml — vmname present in XML" "name '$TARGET_VM' not in vmxml"
    fi

    assert_rpc "getVmDetails" "Kvm" "getVmDetails" \
        "$(python3 -c "import json; print(json.dumps({'vmname':'$TARGET_VM'}))")" '"vminfo"'

    assert_rpc "getVcpu" "Kvm" "getVcpu" "$VM_PARAMS" '"vcpu"'
    saved_vcpu=$(json_get "$RPC_OUT" "vcpu")
    if $VM_CREATED; then
        if [ "$saved_vcpu" = "1" ]; then
            _pass "getVcpu — vcpu=1 (matches create params)"
        else
            _fail "getVcpu — vcpu=1" "expected 1, got '$saved_vcpu'"
        fi
    else
        if [ -n "$saved_vcpu" ] && [ "$saved_vcpu" -ge 1 ] 2>/dev/null; then
            _pass "getVcpu — vcpu >= 1 ($saved_vcpu)"
        else
            _fail "getVcpu — vcpu >= 1" "got: '$saved_vcpu'"
        fi
    fi

    assert_rpc "getNotes" "Kvm" "getNotes" "$VM_PARAMS" '"notes"'

    assert_rpc "enumerateVmNic"      "Kvm" "enumerateVmNic"      "$VM_PARAMS"
    assert_rpc "enumerateUsbByVm"    "Kvm" "enumerateUsbByVm"    "$VM_PARAMS"
    assert_rpc "enumeratePciByVm"    "Kvm" "enumeratePciByVm"    "$VM_PARAMS"
    assert_rpc "enumerateFsPassByVm" "Kvm" "enumerateFsPassByVm" "$VM_PARAMS"

    assert_rpc "enumerateVolumesByVm (disk)" "Kvm" "enumerateVolumesByVm" \
        "$(python3 -c "import json; print(json.dumps({'vmname':'$TARGET_VM','optical':False}))")"
    if $VM_CREATED; then
        vol_count=$(json_list_count "$RPC_OUT")
        if [ "$vol_count" -ge 1 ] 2>/dev/null; then
            _pass "enumerateVolumesByVm — $vol_count disk(s) (expected for new VM)"
        else
            _fail "enumerateVolumesByVm — expected at least 1 disk" "got $vol_count"
        fi
    fi

    assert_rpc "enumerateVolumesByVm (cdrom)" "Kvm" "enumerateVolumesByVm" \
        "$(python3 -c "import json; print(json.dumps({'vmname':'$TARGET_VM','optical':True}))")"

    assert_rpc "enumerateSnapshots" "Kvm" "enumerateSnapshots" "$VM_PARAMS"
    SNAP_COUNT=$(json_list_count "$RPC_OUT")
    if $VM_CREATED; then
        if [ "$SNAP_COUNT" = "0" ]; then
            _pass "enumerateSnapshots — 0 snapshots (expected for new VM)"
        else
            _fail "enumerateSnapshots — expected 0 for new VM" "got $SNAP_COUNT"
        fi
    else
        info "Snapshots on $TARGET_VM: $SNAP_COUNT"
    fi
fi

# ===========================================================================
section "Snapshots"
# ===========================================================================

if ! $VM_CREATED; then
    _skip "addSnapshot"          "test VM was not created"
    _skip "enumerateSnapshots (after add)" "test VM was not created"
    _skip "revertSnapshot"       "test VM was not created"
    _skip "deleteSnapshot"       "test VM was not created"
    _skip "enumerateSnapshots (after delete)" "test VM was not created"
    _skip "deleteAllSnapshots"   "test VM was not created"
    _skip "enumerateSnapshots (after deleteAll)" "test VM was not created"
else
    SNAP_PARAMS=$(python3 -c "import json; print(json.dumps({'vmname':'$TEST_VM_NAME','virttype':'vm'}))")

    # Add first snapshot
    assert_rpc "addSnapshot" "Kvm" "addSnapshot" "$SNAP_PARAMS"

    # Verify it appears and capture its name
    SNAP_NAME=""
    assert_rpc "enumerateSnapshots (after add)" "Kvm" "enumerateSnapshots" "$SNAP_PARAMS"
    snap_count=$(json_list_count "$RPC_OUT")
    if [ "$snap_count" -ge 1 ] 2>/dev/null; then
        _pass "enumerateSnapshots — $snap_count snapshot(s) present after add"
        SNAP_NAME=$(json_list_first "$RPC_OUT" "snapname")
        info "Snapshot name: $SNAP_NAME"
    else
        _fail "enumerateSnapshots — expected >= 1 after add" "got $snap_count"
    fi

    if [ -n "$SNAP_NAME" ]; then
        SNAP_OP_PARAMS=$(python3 -c "
import json
print(json.dumps({'vmname':'$TEST_VM_NAME','virttype':'vm','snapname':'$SNAP_NAME'}))
")
        # Revert to the first snapshot
        assert_rpc "revertSnapshot" "Kvm" "revertSnapshot" "$SNAP_OP_PARAMS"

        # Delete the first snapshot by name before creating a second one.
        # virsh auto-names snapshots by Unix timestamp; deleting first ensures
        # the next create gets a fresh (different) timestamp.
        assert_rpc "deleteSnapshot" "Kvm" "deleteSnapshot" "$SNAP_OP_PARAMS"

        assert_rpc "enumerateSnapshots (after deleteSnapshot)" "Kvm" "enumerateSnapshots" "$SNAP_PARAMS"
        snap_count=$(json_list_count "$RPC_OUT")
        if [ "$snap_count" = "0" ]; then
            _pass "enumerateSnapshots — 0 snapshots after deleteSnapshot"
        else
            _fail "enumerateSnapshots — expected 0 after deleteSnapshot" "got $snap_count"
        fi

        # Add a second snapshot (now guaranteed a new timestamp) so
        # deleteAllSnapshots has something to exercise
        assert_rpc "addSnapshot (second)" "Kvm" "addSnapshot" "$SNAP_PARAMS"

        assert_rpc "enumerateSnapshots (after second add)" "Kvm" "enumerateSnapshots" "$SNAP_PARAMS"
        snap_count=$(json_list_count "$RPC_OUT")
        if [ "$snap_count" -ge 1 ] 2>/dev/null; then
            _pass "enumerateSnapshots — $snap_count snapshot(s) present after second add"
        else
            _fail "enumerateSnapshots — expected >= 1 after second add" "got $snap_count"
        fi

        # Delete all remaining snapshots
        assert_rpc "deleteAllSnapshots" "Kvm" "deleteAllSnapshots" "$SNAP_PARAMS"

        assert_rpc "enumerateSnapshots (after deleteAllSnapshots)" "Kvm" "enumerateSnapshots" "$SNAP_PARAMS"
        snap_count=$(json_list_count "$RPC_OUT")
        if [ "$snap_count" = "0" ]; then
            _pass "enumerateSnapshots — 0 snapshots after deleteAllSnapshots"
        else
            _fail "enumerateSnapshots — expected 0 after deleteAllSnapshots" "got $snap_count"
        fi
    else
        _skip "revertSnapshot"    "no snapshot name captured"
        _skip "deleteSnapshot"    "no snapshot name captured"
        _skip "enumerateSnapshots (after deleteSnapshot)" "no snapshot name captured"
        _skip "addSnapshot (second)" "no snapshot name captured"
        _skip "enumerateSnapshots (after second add)" "no snapshot name captured"
        _skip "deleteAllSnapshots" "no snapshot name captured"
        _skip "enumerateSnapshots (after deleteAllSnapshots)" "no snapshot name captured"
    fi
fi

# ===========================================================================
section "VM lifecycle — delete"
# ===========================================================================

if ! $VM_CREATED; then
    _skip "doCommand undefineplus" "test VM was not created"
    _skip "VM absent after undefineplus" "test VM was not created"
else
    # Port params are strings per the RPC schema (rpc.kvm.docommand)
    DEL_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'name': '$TEST_VM_NAME',
    'command': 'undefineplus',
    'virttype': 'vm',
    'vncport': '0',
    'spiceport': '0',
    'hostport': '0',
    'hostport2': '0'
}))
")
    if assert_rpc "doCommand undefineplus (delete test VM+disk)" "Kvm" "doCommand" "$DEL_PARAMS"; then
        VM_CREATED=false
    fi

    if ! virsh domstate "$TEST_VM_NAME" &>/dev/null 2>&1; then
        _pass "VM '$TEST_VM_NAME' absent from virsh after undefineplus"
    else
        _fail "VM '$TEST_VM_NAME' absent from virsh after undefineplus" \
            "virsh still sees the domain"
    fi
fi

# ===========================================================================
section "Host devices"
# ===========================================================================

assert_rpc "enumerateHostDisk"    "Kvm" "enumerateHostDisk"    '{}'
assert_rpc "enumerateHostOptical" "Kvm" "enumerateHostOptical" '{}'
assert_rpc "enumerateUsbByHost"   "Kvm" "enumerateUsbByHost"   '{}'
assert_rpc "enumeratePciByHost"   "Kvm" "enumeratePciByHost"   '{}'

# ===========================================================================
section "Enumeration helpers"
# ===========================================================================

assert_rpc "enumerateArchitectures" "Kvm" "enumerateArchitectures" '{"arch":""}'
ARCH_COUNT=$(json_list_count "$RPC_OUT")
if [ "$ARCH_COUNT" -ge 1 ] 2>/dev/null; then
    _pass "enumerateArchitectures — $ARCH_COUNT architecture(s) returned"
else
    _fail "enumerateArchitectures — expected at least one" "got count=$ARCH_COUNT"
fi

assert_rpc "enumerateCpus" "Kvm" "enumerateCpus" '{"arch":"x86_64"}'
CPU_COUNT=$(json_list_count "$RPC_OUT")
if [ "$CPU_COUNT" -ge 2 ] 2>/dev/null; then
    _pass "enumerateCpus — $CPU_COUNT CPU model(s) returned"
else
    _fail "enumerateCpus — expected at least 2" "got count=$CPU_COUNT"
fi

assert_rpc "enumerateOses" "Kvm" "enumerateOses" '{}'
OS_COUNT=$(json_list_count "$RPC_OUT")
if [ "$OS_COUNT" -ge 1 ] 2>/dev/null; then
    _pass "enumerateOses — $OS_COUNT OS variant(s) returned"
else
    _fail "enumerateOses — expected at least one" "got count=$OS_COUNT"
fi

assert_rpc "enumerateVg" "Kvm" "enumerateVg" '{}'

# ===========================================================================
section "Backup and Restore lists"
# ===========================================================================

assert_rpc "getBackupList"  "Kvm" "getBackupList"  \
    '{"start":0,"limit":50,"sortfield":"vmname","sortdir":"ASC"}'
assert_rpc "getRestoreList" "Kvm" "getRestoreList" '{}'

# ===========================================================================
section "Monitor stats"
# ===========================================================================

assert_rpc "getMonitorStats" "Kvm" "getMonitorStats" '{}'

# ===========================================================================
section "LXC images"
# ===========================================================================

# enumerateImages reads a cache file; skip if no network access is needed
if [ -f /var/cache/openmediavault/lxc_image_cache ]; then
    assert_rpc "enumerateImages (from cache)" "Kvm" "enumerateImages" '{}'
    IMG_COUNT=$(json_list_count "$RPC_OUT")
    if [ "$IMG_COUNT" -ge 1 ] 2>/dev/null; then
        _pass "enumerateImages — $IMG_COUNT image(s) returned"
    else
        _fail "enumerateImages — expected at least one" "got count=$IMG_COUNT"
    fi
else
    _skip "enumerateImages" "no image cache — run forceImageListRefresh first"
fi

# ===========================================================================
section "Jobs — CRUD"
# ===========================================================================

assert_rpc "getJobList" "Kvm" "getJobList" \
    '{"start":0,"limit":25,"sortfield":"vmname","sortdir":"ASC"}' '"total"'

JOB_CREATE=$(python3 -c "
import json
print(json.dumps({
    'uuid': '$OMV_NEW_UUID',
    'enable': False,
    'vmname': '',
    'path': '$TEST_JOB_PATH',
    'poweroff': False,
    'keep': 3,
    'samefmt': False,
    'compression': False,
    'sendemail': False,
    'emailonerror': False,
    'comment': '$TEST_JOB_COMMENT',
    'execution': 'daily',
    'minute': '0',
    'everynminute': False,
    'hour': '2',
    'everynhour': False,
    'month': '*',
    'dayofmonth': '*',
    'everyndayofmonth': False,
    'dayofweek': '*'
}))
")
assert_rpc "setJob (create)" "Kvm" "setJob" "$JOB_CREATE"
JOB_UUID=$(json_uuid "$RPC_OUT")

if [ -z "$JOB_UUID" ]; then
    JOB_UUID=$(omv-rpc -u admin "Kvm" "getJobList" \
        '{"start":0,"limit":100,"sortfield":"vmname","sortdir":"ASC"}' 2>/dev/null \
        | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
for r in rows:
    if r.get('comment') == '$TEST_JOB_COMMENT':
        print(r['uuid'])
        break
" 2>/dev/null || echo "")
    [ -n "$JOB_UUID" ] && info "Recovered job uuid from DB: $JOB_UUID"
fi
info "Created job uuid=$JOB_UUID"

if [ -n "$JOB_UUID" ]; then
    assert_rpc "getJob" "Kvm" "getJob" \
        "{\"uuid\":\"$JOB_UUID\"}" "\"comment\":\"$TEST_JOB_COMMENT\""
    JOB_DATA="$RPC_OUT"

    # Verify stored fields
    saved_keep=$(json_get "$JOB_DATA" "keep")
    if [ "$saved_keep" = "3" ]; then
        _pass "getJob — keep=3 correct"
    else
        _fail "getJob — keep" "expected 3, got '$saved_keep'"
    fi

    saved_path=$(json_get "$JOB_DATA" "path")
    if [ "$saved_path" = "$TEST_JOB_PATH" ]; then
        _pass "getJob — path correct"
    else
        _fail "getJob — path" "expected '$TEST_JOB_PATH', got '$saved_path'"
    fi

    saved_exec=$(json_get "$JOB_DATA" "execution")
    if [ "$saved_exec" = "daily" ]; then
        _pass "getJob — execution=daily correct"
    else
        _fail "getJob — execution" "expected 'daily', got '$saved_exec'"
    fi

    # Update — change keep and compression
    JOB_UPDATE=$(echo "$JOB_DATA" | python3 -c "
import sys, json
d = json.load(sys.stdin)
d['keep'] = 7
d['compression'] = True
print(json.dumps(d))
" 2>/dev/null)
    if [ -n "$JOB_UPDATE" ]; then
        assert_rpc "setJob (update keep+compression)" "Kvm" "setJob" "$JOB_UPDATE"
        saved_keep2=$(json_get "$RPC_OUT" "keep")
        saved_comp=$(json_get "$RPC_OUT" "compression")
        if [ "$saved_keep2" = "7" ]; then
            _pass "setJob (update) — keep=7 saved correctly"
        else
            _fail "setJob (update) — keep round-trip" "expected 7, got '$saved_keep2'"
        fi
        if [ "$saved_comp" = "True" ] || [ "$saved_comp" = "true" ] || [ "$saved_comp" = "1" ]; then
            _pass "setJob (update) — compression=true saved correctly"
        else
            _fail "setJob (update) — compression round-trip" "expected true, got '$saved_comp'"
        fi
    else
        _skip "setJob (update)" "could not build update params"
    fi
else
    _skip "getJob"            "no job uuid"
    _skip "setJob (update)"   "no job uuid"
fi

# ===========================================================================
section "Negative tests"
# ===========================================================================

BAD_UUID='{"uuid":"00000000-0000-0000-0000-000000000000"}'

assert_rpc_fails "getJob — unknown uuid"    "Kvm" "getJob"    "$BAD_UUID"
assert_rpc_fails "deleteJob — unknown uuid" "Kvm" "deleteJob" "$BAD_UUID"

assert_rpc_fails "getVmDetails — empty vmname" "Kvm" "getVmDetails" \
    '{"vmname":""}'
assert_rpc_fails "getVmDetails — nonexistent vm" "Kvm" "getVmDetails" \
    '{"vmname":"omvtest_nonexistent_vm_99"}'

# ===========================================================================
section "Delete test job"
# ===========================================================================

if [ -n "$JOB_UUID" ]; then
    assert_rpc "deleteJob" "Kvm" "deleteJob" \
        "{\"uuid\":\"$JOB_UUID\"}" && JOB_UUID=""

    # Verify the deleted job is no longer in the list
    assert_rpc "getJobList (after delete)" "Kvm" "getJobList" \
        '{"start":0,"limit":100,"sortfield":"vmname","sortdir":"ASC"}' '"total"'
    if echo "$RPC_OUT" | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
found = any(r.get('comment') == '$TEST_JOB_COMMENT' for r in rows)
sys.exit(0 if not found else 1)
" 2>/dev/null; then
        _pass "getJobList — test job absent after delete"
    else
        _fail "getJobList — test job absent after delete" \
            "'$TEST_JOB_COMMENT' still present in list"
    fi
else
    _skip "deleteJob"                          "job was never created"
    _skip "getJobList (after delete)"          "job was never created"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo "" >&2
echo -e "${BOLD}Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}, ${YELLOW}${SKIP} skipped${NC}" >&2
if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo -e "${RED}Failed tests:${NC}" >&2
    for t in "${FAILED_TESTS[@]}"; do
        echo -e "  - $t" >&2
    done
    exit 1
fi
exit 0

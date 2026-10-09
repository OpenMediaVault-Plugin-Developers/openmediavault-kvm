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

# Expect a *BgProc method to fail (task throws or its output has an
# Exception). Optional 5th arg: grep pattern the failure must mention.
# Task output / error text is available in $BG_OUT after the call.
assert_rpc_bg_fails() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'} pattern=${5:-}
    local filename ec=0
    BG_OUT=""
    filename=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -ne 0 ]; then
        # rejected before the bg task even started - still a failure
        BG_OUT="$filename"
    else
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
            BG_OUT=$(echo "$poll_out" | python3 -c \
                "import sys,json; d=json.load(sys.stdin); e=d.get('error') or {}; print(e.get('message', str(d)))" \
                2>/dev/null || echo "$poll_out")
        else
            BG_OUT=$(echo "$poll_out" | python3 -c \
                "import sys,json; d=json.load(sys.stdin); print(d.get('output',''))" \
                2>/dev/null || echo "")
            if ! echo "$BG_OUT" | grep -q "Exception"; then
                _fail "$desc" "Expected failure but task succeeded: ${BG_OUT: -200}"
                return 1
            fi
        fi
    fi
    if [ -n "$pattern" ] && ! echo "$BG_OUT" | grep -q "$pattern"; then
        _fail "$desc" "Failed, but without '$pattern': ${BG_OUT: -300}"
        return 1
    fi
    _pass "$desc"
    return 0
}

# Build JSON params from key=value arguments without any shell quoting of
# the values. "@true"/"@false" give booleans, "@int:N" an integer.
jp() {
    python3 -c '
import json, sys
d = {}
for a in sys.argv[1:]:
    k, v = a.split("=", 1)
    if v == "@true": v = True
    elif v == "@false": v = False
    elif v.startswith("@int:"): v = int(v[5:])
    d[k] = v
print(json.dumps(d))' "$@"
}

# Source files of a VM's file-backed devices, one per line, read from the
# domain XML (domblklist's columns can't be split reliably when a path
# contains spaces). Args: vm, device type (disk/cdrom), optional "live" to
# read the running config instead of the persistent one.
vm_sources() {
    local flag="--inactive"
    [ "${3:-}" = "live" ] && flag=""
    # shellcheck disable=SC2086
    virsh dumpxml $flag "$1" 2>/dev/null | python3 -c "
import sys, xml.etree.ElementTree as ET
root = ET.fromstring(sys.stdin.read())
for d in root.findall('./devices/disk'):
    src = d.find('source')
    if d.get('device') == sys.argv[1] and d.get('type') == 'file' and src is not None and src.get('file'):
        print(src.get('file'))
" "$2" 2>/dev/null
}

# Path of a VM's first file-backed disk (persistent config)
vm_disk_path() {
    vm_sources "$1" disk | head -n1
}

# Attribute of the first <disk device='disk'> in the persistent XML.
# Args: vm, child element (driver/target/source), attribute
vm_disk_attr() {
    virsh dumpxml --inactive "$1" 2>/dev/null | python3 -c "
import sys, xml.etree.ElementTree as ET
root = ET.fromstring(sys.stdin.read())
for d in root.findall('./devices/disk'):
    if d.get('device') == 'disk':
        el = d.find('$2')
        print(el.get('$3', '') if el is not None else '')
        break
" 2>/dev/null
}

# Format reported by qemu-img for an image
img_format() {
    qemu-img info -U "$1" 2>/dev/null | sed -n 's/^file format:[[:space:]]*\([[:alnum:]]*\).*/\1/p' | head -n1
}

# Undefine a VM created by the tests and delete its file-backed disks
destroy_test_vm() {
    local vm=$1 d
    virsh domstate "$vm" &>/dev/null || return 0
    virsh destroy "$vm" >/dev/null 2>&1 || true
    while IFS= read -r d; do
        [ -n "$d" ] && rm -f "$d"
    done < <(vm_sources "$vm" disk)
    virsh undefine "$vm" --managed-save --snapshots-metadata --nvram >/dev/null 2>&1 \
        || virsh undefine "$vm" --managed-save --snapshots-metadata >/dev/null 2>&1 || true
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
TEST_NET_PREFIX="omvtest-kvm-net"   # libvirt networks created during the run
TEST_BACKUP_DIR1="/tmp/omvtest_kvm_backup_run1"  # doBackup happy path
TEST_BACKUP_DIR2="/tmp/omvtest_kvm_backup_run2"  # doBackup forced-cancel
TEST_BACKUP_DIR3="/tmp/omvtest_kvm_backup_run3"  # omv-backup-vm SIGTERM cleanup
TEST_BACKUP_DIR4="/tmp/omvtest_kvm_backup_run4"  # full (snapshot) backup of a running VM
TEST_BACKUP_DIR5="/tmp/omvtest_kvm_backup_run5"  # full backup interrupted by SIGTERM
TEST_RESTORE_DIR="/tmp/omvtest_kvm_restore"      # restore target directory
TEST_RESTORE_VM="omvtest_kvm_restored"           # VM restored from the incremental chain
TEST_RESTORE_VM2="omvtest_kvm_restored2"         # VM restored from the full backup
TEST_CLONE_VM="omvtest_kvm_clone"                # linked clone of the test VM
TEST_ISO_NAME="omvtest_kvm_cdrom.iso"            # fake ISO attached to the test VM
TEST_MOVE_POOL_NAME="omvtest_kvm_movepool"       # doMove destination pool
TEST_MOVE_POOL_PATH="/tmp/omvtest_kvm_movepool"
# Objects with names the shell would mangle if any command left them unquoted
# (spaces, a single quote, $, and a comma in a path). Single-quoted so the
# literal $x is part of the name.
ODD_VM='omvtest kvm vm '\''q'\'' $x'
ODD_RESTORE_VM='omvtest kvm restored '\''q'\'' $x'
ODD_CLONE_VM='omvtest kvm clone '\''q'\'' $x'
ODD_POOL='omvtest_kvm pool $x'
ODD_POOL_PATH='/tmp/omvtest kvm pool $x,1'
ODD_MOVE_POOL='omvtest_kvm movepool $x'
ODD_MOVE_POOL_PATH='/tmp/omvtest kvm movepool $x'
ODD_NET='omvtest-kvm-net odd $x'
ODD_BACKUP_DIR='/tmp/omvtest kvm backup $x'
# Notes with characters the shell / virt-install used to mangle
TEST_VM_NOTES='omvtest vm - safe to delete "quoted", $HOME `id` 100%'
# Dedicated dir pool for the test VM's own disk, under /tmp rather than any
# pre-existing host pool: whatever pool happens to sort first on the host is
# unpredictable (e.g. virt-manager's root-owned "boot-scratch" pool, which a
# non-root qemu process can't open — /root itself is 0700), and /tmp is
# always writable and reachable by the unprivileged qemu user.
TEST_VM_POOL_NAME="omvtest_kvm_vmpool"
TEST_VM_POOL_PATH="/tmp/omvtest_kvm_vmpool"

# ---------------------------------------------------------------------------
# Tracked state — cleared on successful deletion so cleanup skips them
# ---------------------------------------------------------------------------
JOB_UUID=""
VM_CREATED=false   # set to true once setVm succeeds
FIRST_NET=""       # populated in the Networks section
POOL_CREATED=false # set to true once the test pool is defined
TEST_POOL_PATH=""  # filesystem path of the test pool (cleaned up on exit)
VM_POOL_CREATED=false # set to true once the dedicated /tmp VM-disk pool is defined
MOVE_POOL_CREATED=false # set to true once the doMove destination pool is defined
declare -a CREATED_NETS=()  # libvirt networks defined by the run, undefined on exit

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

    # leftover VM-disk test pool (/tmp)
    if virsh pool-info "$TEST_VM_POOL_NAME" &>/dev/null 2>&1; then
        info "Pre-cleanup: removing leftover VM-disk test pool '$TEST_VM_POOL_NAME'"
        virsh pool-destroy  "$TEST_VM_POOL_NAME" >/dev/null 2>&1 || true
        virsh pool-undefine "$TEST_VM_POOL_NAME" >/dev/null 2>&1 || true
    fi
    rm -rf "$TEST_VM_POOL_PATH" 2>/dev/null || true

    # leftover test networks (any net whose name starts with the test prefix)
    while IFS= read -r net; do
        [ -n "$net" ] || continue
        info "Pre-cleanup: removing leftover test network '$net'"
        virsh net-destroy  "$net" >/dev/null 2>&1 || true
        virsh net-undefine "$net" >/dev/null 2>&1 || true
    done < <(virsh net-list --all --name 2>/dev/null | grep "^${TEST_NET_PREFIX}" || true)

    # leftover restored / cloned / odd-name VMs
    for vm in "$TEST_RESTORE_VM" "$TEST_RESTORE_VM2" "$TEST_CLONE_VM" \
              "$ODD_VM" "$ODD_RESTORE_VM" "$ODD_CLONE_VM"; do
        if virsh domstate "$vm" &>/dev/null; then
            info "Pre-cleanup: removing leftover VM '$vm'"
            destroy_test_vm "$vm"
        fi
    done

    # leftover doMove destination pool and odd-name pools
    for pool in "$TEST_MOVE_POOL_NAME" "$ODD_POOL" "$ODD_MOVE_POOL"; do
        if virsh pool-info "$pool" &>/dev/null 2>&1; then
            info "Pre-cleanup: removing leftover test pool '$pool'"
            virsh pool-destroy  "$pool" >/dev/null 2>&1 || true
            virsh pool-undefine "$pool" >/dev/null 2>&1 || true
        fi
    done
    rm -rf "$TEST_MOVE_POOL_PATH" "$ODD_POOL_PATH" "$ODD_MOVE_POOL_PATH" 2>/dev/null || true

    # leftover backup-execution test directories (and their list rows)
    remove_test_backup_dirs
}

remove_test_backup_dirs() {
    local d
    for d in "$TEST_BACKUP_DIR1" "$TEST_BACKUP_DIR2" "$TEST_BACKUP_DIR3" \
             "$TEST_BACKUP_DIR4" "$TEST_BACKUP_DIR5" "$TEST_RESTORE_DIR" "$ODD_BACKUP_DIR"; do
        rm -rf "$d" 2>/dev/null || true
        if [ -f /etc/omv-backup-vm.list ]; then
            # fixed-string field match; the paths contain regex characters
            awk -F, -v p="$d" '$2 != p && $2 != p "/"' /etc/omv-backup-vm.list > /etc/omv-backup-vm.list.omvtest \
                && cat /etc/omv-backup-vm.list.omvtest > /etc/omv-backup-vm.list
            rm -f /etc/omv-backup-vm.list.omvtest
        fi
    done
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
    for vm in "$TEST_RESTORE_VM" "$TEST_RESTORE_VM2" "$TEST_CLONE_VM" \
              "$ODD_CLONE_VM" "$ODD_RESTORE_VM" "$ODD_VM"; do
        if virsh domstate "$vm" &>/dev/null; then
            info "Deleting test VM '$vm'"
            destroy_test_vm "$vm"
        fi
    done
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

    if $VM_POOL_CREATED; then
        info "Deleting VM-disk test pool '$TEST_VM_POOL_NAME'"
        omv-rpc -u admin "Kvm" "deletePool" "{\"name\":\"$TEST_VM_POOL_NAME\"}" >/dev/null 2>&1 || {
            virsh pool-destroy  "$TEST_VM_POOL_NAME" >/dev/null 2>&1 || true
            virsh pool-undefine "$TEST_VM_POOL_NAME" >/dev/null 2>&1 || true
        }
    fi
    rm -rf "$TEST_VM_POOL_PATH" 2>/dev/null || true

    if $MOVE_POOL_CREATED; then
        info "Deleting move test pool '$TEST_MOVE_POOL_NAME'"
        virsh pool-destroy  "$TEST_MOVE_POOL_NAME" >/dev/null 2>&1 || true
        virsh pool-undefine "$TEST_MOVE_POOL_NAME" >/dev/null 2>&1 || true
    fi
    rm -rf "$TEST_MOVE_POOL_PATH" 2>/dev/null || true

    for pool in "$ODD_POOL" "$ODD_MOVE_POOL"; do
        if virsh pool-info "$pool" &>/dev/null 2>&1; then
            info "Deleting test pool '$pool'"
            virsh pool-destroy  "$pool" >/dev/null 2>&1 || true
            virsh pool-undefine "$pool" >/dev/null 2>&1 || true
        fi
    done
    rm -rf "$ODD_POOL_PATH" "$ODD_MOVE_POOL_PATH" 2>/dev/null || true

    remove_test_backup_dirs

    for net in "${CREATED_NETS[@]}"; do
        info "Deleting test network '$net'"
        omv-rpc -u admin "Kvm" "networkCommand" \
            "{\"name\":\"$net\",\"command\":\"delete\"}" >/dev/null 2>&1 || {
            virsh net-destroy  "$net" >/dev/null 2>&1 || true
            virsh net-undefine "$net" >/dev/null 2>&1 || true
        }
    done
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
section "Installed files — static checks"
# ===========================================================================
# Offline checks of the installed plugin files; no RPC or libvirt involved.

for f in omv-backup-vm omv-linked-clone omv-lxc-snapshot omv-sync-vm-backups-list \
         omv-shrink-disk omv-install-homeassistant omv-install-ipfire \
         omv-install-omv8 omv-install-redox omv-install-talos; do
    if [ ! -f "/usr/sbin/$f" ]; then
        _skip "bash -n $f" "not installed"
    elif out=$(bash -n "/usr/sbin/$f" 2>&1); then
        _pass "bash -n $f"
    else
        _fail "bash -n $f" "$out"
    fi
done
for f in omv-kvm-monitor omv-move-vm omv-restore-vm; do
    if [ ! -f "/usr/sbin/$f" ]; then
        _skip "python syntax $f" "not installed"
    elif out=$(python3 -c "import ast, sys; ast.parse(open(sys.argv[1]).read())" "/usr/sbin/$f" 2>&1); then
        _pass "python syntax $f"
    else
        _fail "python syntax $f" "$out"
    fi
done
if out=$(php -l /usr/share/openmediavault/engined/rpc/kvm.inc 2>&1); then
    _pass "php -l rpc/kvm.inc"
else
    _fail "php -l rpc/kvm.inc" "$out"
fi

# the file postrm removes on uninstall must be the one salt writes
SLS="/srv/salt/omv/deploy/kvm/default.sls"
POSTRM="/var/lib/dpkg/info/openmediavault-kvm.postrm"
if [ -f "$SLS" ] && [ -f "$POSTRM" ]; then
    sls_fwd=$(grep -oE '/etc/sysctl[^ ]*ip_forward\.conf' "$SLS" | head -1)
    postrm_fwd=$(sed -n 's/^ipFwdConf="\(.*\)"$/\1/p' "$POSTRM")
    if [ -n "$sls_fwd" ] && [ "$sls_fwd" = "$postrm_fwd" ]; then
        _pass "postrm removes the ip_forward config salt writes ($sls_fwd)"
    else
        _fail "postrm removes the ip_forward config salt writes" "salt: '$sls_fwd', postrm: '$postrm_fwd'"
    fi
else
    _skip "postrm ip_forward path" "salt state or postrm not found"
fi

# render the cron template for a job whose comment contains characters that
# cron (%) and the shell (' " $) treat specially, then parse the line the way
# cron + sh do and check omv-backup-vm would receive the comment unchanged
JOBS_J2="/srv/salt/omv/deploy/kvm/files/jobs.j2"
if [ -f "$JOBS_J2" ] && python3 -c "import jinja2" 2>/dev/null; then
    cron_out=$(python3 - "$JOBS_J2" <<'PY' 2>&1
import sys, subprocess, jinja2
comment = """it's 100% "ok" $HOME `id`"""
env = jinja2.Environment()
env.filters['to_bool'] = lambda v: v in (True, 1, '1', 'true', 'True', 'yes')
job = dict(enable=True, execution='daily', keep=0, incremental=False, fullinterval=0,
           poweroff=False, samefmt=False, compression=False, sendemail=True,
           emailonerror=False, comment=comment, vmname='vm1', path='/srv/backup')
line = env.from_string(open(sys.argv[1]).read()).render(
    pillar={'headers': {'multiline': ''}}, jobs=[job]).strip().splitlines()[-1]
cmd = line.split(' root ', 1)[1].replace('>/dev/null 2>&1', '')
# cron: "\%" -> "%", an unescaped "%" ends the command
out, esc = [], False
for ch in cmd:
    if esc:
        if ch != '%':
            out.append('\\')
        out.append(ch)
        esc = False
    elif ch == '\\':
        esc = True
    elif ch == '%':
        break
    else:
        out.append(ch)
args = subprocess.run(['sh', '-c', 'set -- ' + ''.join(out) + '; for a; do printf "%s\\n" "$a"; done'],
                      capture_output=True, text=True).stdout.splitlines()
got = args[args.index('-C') + 1] if '-C' in args else None
print('OK' if got == comment else 'MISMATCH: %r' % got)
PY
)
    if [ "$cron_out" = "OK" ]; then
        _pass "cron template — job comment reaches omv-backup-vm unchanged"
    else
        _fail "cron template — job comment reaches omv-backup-vm unchanged" "$cron_out"
    fi
else
    _skip "cron template rendering" "template or python3-jinja2 not available"
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
NET_BRIDGE=$(json_list_first "$RPC_OUT" "bridge")
# Docker-managed bridges (br-<12 hex>) must be filtered out.
if echo "$RPC_OUT" | grep -Eq 'br-[0-9a-f]{12}'; then
    _fail "enumerateBridges — Docker bridges hidden" \
        "found a br-<hex> bridge in: ${RPC_OUT:0:200}"
elif ls -d /sys/class/net/br-[0-9a-f]* &>/dev/null 2>&1; then
    _pass "enumerateBridges — Docker bridges hidden (present on host, excluded)"
else
    _skip "enumerateBridges — Docker bridges hidden" "no Docker bridges on host"
fi

# getNetworkXml against a pre-existing network (read-only)
if [ -n "$FIRST_NET" ]; then
    assert_rpc "getNetworkXml" "Kvm" "getNetworkXml" \
        "$(python3 -c "import json; print(json.dumps({'name':'$FIRST_NET'}))")" '"netxml"'
    if echo "$RPC_OUT" | grep -q "<network"; then
        _pass "getNetworkXml — returns network XML"
    else
        _fail "getNetworkXml — returns network XML" "no <network> element in: ${RPC_OUT:0:200}"
    fi
else
    _skip "getNetworkXml" "no pre-existing network"
fi

# ===========================================================================
section "Network lifecycle — create/verify/delete"
# ===========================================================================
# Defines libvirt networks via the RPC methods, inspects the generated XML
# with getNetworkXml, then deletes them with networkCommand. Networks are only
# *defined* (never started), so no host firewall or interface state is touched.

# Helper: assert a network's dumped XML contains a pattern, then it is tracked
# for deletion. Args: desc name pattern
assert_net_xml() {
    local desc=$1 name=$2 pattern=$3
    if assert_rpc "$desc" "Kvm" "getNetworkXml" \
        "$(python3 -c "import json,sys; print(json.dumps({'name':sys.argv[1]}))" "$name")"; then
        if echo "$RPC_OUT" | grep -q "$pattern"; then
            _pass "$desc — XML matches '$pattern'"
        else
            _fail "$desc — XML matches '$pattern'" "got: ${RPC_OUT:0:200}"
        fi
    fi
}

# Helper: delete a network and verify it is gone. Arg: name
delete_net() {
    local name=$1
    if assert_rpc "networkCommand delete ($name)" "Kvm" "networkCommand" \
        "{\"name\":\"$name\",\"command\":\"delete\"}"; then
        # drop from the cleanup list now that it is gone
        local kept=() n
        for n in "${CREATED_NETS[@]}"; do [ "$n" != "$name" ] && kept+=("$n"); done
        CREATED_NETS=("${kept[@]}")
    fi
    if virsh net-info "$name" &>/dev/null 2>&1; then
        _fail "network '$name' absent after delete" "virsh still sees the network"
    else
        _pass "network '$name' absent after delete"
    fi
}

# --- Isolated network (no <forward> element) ---
NET_ISO="${TEST_NET_PREFIX}-iso"
ISO_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'name': '$NET_ISO',
    'forward': 'isolated',
    'macaddress': '52:54:00:6a:1b:01',
    'gatewayip': '10.123.45.1',
    'subnet': '255.255.255.0',
    'dhcp': False
}))
")
if assert_rpc "setNetwork (isolated)" "Kvm" "setNetwork" "$ISO_PARAMS"; then
    CREATED_NETS+=("$NET_ISO")
    assert_rpc "getNetworkXml (isolated)" "Kvm" "getNetworkXml" \
        "$(python3 -c "import json; print(json.dumps({'name':'$NET_ISO'}))")" '"netxml"'
    if echo "$RPC_OUT" | grep -q "<forward"; then
        _fail "isolated network has no <forward> element" "found <forward> in XML"
    else
        _pass "isolated network has no <forward> element"
    fi
    delete_net "$NET_ISO"
fi

# --- NAT network (<forward mode='nat'>) ---
NET_NAT="${TEST_NET_PREFIX}-nat"
NAT_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'name': '$NET_NAT',
    'forward': 'nat',
    'macaddress': '52:54:00:6a:1b:02',
    'gatewayip': '10.123.46.1',
    'subnet': '255.255.255.0',
    'dhcp': True,
    'startaddress': '10.123.46.100',
    'endaddress': '10.123.46.200'
}))
")
if assert_rpc "setNetwork (nat + dhcp)" "Kvm" "setNetwork" "$NAT_PARAMS"; then
    CREATED_NETS+=("$NET_NAT")
    assert_net_xml "getNetworkXml (nat forward)" "$NET_NAT" "mode='nat'"
    assert_net_xml "getNetworkXml (dhcp range)"  "$NET_NAT" "10.123.46.100"
    delete_net "$NET_NAT"
fi

# --- Bridge-backed network (needs an existing host bridge) ---
NET_BR="${TEST_NET_PREFIX}-br"
if [ -n "$NET_BRIDGE" ]; then
    info "Host bridge for bridge-network test: $NET_BRIDGE"
    BR_PARAMS=$(python3 -c "
import json
print(json.dumps({'name': '$NET_BR', 'bridge': '$NET_BRIDGE'}))
")
    if assert_rpc "setBridgeNetwork" "Kvm" "setBridgeNetwork" "$BR_PARAMS"; then
        CREATED_NETS+=("$NET_BR")
        assert_net_xml "getNetworkXml (bridge name)" "$NET_BR" "bridge name='$NET_BRIDGE'"
        delete_net "$NET_BR"
    fi
else
    _skip "setBridgeNetwork" "no host bridge available"
fi

# --- macvtap network (needs a physical NIC) ---
NET_MV="${TEST_NET_PREFIX}-mv"
MV_NIC=""
if assert_rpc "_enumerateDevices (macvtap NIC discovery)" "Network" "enumerateDevices" '{}' 2>/dev/null; then
    MV_NIC=$(json_list_first "$RPC_OUT" "devicename")
fi
if [ -n "$MV_NIC" ]; then
    info "Physical NIC for macvtap test: $MV_NIC (mode: passthrough)"
    MV_PARAMS=$(python3 -c "
import json,sys
print(json.dumps({'name':'$NET_MV','nic':sys.argv[1],'mode':'passthrough'}))
" "$MV_NIC")
    if assert_rpc "setMacvtap (passthrough)" "Kvm" "setMacvtap" "$MV_PARAMS"; then
        CREATED_NETS+=("$NET_MV")
        assert_net_xml "getNetworkXml (macvtap mode)" "$NET_MV" "mode='passthrough'"
        assert_net_xml "getNetworkXml (macvtap dev)"  "$NET_MV" "interface dev='$MV_NIC'"
        delete_net "$NET_MV"
    fi
else
    _skip "setMacvtap" "no physical NIC reported by Network.enumerateDevices"
fi

# Negative: deleting a non-existent network must fail
assert_rpc_fails "networkCommand delete — unknown network" "Kvm" "networkCommand" \
    "{\"name\":\"${TEST_NET_PREFIX}-does-not-exist\",\"command\":\"delete\"}"

# ===========================================================================
section "Pools"
# ===========================================================================

assert_rpc "getPoolList" "Kvm" "getPoolList" \
    '{"start":0,"limit":25,"sortfield":"name","sortdir":"ASC"}' '"total"'
POOL_COUNT=$(json_list_count "$RPC_OUT")
info "Pools found: $POOL_COUNT"

FIRST_POOL=""
if assert_rpc "enumeratePools" "Kvm" "enumeratePools" '{}'; then
    # Skip virt-manager's auto-created "boot-scratch" pool (~/.cache/virt-manager/boot)
    # if present — it's root-owned scratch storage for install media caching, not a
    # real storage pool, and disks created there are unreadable by the qemu user.
    FIRST_POOL=$(echo "$RPC_OUT" | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
for r in (rows or []):
    if r.get('name') != 'boot-scratch':
        print(r.get('name', ''))
        break
" 2>/dev/null || echo "")
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
# Requires at least one defined network. The VM's own disk lives in a
# dedicated dir pool this section defines under /tmp (rather than whatever
# pre-existing host pool sorts first) so it's always owned/reachable by the
# unprivileged qemu process — see TEST_VM_POOL_NAME above. Creates a 1 GiB
# qcow2 disk and defines a minimal VM (no ISO, no VNC). Per-VM tests run
# against it next; the VM is deleted in the "VM lifecycle — delete" section
# that follows those tests.

if [ -z "$FIRST_NET" ]; then
    info "Skipping VM create — no libvirt network available"
    _skip "setVm (create test VM)" "no libvirt network available"
else
    VM_POOL_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'name': '$TEST_VM_POOL_NAME',
    'path': '$TEST_VM_POOL_PATH',
    'type': 'dir',
    'hostname': '',
    'zpoolname': '',
    'sourcepath': '',
    'vg': ''
}))
")
    if ! assert_rpc "setPool (VM-disk test pool)" "Kvm" "setPool" "$VM_POOL_PARAMS"; then
        _skip "setVm (create test VM)" "could not create dedicated VM-disk pool"
    else
        VM_POOL_CREATED=true
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
        info "OS variant: $TEST_OS  pool: $TEST_VM_POOL_NAME  network: $FIRST_NET  arch: $HOST_ARCH"

        VM_CREATE=$(TEST_VM_NOTES="$TEST_VM_NOTES" python3 -c "
import json, os
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
    'volpool': '$TEST_VM_POOL_NAME',
    'volsize': 1,
    'volunit': 'G',
    'voliso': 'none',
    'voliso2': 'none',
    'audio': False,
    'vnc': False,
    'spice': False,
    'tpm': False,
    'notes': os.environ['TEST_VM_NOTES']
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

            # notes are stored verbatim: quotes, commas, $, backticks and %
            # used to be expanded by the shell or mangled by virt-install
            if assert_rpc "getNotes (test VM)" "Kvm" "getNotes" \
                "{\"vmname\":\"$TEST_VM_NAME\",\"virttype\":\"vm\"}"; then
                saved_notes=$(json_get "$RPC_OUT" "notes")
                if [ "$saved_notes" = "$TEST_VM_NOTES" ]; then
                    _pass "setVm — notes with special characters stored verbatim"
                else
                    _fail "setVm — notes with special characters stored verbatim" \
                        "expected '$TEST_VM_NOTES', got '$saved_notes'"
                fi
            fi
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

    # Add a snapshot with an explicit name and verify it is used verbatim
    NAMED_SNAP="testsnap1"
    NAMED_SNAP_PARAMS=$(python3 -c "
import json
print(json.dumps({'vmname':'$TEST_VM_NAME','virttype':'vm','snapname':'$NAMED_SNAP'}))
")
    # internal snapshots live inside the qcow2; deleting one must remove the
    # data there too, not just libvirt's metadata
    SNAP_DISK=$(vm_disk_path "$TEST_VM_NAME")
    _qcow_snap_count() {
        qemu-img snapshot -l -U "$SNAP_DISK" 2>/dev/null | awk 'NR > 2 && NF' | wc -l
    }

    assert_rpc "addSnapshot (named)" "Kvm" "addSnapshot" "$NAMED_SNAP_PARAMS"
    assert_rpc "enumerateSnapshots (after named add)" "Kvm" "enumerateSnapshots" "$SNAP_PARAMS"
    if echo "$RPC_OUT" | grep -q "\"$NAMED_SNAP\""; then
        _pass "addSnapshot — named snapshot '$NAMED_SNAP' created"
    else
        _fail "addSnapshot — expected snapshot named '$NAMED_SNAP'" "$RPC_OUT"
    fi
    if qemu-img snapshot -l -U "$SNAP_DISK" 2>/dev/null | grep -qw "$NAMED_SNAP"; then
        _pass "addSnapshot — '$NAMED_SNAP' stored in the qcow2"
    else
        _fail "addSnapshot — '$NAMED_SNAP' stored in the qcow2" \
            "qemu-img snapshot -l does not list it (external snapshot?)"
    fi
    # Clean up the named snapshot before exercising the auto-named flow
    assert_rpc "deleteSnapshot (named)" "Kvm" "deleteSnapshot" "$NAMED_SNAP_PARAMS"
    if qemu-img snapshot -l -U "$SNAP_DISK" 2>/dev/null | grep -qw "$NAMED_SNAP"; then
        _fail "deleteSnapshot — snapshot data removed from the qcow2" \
            "'$NAMED_SNAP' still listed by qemu-img snapshot -l"
    else
        _pass "deleteSnapshot — snapshot data removed from the qcow2"
    fi

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
        qcow_snaps=$(_qcow_snap_count)
        if [ "$qcow_snaps" = "0" ]; then
            _pass "deleteAllSnapshots — no snapshot data left in the qcow2"
        else
            _fail "deleteAllSnapshots — no snapshot data left in the qcow2" \
                "qemu-img snapshot -l lists $qcow_snaps snapshot(s)"
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
section "Backup execution (doBackup)"
# ===========================================================================
# Actually runs usr/sbin/omv-backup-vm (via the Kvm.doBackup bg RPC method,
# and once directly) against the disposable test VM. Unlike the sections
# above — which only exercise job *definition* CRUD — these tests cover the
# push-mode backup / _wait_backup state machine and the backupActive /
# _cleanup_backup interrupt trap added in omv-backup-vm 0.4.1. Each case
# uses its own fresh backup dir so every run is a brand-new "full" chain
# member (guaranteed non-trivial NBD copy work / a job that stays active
# long enough to observe or interrupt), rather than depending on dirty-bitmap
# timing from a prior incremental run.
#
# Timing-sensitive: these tests poll virsh domjobinfo for an active job
# within a short window and may be flaky on very slow or very fast storage.

if ! $VM_CREATED; then
    _skip "doBackup (incremental, happy path)" "test VM was not created"
    _skip "doBackup (forced cancel)"           "test VM was not created"
    _skip "omv-backup-vm SIGTERM cleanup"      "test VM was not created"
else
    VM_RUNNING=false
    FIRST_NET_STARTED_BY_TEST=false

    # The VM's NIC is attached to $FIRST_NET (a pre-existing network picked
    # in "VM lifecycle — create"). If it's defined but inactive, virsh start
    # fails immediately with a network-not-active error. Start it ourselves
    # if needed and restore it to inactive afterwards.
    if [ -n "$FIRST_NET" ] && ! virsh net-info "$FIRST_NET" 2>/dev/null | grep -qi '^Active:[[:space:]]*yes'; then
        info "Network '$FIRST_NET' is inactive; starting it for the backup tests"
        if virsh net-start "$FIRST_NET" >/dev/null 2>&1; then
            FIRST_NET_STARTED_BY_TEST=true
        else
            info "Could not start network '$FIRST_NET' (continuing; VM start may fail)"
        fi
    fi

    # The test disk is a freshly created, all-zero/sparse 1G qcow2 — a push-mode
    # backup of it can complete in well under a second on fast storage, which
    # makes it unreliable to catch mid-flight for the forced-cancel/SIGTERM
    # tests below. Populate it with real data (while the VM is still stopped,
    # so nothing else has it open) so those backup jobs have non-trivial work
    # and stay active long enough to observe.
    vm_disk_path=$(vm_disk_path "$TEST_VM_NAME")
    if [ -n "$vm_disk_path" ] && command -v qemu-io >/dev/null 2>&1; then
        info "Populating test VM disk with data so backup jobs have real work to copy"
        qemu-io -f qcow2 -c "write -P 0xAB 0 900M" "$vm_disk_path" >/dev/null 2>&1 || \
            info "Could not pre-populate test disk (continuing; forced-cancel/SIGTERM tests may be flaky)"
    fi

    info "Starting test VM '$TEST_VM_NAME' (push-mode/incremental backups require a live VM)"
    start_err=$(virsh start "$TEST_VM_NAME" 2>&1 >/dev/null)
    start_ec=$?
    if [ $start_ec -eq 0 ]; then
        elapsed=0
        while [ $elapsed -lt 30 ]; do
            [ "$(virsh domstate "$TEST_VM_NAME" 2>/dev/null)" = "running" ] && { VM_RUNNING=true; break; }
            sleep 1; ((elapsed++)) || true
        done
    fi

    if ! $VM_RUNNING; then
        if [ $start_ec -ne 0 ]; then
            _fail "test VM running for backup tests" "virsh start failed :: ${start_err}"
        else
            _fail "test VM running for backup tests" "virsh start succeeded but domstate did not reach 'running' within 30s"
        fi
        _skip "doBackup (incremental, happy path)" "could not start test VM"
        _skip "doBackup (forced cancel)"           "could not start test VM"
        _skip "omv-backup-vm SIGTERM cleanup"      "could not start test VM"
    else
        _pass "test VM running for backup tests"

        # Poll virsh domjobinfo until an active backup job appears (Bounded or
        # Unbounded). Polls every 0.1s (rather than a full second) since the
        # job can start and finish within a single second on fast storage.
        # Echoes 1 on success, 0 on timeout. Arg: timeout seconds.
        _wait_job_active() {
            local timeout=${1:-20} tries jt
            tries=$((timeout * 10))
            while [ "$tries" -gt 0 ]; do
                jt=$(virsh domjobinfo "$TEST_VM_NAME" 2>/dev/null \
                    | awk -F: '/Job type/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2 }')
                { [ "$jt" = "Bounded" ] || [ "$jt" = "Unbounded" ]; } && { echo 1; return; }
                sleep 0.1; tries=$((tries-1))
            done
            echo 0
        }

        # ---------------------------------------------------------------
        # 1. Happy path: full RPC round trip through doBackup, first-ever
        #    chain member (a full push-mode backup) — exercises the
        #    rewritten _wait_backup Bounded/Unbounded -> None -> Completed
        #    happy path end-to-end.
        # ---------------------------------------------------------------
        mkdir -p "$TEST_BACKUP_DIR1"
        DOBACKUP1_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'vmname': '$TEST_VM_NAME',
    'path': '$TEST_BACKUP_DIR1',
    'incremental': True,
    'fullinterval': 0,
    'compression': False
}))
")
        if assert_rpc_bg "doBackup (incremental, happy path)" "Kvm" "doBackup" "$DOBACKUP1_PARAMS" "Done"; then
            if echo "$BG_OUT" | grep -q "Backup job completed\."; then
                _pass "doBackup — _wait_backup reported Completed"
            else
                _fail "doBackup — _wait_backup reported Completed" "${BG_OUT: -400}"
            fi
            if find "$TEST_BACKUP_DIR1/$TEST_VM_NAME" -name '*.qcow2' 2>/dev/null | grep -q .; then
                _pass "doBackup — backup chain files present on disk"
            else
                _fail "doBackup — backup chain files present on disk" \
                    "no qcow2 files under $TEST_BACKUP_DIR1/$TEST_VM_NAME"
            fi
        fi

        # ---------------------------------------------------------------
        # 2. Forced-cancel path: start another fresh (full) push-mode
        #    backup and abort it ourselves mid-job via virsh domjobabort —
        #    exercises the new Failed/Cancelled/Canceled branch of
        #    _wait_backup and the "Backup job reported failure!" exit path.
        # ---------------------------------------------------------------
        mkdir -p "$TEST_BACKUP_DIR2"
        DOBACKUP2_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'vmname': '$TEST_VM_NAME',
    'path': '$TEST_BACKUP_DIR2',
    'incremental': True,
    'fullinterval': 0,
    'compression': False
}))
")
        cancel_filename=$(omv-rpc -u admin "Kvm" "doBackup" "$DOBACKUP2_PARAMS" 2>&1)
        cancel_ec=$?
        cancel_filename=$(echo "$cancel_filename" | tr -d '"')
        if [ $cancel_ec -ne 0 ] || [ -z "$cancel_filename" ]; then
            _fail "doBackup (forced cancel)" "failed to start bg task: ${cancel_filename:0:200}"
        elif [ "$(_wait_job_active 20)" != "1" ]; then
            _fail "doBackup (forced cancel)" "backup job never became active within timeout"
        else
            virsh domjobabort "$TEST_VM_NAME" >/dev/null 2>&1

            timeout=60; elapsed=0; poll_ec=0; poll_out=""
            while [ $elapsed -lt $timeout ]; do
                poll_out=$(omv-rpc -u admin "Exec" "getOutput" \
                    "{\"filename\":\"$cancel_filename\",\"pos\":0}" 2>&1)
                poll_ec=$?
                [ $poll_ec -ne 0 ] && break
                echo "$poll_out" | grep -q '"running":true\|"running": true' || break
                sleep 2; ((elapsed += 2)) || true
            done
            if [ $poll_ec -ne 0 ]; then
                cancel_content=$(echo "$poll_out" | python3 -c \
                    "import sys,json; d=json.load(sys.stdin); e=d.get('error') or {}; print(e.get('message', str(d)))" \
                    2>/dev/null || echo "${poll_out:0:400}")
            else
                cancel_content=$(echo "$poll_out" | python3 -c \
                    "import sys,json; d=json.load(sys.stdin); print(d.get('output',''))" \
                    2>/dev/null || echo "")
            fi

            if echo "$cancel_content" | grep -qE "Backup completed with state '(Failed|Cancelled|Canceled)'"; then
                _pass "doBackup (forced cancel) — _wait_backup reported failure state"
            else
                _fail "doBackup (forced cancel) — _wait_backup reported failure state" "${cancel_content: -400}"
            fi
            if echo "$cancel_content" | grep -q "Backup job reported failure!"; then
                _pass "doBackup (forced cancel) — script reported failure"
            else
                _fail "doBackup (forced cancel) — script reported failure" "${cancel_content: -400}"
            fi
            # a failed incremental must fail the task instead of silently
            # falling back to a full backup
            if [ $poll_ec -ne 0 ] || echo "$cancel_content" | grep -q "Exception"; then
                _pass "doBackup (forced cancel) — task reported as failed"
            else
                _fail "doBackup (forced cancel) — task reported as failed" "${cancel_content: -400}"
            fi
            if echo "$cancel_content" | grep -q "Copy disks to backup directory"; then
                _fail "doBackup (forced cancel) — no fallback to full backup" \
                    "full backup path ran after the failed incremental"
            else
                _pass "doBackup (forced cancel) — no fallback to full backup"
            fi
            if find "$TEST_BACKUP_DIR2/$TEST_VM_NAME" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | grep -q .; then
                _fail "doBackup (forced cancel) — partial chain removed" \
                    "$(find "$TEST_BACKUP_DIR2/$TEST_VM_NAME" | head -5 | tr '\n' ' ')"
            else
                _pass "doBackup (forced cancel) — partial chain removed"
            fi
        fi

        # ---------------------------------------------------------------
        # 3. SIGTERM during an active backup: invoke omv-backup-vm directly
        #    (not via RPC) so we control its pid, kill it mid-job, and
        #    verify the backupActive / _cleanup_backup trap logs the abort
        #    and actually clears the job via virsh domjobabort.
        # ---------------------------------------------------------------
        mkdir -p "$TEST_BACKUP_DIR3"
        TERM_LOG="/tmp/omvtest_kvm_sigterm.log"
        rm -f "$TERM_LOG"
        /usr/sbin/omv-backup-vm -v "$TEST_VM_NAME" -d "$TEST_BACKUP_DIR3" -i >"$TERM_LOG" 2>&1 &
        term_pid=$!

        if [ "$(_wait_job_active 20)" != "1" ]; then
            _fail "omv-backup-vm SIGTERM cleanup" "backup job never became active within timeout"
            kill -TERM "$term_pid" >/dev/null 2>&1 || true
            wait "$term_pid" 2>/dev/null || true
        else
            kill -TERM "$term_pid" >/dev/null 2>&1
            wait "$term_pid" 2>/dev/null
            term_ec=$?

            if grep -q "Received SIGTERM; cleaning up\." "$TERM_LOG" \
                && grep -q "Aborting active backup job\." "$TERM_LOG"; then
                _pass "omv-backup-vm SIGTERM cleanup — trap logged abort"
            else
                _fail "omv-backup-vm SIGTERM cleanup — trap logged abort" "$(tail -5 "$TERM_LOG")"
            fi

            # the handler must stop the script (143), not let it carry on
            if [ "$term_ec" -eq 143 ]; then
                _pass "omv-backup-vm SIGTERM cleanup — script exited (143)"
            else
                _fail "omv-backup-vm SIGTERM cleanup — script exited (143)" "exit code $term_ec"
            fi
            if grep -q "Copy disks to backup directory" "$TERM_LOG"; then
                _fail "omv-backup-vm SIGTERM cleanup — no fallback to full backup" "$(tail -5 "$TERM_LOG")"
            else
                _pass "omv-backup-vm SIGTERM cleanup — no fallback to full backup"
            fi
            if find "$TEST_BACKUP_DIR3/$TEST_VM_NAME" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | grep -q .; then
                _fail "omv-backup-vm SIGTERM cleanup — partial chain removed" \
                    "$(find "$TEST_BACKUP_DIR3/$TEST_VM_NAME" | head -5 | tr '\n' ' ')"
            else
                _pass "omv-backup-vm SIGTERM cleanup — partial chain removed"
            fi
            if virsh checkpoint-list "$TEST_VM_NAME" --name 2>/dev/null | grep -q '^omvbak'; then
                _fail "omv-backup-vm SIGTERM cleanup — no checkpoint left behind" \
                    "$(virsh checkpoint-list "$TEST_VM_NAME" --name 2>/dev/null | tr '\n' ' ')"
            else
                _pass "omv-backup-vm SIGTERM cleanup — no checkpoint left behind"
            fi

            job_cleared=false
            elapsed=0
            while [ $elapsed -lt 10 ]; do
                jt_after=$(virsh domjobinfo "$TEST_VM_NAME" 2>/dev/null \
                    | awk -F: '/Job type/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2 }')
                if [ -z "$jt_after" ] || [ "$jt_after" = "None" ]; then
                    job_cleared=true
                    break
                fi
                sleep 1; ((elapsed++)) || true
            done
            if $job_cleared; then
                _pass "omv-backup-vm SIGTERM cleanup — no lingering backup job"
            else
                _fail "omv-backup-vm SIGTERM cleanup — no lingering backup job" \
                    "domjobinfo still reports '$jt_after' 10s after SIGTERM"
            fi
        fi
        rm -f "$TERM_LOG"

        # ---------------------------------------------------------------
        # 4. Full (non-incremental) backup of the running VM: external
        #    snapshot, copy, blockcommit. The VM must end up back on its
        #    original disk with no overlay files left behind.
        # ---------------------------------------------------------------
        ORIG_DISK=$(vm_disk_path "$TEST_VM_NAME")
        ORIG_DIR=$(dirname "$ORIG_DISK")
        _live_disk() {
            vm_sources "$TEST_VM_NAME" disk live | head -n1
        }
        _overlay_count() {
            find "$ORIG_DIR" -maxdepth 1 -name '*backup-snapshot_*' 2>/dev/null | wc -l
        }

        mkdir -p "$TEST_BACKUP_DIR4"
        FULL_LOG="/tmp/omvtest_kvm_full.log"
        if /usr/sbin/omv-backup-vm -v "$TEST_VM_NAME" -d "$TEST_BACKUP_DIR4" >"$FULL_LOG" 2>&1; then
            _pass "omv-backup-vm full backup (running VM)"
        else
            _fail "omv-backup-vm full backup (running VM)" "$(tail -5 "$FULL_LOG")"
        fi
        if find "$TEST_BACKUP_DIR4/$TEST_VM_NAME" -name '*.bak' 2>/dev/null | grep -q .; then
            _pass "full backup — .bak disk image written"
        else
            _fail "full backup — .bak disk image written" "no .bak under $TEST_BACKUP_DIR4"
        fi
        if [ "$(_live_disk)" = "$ORIG_DISK" ]; then
            _pass "full backup — VM pivoted back to its original disk"
        else
            _fail "full backup — VM pivoted back to its original disk" "live disk is '$(_live_disk)'"
        fi
        if [ "$(_overlay_count)" = "0" ]; then
            _pass "full backup — snapshot overlay removed"
        else
            _fail "full backup — snapshot overlay removed" \
                "$(find "$ORIG_DIR" -maxdepth 1 -name '*backup-snapshot_*' | tr '\n' ' ')"
        fi
        rm -f "$FULL_LOG"

        # ---------------------------------------------------------------
        # 5. SIGTERM during a full backup, while the VM runs on the backup
        #    snapshot: the handler must merge the overlay back and exit.
        # ---------------------------------------------------------------
        mkdir -p "$TEST_BACKUP_DIR5"
        FULLTERM_LOG="/tmp/omvtest_kvm_fullterm.log"
        /usr/sbin/omv-backup-vm -v "$TEST_VM_NAME" -d "$TEST_BACKUP_DIR5" >"$FULLTERM_LOG" 2>&1 &
        fullterm_pid=$!
        on_overlay=false
        for _ in $(seq 1 200); do
            case "$(_live_disk)" in *backup-snapshot_*) on_overlay=true; break ;; esac
            kill -0 "$fullterm_pid" 2>/dev/null || break
            sleep 0.1
        done
        if ! $on_overlay; then
            wait "$fullterm_pid" 2>/dev/null
            _skip "omv-backup-vm SIGTERM during full backup" \
                "VM never observed on the backup snapshot (backup too fast?)"
        else
            kill -TERM "$fullterm_pid" >/dev/null 2>&1
            wait "$fullterm_pid" 2>/dev/null
            fullterm_ec=$?
            if [ "$fullterm_ec" -eq 143 ]; then
                _pass "SIGTERM during full backup — script exited (143)"
            else
                _fail "SIGTERM during full backup — script exited (143)" \
                    "exit code $fullterm_ec :: $(tail -3 "$FULLTERM_LOG")"
            fi
            if grep -q "Merging backup snapshot back into the original disks" "$FULLTERM_LOG"; then
                _pass "SIGTERM during full backup — handler merged the snapshot"
            else
                _fail "SIGTERM during full backup — handler merged the snapshot" "$(tail -5 "$FULLTERM_LOG")"
            fi
            if [ "$(_live_disk)" = "$ORIG_DISK" ]; then
                _pass "SIGTERM during full backup — VM back on its original disk"
            else
                _fail "SIGTERM during full backup — VM back on its original disk" "live disk is '$(_live_disk)'"
            fi
            if [ "$(_overlay_count)" = "0" ]; then
                _pass "SIGTERM during full backup — snapshot overlay removed"
            else
                _fail "SIGTERM during full backup — snapshot overlay removed" \
                    "$(find "$ORIG_DIR" -maxdepth 1 -name '*backup-snapshot_*' | tr '\n' ' ')"
            fi
        fi
        rm -f "$FULLTERM_LOG"

        # ---------------------------------------------------------------
        # 6. Monitor memory: VMs created by the plugin have no balloon, so
        #    used memory must come from the RSS fallback, not be reported as
        #    the whole allocation (it used to be a constant 100%).
        # ---------------------------------------------------------------
        if systemctl is-active --quiet omv-kvm-monitor; then
            mon_pct=""
            for _ in $(seq 1 15); do
                mon_pct=$(omv-rpc -u admin "Kvm" "getMonitorStats" '{}' 2>/dev/null | python3 -c "
import sys, json
for r in json.load(sys.stdin):
    if r.get('vm_name') == '$TEST_VM_NAME' and r.get('state_str') == 'running':
        print(r.get('mem_percent', ''))
        break
" 2>/dev/null)
                [ -n "$mon_pct" ] && break
                sleep 2
            done
            if [ -z "$mon_pct" ]; then
                _skip "monitor — memory usage below 100% for idle VM" "test VM not collected yet"
            elif python3 -c "import sys; sys.exit(0 if 0 < float('$mon_pct') < 100 else 1)"; then
                _pass "monitor — memory usage below 100% for idle VM (${mon_pct}%)"
            else
                _fail "monitor — memory usage below 100% for idle VM" "mem_percent=$mon_pct"
            fi
        else
            _skip "monitor — memory usage below 100% for idle VM" "omv-kvm-monitor not running"
        fi

        info "Shutting down test VM '$TEST_VM_NAME'"
        virsh destroy "$TEST_VM_NAME" >/dev/null 2>&1 || true
    fi

    if $FIRST_NET_STARTED_BY_TEST; then
        info "Restoring network '$FIRST_NET' to inactive (was inactive before backup tests)"
        virsh net-destroy "$FIRST_NET" >/dev/null 2>&1 || true
    fi
fi

# ===========================================================================
section "Restore (doRestore)"
# ===========================================================================
# Restores the backups written in "Backup execution" to new VM names. The
# test disk is not written between those backups and now (the VM has no OS),
# so each restored image must match the original disk byte for byte.

_restore_params() {
    # args: date, source dir, new name
    python3 -c "
import json, sys
print(json.dumps({
    'backup': '$TEST_VM_NAME | ' + sys.argv[1] + ' | ' + sys.argv[2],
    'newname': sys.argv[3],
    'newpath': '$TEST_RESTORE_DIR'
}))
" "$1" "$2" "$3"
}

INCR_DATE=""
INCR_MANIFEST=$(ls "$TEST_BACKUP_DIR1/$TEST_VM_NAME"/*/manifest 2>/dev/null | head -1)
[ -n "$INCR_MANIFEST" ] && INCR_DATE=$(awk -F'|' 'NR == 1 { print $2 }' "$INCR_MANIFEST")
FULL_DATE=$(find "$TEST_BACKUP_DIR4/$TEST_VM_NAME" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | head -1)

if ! $VM_CREATED; then
    _skip "doRestore" "test VM was not created"
else
    mkdir -p "$TEST_RESTORE_DIR"
    SRC_DISK=$(vm_disk_path "$TEST_VM_NAME")

    # --- incremental chain restore
    if [ -z "$INCR_DATE" ]; then
        _skip "doRestore (incremental chain)" "no incremental backup from the backup tests"
    elif assert_rpc_bg "doRestore (incremental chain)" "Kvm" "doRestore" \
            "$(_restore_params "$INCR_DATE" "$TEST_BACKUP_DIR1" "$TEST_RESTORE_VM")" "Done"; then
        if virsh dominfo "$TEST_RESTORE_VM" &>/dev/null; then
            _pass "doRestore (incremental) — VM '$TEST_RESTORE_VM' defined"
        else
            _fail "doRestore (incremental) — VM '$TEST_RESTORE_VM' defined" "virsh dominfo failed"
        fi
        restored=$(vm_disk_path "$TEST_RESTORE_VM")
        if [ "$restored" = "$TEST_RESTORE_DIR/${TEST_RESTORE_VM}_001.qcow2" ] && [ -f "$restored" ]; then
            _pass "doRestore (incremental) — disk restored into the restore dir"
        else
            _fail "doRestore (incremental) — disk restored into the restore dir" "disk path '$restored'"
        fi
        if qemu-img compare -U "$SRC_DISK" "$restored" >/dev/null 2>&1; then
            _pass "doRestore (incremental) — restored image matches the original"
        else
            _fail "doRestore (incremental) — restored image matches the original" \
                "$(qemu-img compare -U "$SRC_DISK" "$restored" 2>&1 | tail -1)"
        fi

        # restoring the same name again must be refused before anything is
        # written - the existing VM and its disk stay untouched
        before=$(stat -c %Y "$restored" 2>/dev/null)
        assert_rpc_bg_fails "doRestore — refuses an existing VM name" "Kvm" "doRestore" \
            "$(_restore_params "$INCR_DATE" "$TEST_BACKUP_DIR1" "$TEST_RESTORE_VM")" "already exists"
        if [ "$(stat -c %Y "$restored" 2>/dev/null)" = "$before" ]; then
            _pass "doRestore — existing VM's disk not overwritten"
        else
            _fail "doRestore — existing VM's disk not overwritten" "mtime of $restored changed"
        fi
    fi

    # restoring over the source VM's own name must also be refused
    if [ -n "$INCR_DATE" ]; then
        src_before=$(stat -c %Y "$SRC_DISK" 2>/dev/null)
        assert_rpc_bg_fails "doRestore — refuses the source VM's name" "Kvm" "doRestore" \
            "$(_restore_params "$INCR_DATE" "$TEST_BACKUP_DIR1" "$TEST_VM_NAME")" "already exists"
        if [ "$(stat -c %Y "$SRC_DISK" 2>/dev/null)" = "$src_before" ]; then
            _pass "doRestore — source VM's disk not touched"
        else
            _fail "doRestore — source VM's disk not touched" "mtime of $SRC_DISK changed"
        fi
    fi

    # --- full (self-contained) backup restore
    if [ -z "$FULL_DATE" ]; then
        _skip "doRestore (full backup)" "no full backup from the backup tests"
    elif assert_rpc_bg "doRestore (full backup)" "Kvm" "doRestore" \
            "$(_restore_params "$FULL_DATE" "$TEST_BACKUP_DIR4" "$TEST_RESTORE_VM2")" "Done"; then
        restored2=$(vm_disk_path "$TEST_RESTORE_VM2")
        if [ -n "$restored2" ] && [ -f "$restored2" ]; then
            _pass "doRestore (full) — disk restored"
        else
            _fail "doRestore (full) — disk restored" "disk path '$restored2'"
        fi
        drv=$(vm_disk_attr "$TEST_RESTORE_VM2" driver type)
        fmt=$(img_format "$restored2")
        if [ -n "$fmt" ] && [ "$drv" = "$fmt" ]; then
            _pass "doRestore (full) — driver type matches image format ($fmt)"
        else
            _fail "doRestore (full) — driver type matches image format" "driver '$drv', image '$fmt'"
        fi
        if qemu-img compare -U "$SRC_DISK" "$restored2" >/dev/null 2>&1; then
            _pass "doRestore (full) — restored image matches the original"
        else
            _fail "doRestore (full) — restored image matches the original" \
                "$(qemu-img compare -U "$SRC_DISK" "$restored2" 2>&1 | tail -1)"
        fi
    fi

    destroy_test_vm "$TEST_RESTORE_VM"
    destroy_test_vm "$TEST_RESTORE_VM2"
fi

# ===========================================================================
section "Linked clone (createLinkedClone)"
# ===========================================================================
# Attaches a (fake) ISO to the test VM first: the clone must only overlay
# real disks and leave the cdrom pointing at the same ISO.

TEST_ISO_PATH="$TEST_VM_POOL_PATH/$TEST_ISO_NAME"

if ! $VM_CREATED; then
    _skip "createLinkedClone" "test VM was not created"
else
    truncate -s 1M "$TEST_ISO_PATH"
    assert_rpc "addOptical (fake ISO)" "Kvm" "addOptical" \
        "$(python3 -c "import json; print(json.dumps({'vmname':'$TEST_VM_NAME','voliso':'$TEST_ISO_PATH'}))")"

    SRC_DISK=$(vm_disk_path "$TEST_VM_NAME")
    CLONE_PARAMS=$(python3 -c "import json; print(json.dumps({'clone':'$TEST_CLONE_VM','source':'$TEST_VM_NAME'}))")
    if assert_rpc "createLinkedClone (VM with ISO attached)" "Kvm" "createLinkedClone" "$CLONE_PARAMS"; then
        clone_disk=$(vm_disk_path "$TEST_CLONE_VM")
        expected="$(dirname "$SRC_DISK")/${TEST_CLONE_VM}_$(basename "$SRC_DISK")"
        if [ "$clone_disk" = "$expected" ] && [ -f "$clone_disk" ]; then
            _pass "createLinkedClone — overlay disk created"
        else
            _fail "createLinkedClone — overlay disk created" "clone disk '$clone_disk', expected '$expected'"
        fi
        backing=$(qemu-img info -U "$clone_disk" 2>/dev/null | sed -n 's/^backing file: //p')
        if [ "$backing" = "$SRC_DISK" ]; then
            _pass "createLinkedClone — overlay backed by the source disk"
        else
            _fail "createLinkedClone — overlay backed by the source disk" "backing '$backing'"
        fi
        if [ "$(vm_disk_attr "$TEST_CLONE_VM" driver type)" = "qcow2" ]; then
            _pass "createLinkedClone — overlay driver type is qcow2"
        else
            _fail "createLinkedClone — overlay driver type is qcow2" \
                "got '$(vm_disk_attr "$TEST_CLONE_VM" driver type)'"
        fi
        if vm_sources "$TEST_CLONE_VM" cdrom | grep -qxF "$TEST_ISO_PATH"; then
            _pass "createLinkedClone — cdrom still points at the ISO"
        else
            _fail "createLinkedClone — cdrom still points at the ISO" \
                "$(virsh domblklist "$TEST_CLONE_VM" --details --inactive 2>&1 | tr -s ' ' | tr '\n' ';')"
        fi
        if [ ! -e "$(dirname "$TEST_ISO_PATH")/${TEST_CLONE_VM}_$TEST_ISO_NAME" ]; then
            _pass "createLinkedClone — no overlay created for the ISO"
        else
            _fail "createLinkedClone — no overlay created for the ISO" "overlay of the ISO exists"
        fi
        assert_rpc_fails "createLinkedClone — refuses an existing clone name" "Kvm" "createLinkedClone" "$CLONE_PARAMS"
    fi
    destroy_test_vm "$TEST_CLONE_VM"
    if [ -f "$SRC_DISK" ]; then
        _pass "linked clone removal left the source disk intact"
    else
        _fail "linked clone removal left the source disk intact" "$SRC_DISK is gone"
    fi
fi

# ===========================================================================
section "Move VM (doMove)"
# ===========================================================================
# Moves the test VM's disk to another dir pool and back. The disk's other
# settings (bus, cache) must survive, and an existing file at the destination
# must never be overwritten.

if ! $VM_CREATED; then
    _skip "doMove" "test VM was not created"
else
    MOVE_POOL_PARAMS=$(python3 -c "
import json
print(json.dumps({'name': '$TEST_MOVE_POOL_NAME', 'path': '$TEST_MOVE_POOL_PATH', 'type': 'dir',
                  'hostname': '', 'zpoolname': '', 'sourcepath': '', 'vg': ''}))
")
    if ! assert_rpc "setPool (move destination pool)" "Kvm" "setPool" "$MOVE_POOL_PARAMS"; then
        _skip "doMove" "could not create destination pool"
    else
        MOVE_POOL_CREATED=true
        MV_ORIG=$(vm_disk_path "$TEST_VM_NAME")
        MV_NEW="$TEST_MOVE_POOL_PATH/$(basename "$MV_ORIG")"
        bus_before=$(vm_disk_attr "$TEST_VM_NAME" target bus)
        cache_before=$(vm_disk_attr "$TEST_VM_NAME" driver cache)
        _move_params() {
            python3 -c "import json; print(json.dumps({'vmname':'$TEST_VM_NAME','pool':'$1','poweroff':False}))"
        }

        if assert_rpc_bg "doMove (to destination pool)" "Kvm" "doMove" "$(_move_params "$TEST_MOVE_POOL_NAME")"; then
            if [ "$(vm_disk_path "$TEST_VM_NAME")" = "$MV_NEW" ] && [ -f "$MV_NEW" ] && [ ! -e "$MV_ORIG" ]; then
                _pass "doMove — disk moved and VM points at the new path"
            else
                _fail "doMove — disk moved and VM points at the new path" \
                    "VM disk '$(vm_disk_path "$TEST_VM_NAME")', new exists: $([ -f "$MV_NEW" ] && echo y || echo n), old exists: $([ -e "$MV_ORIG" ] && echo y || echo n)"
            fi
            bus_after=$(vm_disk_attr "$TEST_VM_NAME" target bus)
            cache_after=$(vm_disk_attr "$TEST_VM_NAME" driver cache)
            if [ "$bus_after" = "$bus_before" ] && [ "$cache_after" = "$cache_before" ]; then
                _pass "doMove — disk bus/cache preserved ($bus_after/$cache_after)"
            else
                _fail "doMove — disk bus/cache preserved" \
                    "before '$bus_before/$cache_before', after '$bus_after/$cache_after'"
            fi

            # an unrelated file now sits where the disk would go back to
            echo "omvtest marker" > "$MV_ORIG"
            assert_rpc_bg_fails "doMove — refuses to overwrite an existing file" "Kvm" "doMove" \
                "$(_move_params "$TEST_VM_POOL_NAME")" "already exists"
            if [ "$(cat "$MV_ORIG" 2>/dev/null)" = "omvtest marker" ] && [ "$(vm_disk_path "$TEST_VM_NAME")" = "$MV_NEW" ]; then
                _pass "doMove — existing file and VM left untouched"
            else
                _fail "doMove — existing file and VM left untouched" \
                    "marker: '$(head -c 40 "$MV_ORIG" 2>/dev/null)', VM disk: '$(vm_disk_path "$TEST_VM_NAME")'"
            fi
            rm -f "$MV_ORIG"

            if assert_rpc_bg "doMove (back to original pool)" "Kvm" "doMove" "$(_move_params "$TEST_VM_POOL_NAME")"; then
                if [ "$(vm_disk_path "$TEST_VM_NAME")" = "$MV_ORIG" ] && [ -f "$MV_ORIG" ]; then
                    _pass "doMove — disk moved back"
                else
                    _fail "doMove — disk moved back" "VM disk '$(vm_disk_path "$TEST_VM_NAME")'"
                fi
            fi
        fi

        virsh pool-destroy "$TEST_MOVE_POOL_NAME" >/dev/null 2>&1 || true
        if assert_rpc "deletePool (move destination pool)" "Kvm" "deletePool" "{\"name\":\"$TEST_MOVE_POOL_NAME\"}"; then
            MOVE_POOL_CREATED=false
        fi
        rm -rf "$TEST_MOVE_POOL_PATH" 2>/dev/null || true
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
    DEL_DISK=$(vm_disk_path "$TEST_VM_NAME")
    if assert_rpc "doCommand undefineplus (delete test VM+disk)" "Kvm" "doCommand" "$DEL_PARAMS"; then
        VM_CREATED=false
    fi

    if [ -n "$DEL_DISK" ] && [ ! -e "$DEL_DISK" ]; then
        _pass "undefineplus — VM disk deleted"
    else
        _fail "undefineplus — VM disk deleted" "'$DEL_DISK' still exists"
    fi
    if [ -f "$TEST_ISO_PATH" ]; then
        _pass "undefineplus — attached ISO not deleted"
        rm -f "$TEST_ISO_PATH"
    else
        _fail "undefineplus — attached ISO not deleted" "'$TEST_ISO_PATH' is gone"
    fi

    if ! virsh domstate "$TEST_VM_NAME" &>/dev/null 2>&1; then
        _pass "VM '$TEST_VM_NAME' absent from virsh after undefineplus"
    else
        _fail "VM '$TEST_VM_NAME' absent from virsh after undefineplus" \
            "virsh still sees the domain"
    fi
fi

if $VM_POOL_CREATED; then
    # virt-install auto-starts a pool when a disk it creates lives inside that
    # pool's target directory (our own setPool call above only pool-defines
    # it) — deletePool's pool-undefine fails on an active pool, so stop it
    # first.
    virsh pool-destroy "$TEST_VM_POOL_NAME" >/dev/null 2>&1 || true
    if assert_rpc "deletePool (VM-disk test pool)" "Kvm" "deletePool" "{\"name\":\"$TEST_VM_POOL_NAME\"}"; then
        VM_POOL_CREATED=false
    fi
    rmdir "$TEST_VM_POOL_PATH" 2>/dev/null || true
fi

# ===========================================================================
section "Unusual names (shell quoting)"
# ===========================================================================
# Runs a VM lifecycle where every object name contains characters the shell
# treats specially: spaces, a single quote and $ (plus a comma in the pool
# path). Every RPC used to pass names to commands unquoted, so these broke or
# were expanded by the shell.

ODD_OK=true
if assert_rpc "setNetwork (odd name)" "Kvm" "setNetwork" \
        "$(jp name="$ODD_NET" forward=isolated macaddress=52:54:00:6a:1b:09 \
              gatewayip=10.123.49.1 subnet=255.255.255.0 dhcp=@false)"; then
    CREATED_NETS+=("$ODD_NET")
    if virsh net-info "$ODD_NET" &>/dev/null; then
        _pass "odd network defined under its exact name"
    else
        _fail "odd network defined under its exact name" "virsh net-info '$ODD_NET' failed"
        ODD_OK=false
    fi
else
    ODD_OK=false
fi

if $ODD_OK && assert_rpc "setPool (odd name and path)" "Kvm" "setPool" \
        "$(jp name="$ODD_POOL" path="$ODD_POOL_PATH" type=dir hostname= zpoolname= sourcepath= vg=)"; then
    if [ "$(virsh pool-dumpxml "$ODD_POOL" 2>/dev/null | sed -n 's:.*<path>\(.*\)</path>.*:\1:p')" = "$ODD_POOL_PATH" ]; then
        _pass "odd pool defined with its exact path"
    else
        _fail "odd pool defined with its exact path" "$(virsh pool-dumpxml "$ODD_POOL" 2>&1 | grep path)"
    fi
else
    ODD_OK=false
fi

ODD_VM_CREATED=false
if $ODD_OK; then
    HOST_ARCH=$(dpkg --print-architecture 2>/dev/null || echo "x86_64")
    [ "$HOST_ARCH" = "amd64" ] && HOST_ARCH="x86_64"
    [ "$HOST_ARCH" = "arm64" ] && HOST_ARCH="aarch64"
    if assert_rpc "setVm (odd name, odd pool, odd network)" "Kvm" "setVm" \
            "$(jp lxc=@false vmname="$ODD_VM" arch="$HOST_ARCH" cpu='host host-passthrough' otherCpu= \
                  os=generic uefi=@false secure=@false vcpu=@int:1 memory=@int:256 memoryunit=MiB \
                  network="$ODD_NET" model=virtio macaddress= bridge= brmodel= \
                  voldisk='Create new disk' volbus=virtio volformat=qcow2 volname= volpool="$ODD_POOL" \
                  volsize=@int:1 volunit=G voliso=none voliso2=none audio=@false vnc=@false \
                  spice=@false tpm=@false notes="$TEST_VM_NOTES")"; then
        ODD_VM_CREATED=true
    fi
fi

if ! $ODD_VM_CREATED; then
    _skip "unusual names lifecycle" "odd network/pool/VM could not be created"
else
    ODD_DISK=$(vm_disk_path "$ODD_VM")
    if [ "$(dirname "$ODD_DISK")" = "$ODD_POOL_PATH" ] && [ -f "$ODD_DISK" ]; then
        _pass "setVm (odd) — disk created in the odd pool ($(basename "$ODD_DISK"))"
    else
        _fail "setVm (odd) — disk created in the odd pool" "disk '$ODD_DISK'"
    fi
    if virsh domiflist "$ODD_VM" 2>/dev/null | grep -qF "$ODD_NET"; then
        _pass "setVm (odd) — NIC attached to the odd network"
    else
        _fail "setVm (odd) — NIC attached to the odd network" "$(virsh domiflist "$ODD_VM" 2>&1 | tail -2)"
    fi

    ODD_VMP=$(jp vmname="$ODD_VM" virttype=vm)
    assert_rpc "getVmList (odd VM listed)" "Kvm" "getVmList" \
        '{"start":0,"limit":200,"sortfield":"vmname","sortdir":"ASC"}'
    if echo "$RPC_OUT" | python3 -c "
import sys, json
d = json.load(sys.stdin)
rows = d.get('data', d) if isinstance(d, dict) else d
sys.exit(0 if any(r.get('vmname') == sys.argv[1] for r in rows) else 1)" "$ODD_VM"; then
        _pass "getVmList — odd VM present under its exact name"
    else
        _fail "getVmList — odd VM present under its exact name" "not found"
    fi
    assert_rpc "getVmXml (odd)"     "Kvm" "getVmXml"     "$ODD_VMP" '"vmxml"'
    assert_rpc "getVmDetails (odd)" "Kvm" "getVmDetails" "$(jp vmname="$ODD_VM")" 'State:'
    assert_rpc "enumerateVmNic (odd)" "Kvm" "enumerateVmNic" "$ODD_VMP"
    if echo "$RPC_OUT" | grep -qF "$ODD_NET"; then
        _pass "enumerateVmNic (odd) — source shows the full network name"
    else
        _fail "enumerateVmNic (odd) — source shows the full network name" "${RPC_OUT:0:300}"
    fi
    assert_rpc "enumerateVolumesByVm (odd)" "Kvm" "enumerateVolumesByVm" "$(jp vmname="$ODD_VM" optical=@false)"
    if echo "$RPC_OUT" | grep -qF "$(basename "$ODD_DISK")"; then
        _pass "enumerateVolumesByVm (odd) — full disk path returned"
    else
        _fail "enumerateVolumesByVm (odd) — full disk path returned" "${RPC_OUT:0:300}"
    fi

    # notes
    assert_rpc "setNotes (odd)" "Kvm" "setNotes" "$(jp vmname="$ODD_VM" virttype=vm notes="$TEST_VM_NOTES")"
    assert_rpc "getNotes (odd)" "Kvm" "getNotes" "$ODD_VMP"
    if [ "$(json_get "$RPC_OUT" notes)" = "$TEST_VM_NOTES" ]; then
        _pass "notes on odd VM stored verbatim"
    else
        _fail "notes on odd VM stored verbatim" "got '$(json_get "$RPC_OUT" notes)'"
    fi

    # snapshot with an odd name
    ODD_SNAP='snap '\''one'\'' $x'
    assert_rpc "addSnapshot (odd names)" "Kvm" "addSnapshot" "$(jp vmname="$ODD_VM" virttype=vm snapname="$ODD_SNAP")"
    if virsh snapshot-list "$ODD_VM" --name 2>/dev/null | grep -qxF "$ODD_SNAP"; then
        _pass "addSnapshot (odd) — snapshot created under its exact name"
    else
        _fail "addSnapshot (odd) — snapshot created under its exact name" "$(virsh snapshot-list "$ODD_VM" --name 2>&1)"
    fi
    assert_rpc "deleteSnapshot (odd names)" "Kvm" "deleteSnapshot" "$(jp vmname="$ODD_VM" virttype=vm snapname="$ODD_SNAP")"

    # ISO with spaces and a comma in its path
    ODD_ISO="$ODD_POOL_PATH/install disk,1.iso"
    truncate -s 1M "$ODD_ISO"
    assert_rpc "addOptical (ISO path with space and comma)" "Kvm" "addOptical" "$(jp vmname="$ODD_VM" voliso="$ODD_ISO")"
    if virsh domblklist "$ODD_VM" --details --inactive 2>/dev/null | grep -qF "$ODD_ISO"; then
        _pass "addOptical (odd) — cdrom points at the exact ISO path"
    else
        _fail "addOptical (odd) — cdrom points at the exact ISO path" \
            "$(virsh domblklist "$ODD_VM" --details --inactive 2>&1 | tail -3)"
    fi
    # virt-install/virt-xml double-escape & ' " < > in paths; refuse them
    # instead of silently pointing the VM at the wrong file
    BAD_ISO="$ODD_POOL_PATH/bad'name.iso"
    truncate -s 1M "$BAD_ISO"
    assert_rpc_fails "addOptical — refuses a path virt-xml would corrupt" "Kvm" "addOptical" \
        "$(jp vmname="$ODD_VM" voliso="$BAD_ISO")"
    rm -f "$BAD_ISO"

    # full backup (VM is off) into a directory with an odd name
    mkdir -p "$ODD_BACKUP_DIR"
    ODD_DATE=""
    if assert_rpc_bg "doBackup (odd VM and backup dir)" "Kvm" "doBackup" \
            "$(jp vmname="$ODD_VM" path="$ODD_BACKUP_DIR" compression=@false)" "Done"; then
        ODD_DATE=$(find "$ODD_BACKUP_DIR/$ODD_VM" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | head -1)
        if [ -n "$ODD_DATE" ] && find "$ODD_BACKUP_DIR/$ODD_VM/$ODD_DATE" -name '*.bak' | grep -q .; then
            _pass "doBackup (odd) — backup written under the exact VM name"
        else
            _fail "doBackup (odd) — backup written under the exact VM name" \
                "$(find "$ODD_BACKUP_DIR" 2>&1 | head -5 | tr '\n' ' ')"
        fi
        if awk -F, -v p="$ODD_BACKUP_DIR" -v v="$ODD_VM" -v d="$ODD_DATE" \
                '$2 == p && $3 == v && $4 == d { f = 1 } END { exit !f }' /etc/omv-backup-vm.list; then
            _pass "doBackup (odd) — list row recorded with exact names"
        else
            _fail "doBackup (odd) — list row recorded with exact names" "$(grep -F "$ODD_VM" /etc/omv-backup-vm.list)"
        fi
    fi

    if [ -n "$ODD_DATE" ]; then
        if assert_rpc_bg "doRestore (odd names)" "Kvm" "doRestore" \
                "$(jp backup="$ODD_VM | $ODD_DATE | $ODD_BACKUP_DIR" newname="$ODD_RESTORE_VM" newpath="$ODD_POOL_PATH")" "Done"; then
            r_disk=$(vm_disk_path "$ODD_RESTORE_VM")
            if [ -f "$r_disk" ] && qemu-img compare -U "$ODD_DISK" "$r_disk" >/dev/null 2>&1; then
                _pass "doRestore (odd) — restored VM defined, image matches"
            else
                _fail "doRestore (odd) — restored VM defined, image matches" "restored disk '$r_disk'"
            fi
            # the restored VM keeps its cdrom; its ISO path has a comma
            if virsh domblklist "$ODD_RESTORE_VM" --details --inactive 2>/dev/null | grep -qF "$ODD_ISO"; then
                _pass "doRestore (odd) — cdrom path kept"
            else
                _fail "doRestore (odd) — cdrom path kept" "$(virsh domblklist "$ODD_RESTORE_VM" --details --inactive 2>&1 | tail -3)"
            fi
        fi
        destroy_test_vm "$ODD_RESTORE_VM"

        assert_rpc "deleteBackup (odd names)" "Kvm" "deleteBackup" \
            "$(jp date="$ODD_DATE" path="$ODD_BACKUP_DIR" vmname="$ODD_VM")"
        if [ ! -e "$ODD_BACKUP_DIR/$ODD_VM/$ODD_DATE" ] && ! grep -qF ",$ODD_VM,$ODD_DATE," /etc/omv-backup-vm.list; then
            _pass "deleteBackup (odd) — files and list row removed"
        else
            _fail "deleteBackup (odd) — files and list row removed" \
                "dir exists: $([ -e "$ODD_BACKUP_DIR/$ODD_VM/$ODD_DATE" ] && echo y || echo n)"
        fi
    fi

    # linked clone
    if assert_rpc "createLinkedClone (odd names)" "Kvm" "createLinkedClone" "$(jp clone="$ODD_CLONE_VM" source="$ODD_VM")"; then
        c_disk=$(vm_disk_path "$ODD_CLONE_VM")
        backing=$(qemu-img info -U "$c_disk" 2>/dev/null | sed -n 's/^backing file: //p')
        if [ "$backing" = "$ODD_DISK" ]; then
            _pass "createLinkedClone (odd) — overlay backed by the exact source path"
        else
            _fail "createLinkedClone (odd) — overlay backed by the exact source path" "backing '$backing'"
        fi
    fi
    destroy_test_vm "$ODD_CLONE_VM"

    # move to an odd-named pool and back
    if assert_rpc "setPool (odd move destination)" "Kvm" "setPool" \
            "$(jp name="$ODD_MOVE_POOL" path="$ODD_MOVE_POOL_PATH" type=dir hostname= zpoolname= sourcepath= vg=)"; then
        if assert_rpc_bg "doMove (odd names)" "Kvm" "doMove" "$(jp vmname="$ODD_VM" pool="$ODD_MOVE_POOL" poweroff=@false)"; then
            if [ "$(dirname "$(vm_disk_path "$ODD_VM")")" = "$ODD_MOVE_POOL_PATH" ]; then
                _pass "doMove (odd) — disk now in the odd destination pool"
            else
                _fail "doMove (odd) — disk now in the odd destination pool" "disk '$(vm_disk_path "$ODD_VM")'"
            fi
            assert_rpc_bg "doMove (odd names, back)" "Kvm" "doMove" "$(jp vmname="$ODD_VM" pool="$ODD_POOL" poweroff=@false)"
        fi
        virsh pool-destroy "$ODD_MOVE_POOL" >/dev/null 2>&1 || true
        assert_rpc "deletePool (odd move destination)" "Kvm" "deletePool" "$(jp name="$ODD_MOVE_POOL")"
        rm -rf "$ODD_MOVE_POOL_PATH"
    fi

    # delete VM and disk
    ODD_DISK=$(vm_disk_path "$ODD_VM")
    if assert_rpc "doCommand undefineplus (odd VM)" "Kvm" "doCommand" \
            "$(jp name="$ODD_VM" command=undefineplus virttype=vm vncport=0 spiceport=0 hostport=0 hostport2=0)"; then
        if ! virsh dominfo "$ODD_VM" &>/dev/null && [ ! -e "$ODD_DISK" ]; then
            _pass "undefineplus (odd) — VM and disk removed"
        else
            _fail "undefineplus (odd) — VM and disk removed" "disk '$ODD_DISK'"
        fi
        if [ -f "$ODD_ISO" ]; then
            _pass "undefineplus (odd) — ISO kept"
        else
            _fail "undefineplus (odd) — ISO kept" "'$ODD_ISO' is gone"
        fi
    fi
fi

# tear down the odd pool and network
if virsh pool-info "$ODD_POOL" &>/dev/null; then
    virsh pool-destroy "$ODD_POOL" >/dev/null 2>&1 || true
    assert_rpc "deletePool (odd)" "Kvm" "deletePool" "$(jp name="$ODD_POOL")"
fi
rm -rf "$ODD_POOL_PATH" "$ODD_BACKUP_DIR" 2>/dev/null || true
if virsh net-info "$ODD_NET" &>/dev/null; then
    delete_net "$ODD_NET"
fi

# ===========================================================================
section "Host devices"
# ===========================================================================

assert_rpc "enumerateHostDisk"    "Kvm" "enumerateHostDisk"    '{}'
assert_rpc "enumerateHostOptical" "Kvm" "enumerateHostOptical" '{}'
assert_rpc "enumerateUsbByHost"   "Kvm" "enumerateUsbByHost"   '{}'
# every host USB device must be offered, including ones without a serial
# number (they report "iSerial 0" and used to be dropped)
if command -v lsusb >/dev/null 2>&1; then
    usb_host=$(lsusb 2>/dev/null | grep -c '^Bus')
    usb_rpc=$(json_list_count "$RPC_OUT")
    if [ "$usb_rpc" = "$usb_host" ]; then
        _pass "enumerateUsbByHost — all $usb_host host USB device(s) listed"
    else
        _fail "enumerateUsbByHost — all host USB devices listed" "lsusb: $usb_host, RPC: $usb_rpc"
    fi
fi
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
section "Backup list sync (syncBackupList)"
# ===========================================================================
# Exercises Kvm.syncBackupList (usr/sbin/omv-sync-vm-backups-list), which
# reconciles /etc/omv-backup-vm.list against what's actually on disk. Reuses
# the real chain the "doBackup (incremental, happy path)" test wrote to
# TEST_BACKUP_DIR1 as a known-good entry, and injects one deliberately
# orphaned row (a date that was never backed up) alongside it. syncBackupList
# operates on the whole list file, not just our test rows, so the counting
# assertion below is scoped to rows mentioning TEST_BACKUP_DIR1 rather than
# the full file — the file may carry other, real orphaned rows unrelated to
# this run (e.g. from other VMs) that syncBackupList will also legitimately
# clean up, and that's not something this test should fail on.

BACKUP_LIST="/etc/omv-backup-vm.list"
SYNC_ORPHAN_DATE="1999-01-01_00-00-00"
SYNC_ORPHAN_UUID="00000000-0000-0000-0000-000000000000"

# rows are only ever appended, never reordered, so the last match for our
# test VM/path is the freshest one — earlier matches would be leftovers from
# previous runs of this suite (their backup dir gets wiped by cleanup(), but
# the list-file row is deliberately left for syncBackupList to clean up, so
# it can't have been removed by our own prior runs)
real_entry=""
if [ -f "$BACKUP_LIST" ]; then
    real_entry=$(grep ",${TEST_VM_NAME}," "$BACKUP_LIST" | grep -F "$TEST_BACKUP_DIR1" | tail -1)
fi

if [ -z "$real_entry" ]; then
    _skip "syncBackupList removes orphaned entry"    "no real backup entry from the happy-path test to compare against"
    _skip "syncBackupList keeps real entry"          "no real backup entry from the happy-path test to compare against"
    _skip "syncBackupList removes exactly one entry" "no real backup entry from the happy-path test to compare against"
    _skip "syncBackupList backs up list file"        "no real backup entry from the happy-path test to compare against"
else
    # purge any TEST_BACKUP_DIR1 rows left behind by earlier runs of this
    # suite (besides the current real one) so the count assertion below
    # isn't thrown off by this test's own accumulated history
    stale_tmp=$(mktemp)
    grep -vF "$TEST_BACKUP_DIR1" "$BACKUP_LIST" > "$stale_tmp" || true
    echo "$real_entry" >> "$stale_tmp"
    mv "$stale_tmp" "$BACKUP_LIST"

    echo "${SYNC_ORPHAN_UUID},${TEST_BACKUP_DIR1},${TEST_VM_NAME},${SYNC_ORPHAN_DATE},0" >> "$BACKUP_LIST"
    pre_count=$(grep -cF "$TEST_BACKUP_DIR1" "$BACKUP_LIST")
    rm -f "${BACKUP_LIST}.bak"

    # syncBackupList runs via execBgProc (like doBackup/doMove) so the RPC
    # returns a bg task filename immediately; poll it to completion before
    # checking the list file, same technique as the "doBackup (forced
    # cancel)" polling above.
    sync_filename=$(omv-rpc -u admin "Kvm" "syncBackupList" '{}' 2>&1)
    sync_start_ec=$?
    sync_filename=$(echo "$sync_filename" | tr -d '"')

    sync_failed=false
    sync_err=""
    if [ $sync_start_ec -ne 0 ] || [ -z "$sync_filename" ]; then
        sync_failed=true
        sync_err="failed to start bg task: ${sync_filename:0:300}"
    else
        timeout=60; elapsed=0; poll_ec=0; poll_out=""
        while [ $elapsed -lt $timeout ]; do
            poll_out=$(omv-rpc -u admin "Exec" "getOutput" \
                "{\"filename\":\"$sync_filename\",\"pos\":0}" 2>&1)
            poll_ec=$?
            [ $poll_ec -ne 0 ] && break
            echo "$poll_out" | grep -q '"running":true\|"running": true' || break
            sleep 1; ((elapsed += 1)) || true
        done
        if [ $elapsed -ge $timeout ]; then
            sync_failed=true
            sync_err="bg task timed out after ${timeout}s"
        elif [ $poll_ec -ne 0 ]; then
            sync_failed=true
            sync_err=$(echo "$poll_out" | python3 -c \
                "import sys,json; d=json.load(sys.stdin); e=d.get('error') or {}; print(e.get('message', str(d))[:300])" \
                2>/dev/null || echo "${poll_out:0:300}")
        else
            sync_content=$(echo "$poll_out" | python3 -c \
                "import sys,json; d=json.load(sys.stdin); print(d.get('output',''))" \
                2>/dev/null || echo "")
            if echo "$sync_content" | grep -q "Exception"; then
                sync_failed=true
                sync_err=$(echo "$sync_content" | grep "Exception" | head -2)
            fi
        fi
    fi

    if $sync_failed; then
        _fail "syncBackupList removes orphaned entry"    "$sync_err"
        _fail "syncBackupList keeps real entry"          "$sync_err"
        _fail "syncBackupList removes exactly one entry" "$sync_err"
        _fail "syncBackupList backs up list file"        "$sync_err"
        # sync never ran (or never finished), so the injected row is still ours to clean up
        sed -i "\|${SYNC_ORPHAN_DATE}|d" "$BACKUP_LIST"
    else
        if grep -qF "$SYNC_ORPHAN_DATE" "$BACKUP_LIST"; then
            _fail "syncBackupList removes orphaned entry" "orphan row for $SYNC_ORPHAN_DATE still present"
        else
            _pass "syncBackupList removes orphaned entry"
        fi

        if grep -qF "$real_entry" "$BACKUP_LIST"; then
            _pass "syncBackupList keeps real entry"
        else
            _fail "syncBackupList keeps real entry" "real entry disappeared: $real_entry"
        fi

        post_count=$(grep -cF "$TEST_BACKUP_DIR1" "$BACKUP_LIST")
        if [ "$post_count" -eq $((pre_count - 1)) ]; then
            _pass "syncBackupList removes exactly one entry"
        else
            _fail "syncBackupList removes exactly one entry" \
                "expected $((pre_count - 1)) TEST_BACKUP_DIR1 rows after sync, got $post_count"
        fi

        if [ -f "${BACKUP_LIST}.bak" ] && grep -qF "$SYNC_ORPHAN_DATE" "${BACKUP_LIST}.bak"; then
            _pass "syncBackupList backs up list file"
        else
            _fail "syncBackupList backs up list file" \
                "${BACKUP_LIST}.bak missing, or doesn't contain pre-sync state"
        fi
    fi
fi

# ===========================================================================
section "Monitor stats"
# ===========================================================================

assert_rpc "getMonitorStats" "Kvm" "getMonitorStats" '{}'
if systemctl is-active --quiet omv-kvm-monitor; then
    assert_rpc "deleteMonitorVm — nonexistent vm" "Kvm" "deleteMonitorVm" \
        '{"name":"omv-test-no-such-vm"}'
fi

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
    'incremental': True,
    'fullinterval': 5,
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

    saved_incr=$(json_get "$JOB_DATA" "incremental")
    if [ "$saved_incr" = "True" ] || [ "$saved_incr" = "true" ] || [ "$saved_incr" = "1" ]; then
        _pass "getJob — incremental=true saved correctly"
    else
        _fail "getJob — incremental round-trip" "expected true, got '$saved_incr'"
    fi

    saved_finterval=$(json_get "$JOB_DATA" "fullinterval")
    if [ "$saved_finterval" = "5" ]; then
        _pass "getJob — fullinterval=5 saved correctly"
    else
        _fail "getJob — fullinterval round-trip" "expected 5, got '$saved_finterval'"
    fi

    # Update — change keep, compression, and the incremental fields
    JOB_UPDATE=$(echo "$JOB_DATA" | python3 -c "
import sys, json
d = json.load(sys.stdin)
d['keep'] = 7
d['compression'] = True
d['incremental'] = False
d['fullinterval'] = 10
print(json.dumps(d))
" 2>/dev/null)
    if [ -n "$JOB_UPDATE" ]; then
        assert_rpc "setJob (update keep+compression+incremental)" "Kvm" "setJob" "$JOB_UPDATE"
        saved_keep2=$(json_get "$RPC_OUT" "keep")
        saved_comp=$(json_get "$RPC_OUT" "compression")
        saved_incr2=$(json_get "$RPC_OUT" "incremental")
        saved_finterval2=$(json_get "$RPC_OUT" "fullinterval")
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
        if [ "$saved_incr2" = "False" ] || [ "$saved_incr2" = "false" ] || [ "$saved_incr2" = "0" ]; then
            _pass "setJob (update) — incremental=false saved correctly"
        else
            _fail "setJob (update) — incremental round-trip" "expected false, got '$saved_incr2'"
        fi
        if [ "$saved_finterval2" = "10" ]; then
            _pass "setJob (update) — fullinterval=10 saved correctly"
        else
            _fail "setJob (update) — fullinterval round-trip" "expected 10, got '$saved_finterval2'"
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

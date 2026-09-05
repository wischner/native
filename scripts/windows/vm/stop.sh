#!/usr/bin/env bash
# A postDebugTask also covers the adapter forcibly terminating its pipe.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
exec 9>"${vm_workspace}/build/windows-vm-debug.lock"
flock -w 15 9 || { echo 'A Windows VM debug session is still active.' >&2; exit 1; }
vm_address
vm_powershell "& '${vm_remote_dir}/session.ps1' -Action Stop"

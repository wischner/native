#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

ssh_exec_tty \
    "set -euo pipefail; \
if ! /usr/sbin/DevToolsSecurity -status 2>&1 | grep -q enabled; then \
  echo 'Remote LLDB permission is disabled.' >&2; \
  echo 'Run once on the Mac: sudo /usr/sbin/DevToolsSecurity -enable' >&2; \
  exit 1; \
fi; \
cd '${REMOTE_BUILD_DIR}'; \
exec lldb -o \"process launch --stop-at-entry\" -o \"command script import '${REMOTE_PROJECT_DIR}/scripts/macos/remote/lldb_autoquit.py'\" \
    -o continue '${REMOTE_VISION_EXE}'"

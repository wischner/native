#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

: "${MAC_REMOTE_HOST:=leia}"
: "${MAC_REMOTE_USER:=tomaz}"
: "${MAC_REMOTE_BASE:=/Users/${MAC_REMOTE_USER}/Projects}"

PROJECT_NAME="${MAC_REMOTE_PROJECT:-$(basename "${WORKSPACE_DIR}")}"
REMOTE="${MAC_REMOTE_USER}@${MAC_REMOTE_HOST}"
REMOTE_PROJECT_DIR="${MAC_REMOTE_BASE}/${PROJECT_NAME}"
REMOTE_BUILD_DIR="${REMOTE_PROJECT_DIR}/build/macos-debug"
REMOTE_PAINTER_EXE="${REMOTE_BUILD_DIR}/examples/02_painter_example/painter-example"
REMOTE_MENU_EXE="${REMOTE_BUILD_DIR}/examples/03_menu_example/menu-example"

ssh_exec() {
    ssh "${REMOTE}" "$@"
}

ssh_exec_tty() {
    ssh -tt "${REMOTE}" "$@"
}

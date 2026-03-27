#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

kind="${1:-painter}"
case "${kind}" in
    painter)
        exe="${REMOTE_PAINTER_EXE}"
        ;;
    menu)
        exe="${REMOTE_MENU_EXE}"
        ;;
    *)
        echo "Usage: $0 [painter|menu]" >&2
        exit 2
        ;;
esac

ssh_exec_tty "set -euo pipefail; cd '${REMOTE_BUILD_DIR}'; exec lldb '${exe}'"

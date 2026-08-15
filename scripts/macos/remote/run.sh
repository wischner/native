#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

ssh_exec_tty \
    "set -euo pipefail; exec open -W '${REMOTE_VISION_BUNDLE}'"

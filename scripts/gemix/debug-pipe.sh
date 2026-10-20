#!/usr/bin/env bash
set -euo pipefail

# Run Rasta, GEM and GDB together in the published build image.
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
exec "${SCRIPT_DIR}/../linux/docker-debug.sh" \
    --session "${SCRIPT_DIR}/rasta-session.sh" "$@"

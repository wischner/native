#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

rsync -az --delete \
    --exclude '.git/' \
    --exclude 'build/' \
    --exclude 'out/' \
    --exclude '.DS_Store' \
    --exclude '.vscode/.ropeproject/' \
    -e "ssh -T -o RemoteCommand=none -o RequestTTY=no" \
    "${WORKSPACE_DIR}/" "${REMOTE}:${REMOTE_PROJECT_DIR}/"

echo "Synced to ${REMOTE_PROJECT_DIR} on ${REMOTE}."

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

"${SCRIPT_DIR}/sync.sh"

ssh_exec "set -euo pipefail; \
export PATH=/opt/homebrew/bin:/usr/local/bin:/Applications/CMake.app/Contents/bin:\$PATH; \
cmake_bin=\$(command -v cmake || true); \
if [ -z \"\$cmake_bin\" ]; then \
  echo 'cmake not found on remote host. Install it with: brew install cmake' >&2; \
  exit 127; \
fi; \
mkdir -p '${REMOTE_PROJECT_DIR}'; \
cd '${REMOTE_PROJECT_DIR}'; \
\"\$cmake_bin\" -S . -B build/macos-debug -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug; \
\"\$cmake_bin\" --build build/macos-debug -j\"\$(sysctl -n hw.ncpu)\""

echo "Built Debug binaries at ${REMOTE_BUILD_DIR}."

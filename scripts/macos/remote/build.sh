#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

# Check prerequisites before syncing the source tree.
ssh_exec "set -euo pipefail; \
export PATH=\"\$HOME/Applications/CMake.app/Contents/bin\":/opt/homebrew/bin:/usr/local/bin:/Applications/CMake.app/Contents/bin:\$PATH; \
if ! xcrun --find clang++ >/dev/null 2>&1; then \
  echo 'Apple Command Line Tools are missing. Run xcode-select --install on the Mac and complete the installer.' >&2; \
  exit 127; \
fi; \
if ! command -v cmake >/dev/null 2>&1; then \
  echo 'CMake is missing. Install CMake.app in ~/Applications or /Applications, or install it with Homebrew.' >&2; \
  exit 127; \
fi"

"${SCRIPT_DIR}/sync.sh"

ssh_exec "set -euo pipefail; \
export PATH=\"\$HOME/Applications/CMake.app/Contents/bin\":/opt/homebrew/bin:/usr/local/bin:/Applications/CMake.app/Contents/bin:\$PATH; \
mkdir -p '${REMOTE_PROJECT_DIR}'; \
cd '${REMOTE_PROJECT_DIR}'; \
cmake -S . -B build/macos-debug -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug; \
cmake --build build/macos-debug -j\"\$(sysctl -n hw.ncpu)\""

echo "Built Debug binaries at ${REMOTE_BUILD_DIR}."

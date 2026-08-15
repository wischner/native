#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

"${SCRIPT_DIR}/build.sh"

ssh_exec "set -euo pipefail; \
bundle='${REMOTE_VISION_BUNDLE}'; \
bin='${REMOTE_VISION_EXE}'; \
if [ ! -x \"\$bin\" ]; then \
  echo \"Missing binary: \$bin\" >&2; \
  exit 1; \
fi; \
xattr -dr com.apple.quarantine \"\$bundle\" 2>/dev/null || true; \
codesign --force --deep --sign - \"\$bundle\" >/dev/null 2>&1 || true; \
if xattr -r \"\$bundle\" | grep -q com.apple.quarantine; then \
  echo \"Quarantine flag present: \$bundle\" >&2; \
  exit 1; \
fi; \
file \"\$bin\"; \
otool -L \"\$bin\" | sed -n '1,6p'; \
log=\"/tmp/native-smoke-vision.log\"; \
\"\$bin\" >\"\$log\" 2>&1 & pid=\$!; \
sleep 2; \
if kill -0 \"\$pid\" 2>/dev/null; then \
  kill \"\$pid\" >/dev/null 2>&1 || true; \
  wait \"\$pid\" >/dev/null 2>&1 || true; \
  echo \"SMOKE_OK \$bin\"; \
else \
  echo \"SMOKE_FAIL \$bin\" >&2; \
  cat \"\$log\" >&2 || true; \
  exit 1; \
fi"

echo "macOS remote smoke test passed on ${REMOTE}."

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./common.sh
source "${SCRIPT_DIR}/common.sh"

"${SCRIPT_DIR}/build.sh"

ssh_exec "set -euo pipefail; \
for bin in '${REMOTE_APP_EXE}' '${REMOTE_PAINTER_EXE}'; do \
  if [ ! -x \"\$bin\" ]; then \
    echo \"Missing binary: \$bin\" >&2; \
    exit 1; \
  fi; \
  xattr -d com.apple.quarantine \"\$bin\" 2>/dev/null || true; \
  codesign --force --sign - \"\$bin\" >/dev/null 2>&1 || true; \
  if xattr -p com.apple.quarantine \"\$bin\" >/dev/null 2>&1; then \
    echo \"Quarantine flag present: \$bin\" >&2; \
    exit 1; \
  fi; \
  file \"\$bin\"; \
  otool -L \"\$bin\" | sed -n '1,6p'; \
done; \
for bin in '${REMOTE_APP_EXE}' '${REMOTE_PAINTER_EXE}'; do \
  log=\"/tmp/native-smoke-\$(basename \"\$bin\").log\"; \
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
  fi; \
done"

echo "macOS remote smoke test passed on ${REMOTE}."

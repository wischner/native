#!/usr/bin/env bash
set -euo pipefail

# cppdbg enables debuginfod internally. Wine exposes many system modules, and
# downloading all of their debug data can stall startup indefinitely.
unset DEBUGINFOD_URLS
exec /usr/bin/gdb "$@"

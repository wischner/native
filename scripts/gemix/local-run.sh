#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <app-path> [app-args...]" >&2
    exit 2
fi

APP_PATH=$1
shift || true

RASTA_BIN=${RASTA_BIN:-/home/tstih/data/tstih/rasta/bin/rasta}
GEM_LIB_DIR=${GEM_LIB_DIR:-/home/tstih/data/triglav-os/gem/bin}
GEMIX_APP_IMAGE=${GEMIX_APP_IMAGE:-wischner/gcc-x86_64-gemix:latest}
GEMIX_RUN_MODE=${GEMIX_RUN_MODE:-docker}
SANITIZER_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/11/libasan.so
SANITIZER_PRELOAD+=:/usr/lib/gcc/x86_64-linux-gnu/11/libubsan.so

RASTA_WIDTH=${RASTA_WIDTH:-900}
RASTA_HEIGHT=${RASTA_HEIGHT:-900}
RASTA_BPP=${RASTA_BPP:-1}
RASTA_FB=${RASTA_FB:-/tmp/rasta.fb}
RASTA_PORT=${RASTA_PORT:-5000}
RASTA_HOST=${RASTA_HOST:-127.0.0.1}
RASTA_SCALE=${RASTA_SCALE:-1}
RASTA_CURSOR=${RASTA_CURSOR:-off}
RASTA_INVERSE=${RASTA_INVERSE:-on}

EFFECTIVE_RASTA_WIDTH=$RASTA_WIDTH
if (( RASTA_BPP == 1 )) && (( RASTA_WIDTH % 8 != 0 )); then
    EFFECTIVE_RASTA_WIDTH=$(( ((RASTA_WIDTH + 7) / 8) * 8 ))
    echo "adjusting GEMix+rasta width from $RASTA_WIDTH to" \
        "$EFFECTIVE_RASTA_WIDTH for 1bpp byte alignment." >&2
fi

if [[ ! -x "$RASTA_BIN" ]]; then
    echo "missing rasta binary: $RASTA_BIN" >&2
    exit 1
fi

if [[ ! -x "$APP_PATH" ]]; then
    echo "missing app binary: $APP_PATH" >&2
    exit 1
fi

RASTA_PID=

cleanup() {
    local rc=$?
    if [[ -n "${RASTA_PID:-}" ]] && kill -0 "$RASTA_PID" 2>/dev/null; then
        kill "$RASTA_PID" 2>/dev/null || true
        wait "$RASTA_PID" 2>/dev/null || true
    fi
    exit $rc
}

trap cleanup EXIT INT TERM

"$RASTA_BIN" \
    --width "$EFFECTIVE_RASTA_WIDTH" \
    --height "$RASTA_HEIGHT" \
    --bpp "$RASTA_BPP" \
    --framebuffer "$RASTA_FB" \
    --port "$RASTA_PORT" \
    --scale "$RASTA_SCALE" \
    --cursor "$RASTA_CURSOR" \
    --inverse "$RASTA_INVERSE" &
RASTA_PID=$!
sleep 0.5
if [[ -e "$RASTA_FB" ]]; then
    chmod 666 "$RASTA_FB" 2>/dev/null || true
fi

if [[ "$GEMIX_RUN_MODE" == "host" ]]; then
    export GEM_VDI_WIDTH="$EFFECTIVE_RASTA_WIDTH"
    export GEM_VDI_HEIGHT="$RASTA_HEIGHT"
    export GEM_RASTA_FRAMEBUFFER="$RASTA_FB"
    export GEM_RASTA_HOST="$RASTA_HOST"
    export GEM_RASTA_PORT="$RASTA_PORT"
    export GEM_RASTA_SCALE="$RASTA_SCALE"
    export GEM_RASTA_CURSOR="$RASTA_CURSOR"
    export GEM_RASTA_INVERSE="$RASTA_INVERSE"
    export GEM_RESOURCE_DIR="${GEM_RESOURCE_DIR:-$GEM_LIB_DIR/gemix/share/gem}"
    export LD_LIBRARY_PATH="$GEM_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    LD_LIBRARY_PATH="$GEM_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$APP_PATH" "$@"
else
    docker run --rm \
        --network host \
        -u "$(id -u):$(id -g)" \
        -v "$PWD":"$PWD" \
        -v /tmp:/tmp \
        -e GEM_TRACE_AES="${GEM_TRACE_AES:-}" \
        -e GEM_TRACE_DRAW="${GEM_TRACE_DRAW:-}" \
        -e GEM_TRACE_HID="${GEM_TRACE_HID:-}" \
        -e GEM_RESOURCE_DIR="${GEM_RESOURCE_DIR:-/opt/gemix/share/gem}" \
        -e GEM_VDI_WIDTH="$EFFECTIVE_RASTA_WIDTH" \
        -e GEM_VDI_HEIGHT="$RASTA_HEIGHT" \
        -e GEM_RASTA_FRAMEBUFFER="$RASTA_FB" \
        -e GEM_RASTA_HOST="$RASTA_HOST" \
        -e GEM_RASTA_PORT="$RASTA_PORT" \
        -e GEM_RASTA_SCALE="$RASTA_SCALE" \
        -e GEM_RASTA_CURSOR="$RASTA_CURSOR" \
        -e GEM_RASTA_INVERSE="$RASTA_INVERSE" \
        -e LD_PRELOAD="$SANITIZER_PRELOAD" \
        -e ASAN_OPTIONS=detect_leaks=0 \
        -w "$PWD" \
        "$GEMIX_APP_IMAGE" \
        bash -lc 'exec "$@"' bash "$APP_PATH" "$@"
fi

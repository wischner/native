#!/usr/bin/env bash
# Deploy Docker-built Vision and its assets to the elementary Wayland VM.
set -euo pipefail
workspace=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
host=${ELEMENTARY_VM_HOST:-echo}
remote_dir=/home/tomaz/Projects/native/run
staging=$workspace/build/elementary-vm
mkdir -p "$staging/lib"
[[ -x $workspace/build/linux-elementary/src/vision ]] || {
    echo 'Build docker-elementary before deploying.' >&2
    exit 1
}
docker run --rm native-wayland-sdk:24.04 \
    cat /usr/lib/x86_64-linux-gnu/libgtksourceview-5.so.0 \
    > "$staging/lib/libgtksourceview-5.so.0"
ssh -o BatchMode=yes "$host" "mkdir -p $remote_dir"
rsync -az "$workspace/build/linux-elementary/src/vision" \
    "$workspace/build/linux-elementary/src/retro-terminal" \
    "$staging/lib" "$host:$remote_dir/"
echo "Deployed Vision to $host:$remote_dir" >&2

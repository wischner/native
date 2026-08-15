#!/usr/bin/env bash
set -euo pipefail

# Start one managed VM when necessary and wait for key-based SSH access.
if [[ $# -ne 2 ]]; then
    echo "usage: $0 <libvirt-domain> <ssh-target>" >&2
    exit 2
fi

DOMAIN=$1
SSH_TARGET=$2

DOMAIN_STATE=$(virsh domstate "${DOMAIN}")
if [[ "${DOMAIN_STATE}" != "running" ]]; then
    echo "Starting libvirt domain ${DOMAIN}..."
    virsh start "${DOMAIN}"
else
    echo "Libvirt domain ${DOMAIN} is already running."
fi

echo "Waiting for key-based SSH login as ${SSH_TARGET}..."
for _ in $(seq 1 45); do
    if ssh \
        -o BatchMode=yes \
        -o ConnectTimeout=2 \
        "${SSH_TARGET}" true 2>/dev/null; then
        echo "SSH login verified; ${DOMAIN} is ready at ${SSH_TARGET}."
        exit 0
    fi
    sleep 1
done

echo "${DOMAIN} started, but SSH did not become ready at ${SSH_TARGET}." >&2
exit 1

FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive

# Minimal toolchain used by all variants
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
# No user baked in; we run with -u <host uid:gid> from CMake.

# Haiku cross toolchain (x86_64). Image name tracks current Haiku SDK tag.
FROM haiku/cross-compiler:x86_64-r1beta4
ARG DEBIAN_FRONTEND=noninteractive

# Basic build helpers *inside* the container for the inner CMake build
# (the base image is Debian/Ubuntu-like, so apt is fine)
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    make \
    git \
    pkg-config \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work

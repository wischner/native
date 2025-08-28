FROM native-build:base
ARG DEBIAN_FRONTEND=noninteractive

# MinGW-w64 cross toolchain (x86_64), plus windres for resources
RUN apt-get update && apt-get install -y --no-install-recommends \
    mingw-w64 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work

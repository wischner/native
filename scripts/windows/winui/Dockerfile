FROM ubuntu:24.04@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake make g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix \
    gdb-mingw-w64 gdb-mingw-w64-target python3 ca-certificates wine64 zstd \
    && rm -rf /var/lib/apt/lists/*
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
    wine32:i386 && rm -rf /var/lib/apt/lists/*
COPY install-sdk.py /tmp/install-sdk.py
RUN python3 /tmp/install-sdk.py && rm /tmp/install-sdk.py

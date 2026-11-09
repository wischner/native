# Repair ownership in the pinned SDK's WINGs implementation. Keep its
# native widgets and all existing session/debugging tools.
FROM wischner/gcc-x86_64-linux-window-maker@sha256:eebadf566cc05a0a1728957d2df27bceabf32e752383342f705fe770fb8fc409
COPY wings-lifetimes.patch /tmp/wings-lifetimes.patch
RUN cd /usr/local/src/window-maker \
    && patch -p1 < /tmp/wings-lifetimes.patch \
    && cmake -S . -B /tmp/native-wings-build \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DWMAKER_DEV_TREE=OFF -DWMAKER_STAGE_DIR=/tmp/native-wings-stage \
        -DBUILD_SHARED_LIBS=ON -DUSE_PANGO=ON \
    && cmake --build /tmp/native-wings-build --target WINGs --parallel 2 \
    && cp /tmp/native-wings-stage/lib/libWINGs.so.3.2.0 /usr/local/lib/ \
    && ldconfig \
    && rm -rf /tmp/native-wings-build /tmp/native-wings-stage /tmp/wings-lifetimes.patch

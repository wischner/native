# Preserve the pinned SDK and desktop; give blank XView menu separators the
# same half-button height as olwm desktop separators.
FROM wischner/gcc-x86_64-linux-openlook@sha256:54aeff99c1f8f6a178a18311840d1fd20d0515db1cb629770e6fd1897e4d17a6
COPY menu-spacing.patch button-caption.patch /tmp/
RUN git init /usr/local/src/open-look \
    && cd /usr/local/src/open-look \
    && git remote add origin https://github.com/retro-vault/open-look.git \
    && git fetch --depth 1 origin 4af207984537f7f895ca06616ef07c1d79236694 \
    && git checkout --detach FETCH_HEAD \
    && patch -p1 < /tmp/menu-spacing.patch \
    && patch -p1 < /tmp/button-caption.patch \
    && cmake -S . -B /tmp/native-openlook-build \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/openwin \
        -DOPENLOOK_SYSCONFDIR=/etc/openlook \
    && cmake --build /tmp/native-openlook-build --target xview xview_static olgx olgx_static --parallel 4 \
    && cp /tmp/native-openlook-build/runtime/lib/libxview.so.1.0.0 /usr/openwin/lib/ \
    && cp /tmp/native-openlook-build/runtime/lib/libxview.a /usr/openwin/lib/ \
    && cp /tmp/native-openlook-build/runtime/lib/libolgx.so.1.0.0 /usr/openwin/lib/ \
    && cp /tmp/native-openlook-build/runtime/lib/libolgx.a /usr/openwin/lib/ \
    && ldconfig \
    && rm -rf /tmp/native-openlook-build /usr/local/src/open-look /tmp/menu-spacing.patch /tmp/button-caption.patch

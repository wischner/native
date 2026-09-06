# Build the matching direct and proxy SDK from the verified GEM release.
FROM wischner/gcc-x86_64-linux-x11:latest

LABEL org.opencontainers.image.title="native GEM v1.0.0 SDK" \
      org.opencontainers.image.source="https://github.com/triglav-os/gem" \
      org.opencontainers.image.revision="0df463d9ff4b4cbbdd0612ce700c9e11e5e33661"

RUN git clone --depth 1 --branch v1.0.0 https://github.com/triglav-os/gem.git /opt/gem-source \
    && test "$(git -C /opt/gem-source rev-parse HEAD)" = 0df463d9ff4b4cbbdd0612ce700c9e11e5e33661 \
    && cmake -S /opt/gem-source -B /opt/gem-source/build \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DGEM_PLATFORM=rasta \
    && cmake --build /opt/gem-source/build --target gem_sdk gemd --parallel 4 \
    && cp -a /opt/gem-source/bin/sdk /opt/gemix \
    && cp /opt/gem-source/bin/core/gemd /opt/gemix/bin/ \
    && cp -a /opt/gemix/include/. /usr/local/include/ \
    && cp -a /opt/gemix/lib/. /usr/local/lib/ \
    && ldconfig

COPY sdk-pkgconfig.cmake /opt/gemix/sdk-pkgconfig.cmake
RUN cmake -P /opt/gemix/sdk-pkgconfig.cmake

ENV GEM_RESOURCE_DIR=/opt/gemix/share/gem

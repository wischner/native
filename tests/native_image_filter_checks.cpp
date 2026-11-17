//
// Verifies eager filter order, independent storage, edge sampling,
// transparent interpolation and deterministic black-and-white dithering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>

#include <stdexcept>

namespace
{
    void require(bool condition) {
        if (!condition) throw std::runtime_error("image filter check failed");
    }
}

// Return success for the shared window/API test runner without a display.
bool native_image_filter_checks() {
    using namespace native;
    try {
        img source(2, 1);
        source.pixels()[0] = rgba(255, 0, 0, 255);
        source.pixels()[1] = rgba(0, 0, 255, 64);
        auto enlarged = source >> resize(4, 2);
        require(enlarged.w() == 4 && enlarged.h() == 2);
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 4; ++x)
                require(enlarged.pixels()[y * 4 + x] == source.pixels()[x / 2]);
        enlarged.pixels()[0] = rgba(0, 255, 0, 255);
        require(source.pixels()[0] == rgba(255, 0, 0, 255));

        // Fully transparent blue must not tint a translucent red edge.
        source.pixels()[1] = rgba(0, 0, 255, 0);
        auto linear = source >> resize(4, 1, image_filter::linear);
        require(linear.pixels()[0] == source.pixels()[0]);
        require(linear.pixels()[1] == rgba(255, 0, 0, 191));
        require(linear.pixels()[2] == rgba(255, 0, 0, 64));
        require(linear.pixels()[3].a == 0);

        img gray(1, 1);
        gray.pixels()[0] = rgba(128, 128, 128, 79);
        auto pattern = gray >> resize(4, 4) >> dither();
        int white = 0;
        for (int i = 0; i < 16; ++i) {
            const auto value = pattern.pixels()[i];
            require(value.r == value.g && value.g == value.b);
            require(value.r == 0 || value.r == 255);
            require(value.a == 79);
            white += value.r == 255;
        }
        require(white == 8 && pattern.w() == 4 && pattern.h() == 4);
        require(gray.pixels()[0] == rgba(128, 128, 128, 79));
        auto reverse = gray >> dither() >> resize(4, 4);
        for (int i = 0; i < 16; ++i)
            require(reverse.pixels()[i] == rgba(255, 255, 255, 79));

        img endpoints(2, 1);
        endpoints.pixels()[0] = rgba(0, 0, 0, 0);
        endpoints.pixels()[1] = rgba(255, 255, 255, 255);
        auto exact = endpoints >> dither();
        require(exact.pixels()[0] == endpoints.pixels()[0]);
        require(exact.pixels()[1] == endpoints.pixels()[1]);

        for (const auto dimensions : {size(0, 1), size(1, 0), size(32768, 1)}) {
            bool rejected = false;
            try { (void)resize(dimensions.w, dimensions.h); }
            catch (const std::invalid_argument &) { rejected = true; }
            require(rejected);
        }
        bool rejected = false;
        try { (void)resize(1, 1, static_cast<image_filter>(99)); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected);
        resize changed(1, 1);
        changed.width = 0;
        rejected = false;
        try { (void)(source >> changed); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected);
        return true;
    } catch (...) {
        return false;
    }
}

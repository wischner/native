//
// Checks real BGRA framebuffer pixels for image composition, clipping,
// repaint, native occlusion, window movement, resize and overlay teardown.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "../lib/native/toolkits/gemix/globals.h"

namespace
{
    void require(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    // Fence proxy drawing and read the display-ready BGRA framebuffer.
    std::vector<unsigned char> capture() {
        WORD attributes[6]{};
        vqt_attributes(linux::gemix::runtime.vdi_handle, attributes);
        std::ifstream stream(std::getenv("GEM_RASTA_FRAMEBUFFER"),
                             std::ios::binary);
        std::vector<unsigned char> bytes(
            std::istreambuf_iterator<char>(stream), {});
        require(bytes.size() == 904U * 900U * 4U,
                "GEM presenter uses a 32-bit BGRA framebuffer");
        return bytes;
    }

    native::rgba at(const std::vector<unsigned char> &bytes, int x, int y) {
        const auto i = (std::size_t(y) * 904 + x) * 4;
        return native::rgba(bytes.at(i + 2), bytes.at(i + 1),
                            bytes.at(i), bytes.at(i + 3));
    }
}

// Run with both in-process AES/VDI and serialized libgem/gemd transports.
void native_gemix_color_checks(native::app_wnd &owner) {
    using namespace native;
    using namespace linux::gemix;
    modeless_wnd host(owner, "Colour", 260, 100, 240, 180);
    canvas surface(12, 16, 120, 100);
    surface.set_horizontal_scrollbar_policy(scrollbar_policy::never)
        .set_vertical_scrollbar_policy(scrollbar_policy::never);
    img image(80, 60);
    image.get_gpx().clear(rgba(241, 37, 83, 255));
    img translucent(8, 8);
    translucent.get_gpx().clear(rgba(0, 0, 255, 128));
    host.on_wnd_paint.connect([](wnd_paint_event event) {
        event.g.clear(rgba(255, 255, 255, 255));
        return true;
    });
    surface.on_wnd_paint.connect([&](wnd_paint_event event) {
        event.g.clear(rgba(10, 60, 90, 255));
        event.g.draw_img(image, point(4, 4));
        event.g.set_ink(rgba(0, 255, 0, 255))
            .draw_line(point(6, 8), point(60, 8));
        event.g.draw_img(translucent, point(20, 20));
        event.g.set_ink(rgba(255, 255, 0, 255))
            .draw_rect(rect(30, 30, 6, 6), true);
        event.g.set_ink(rgba(0, 0, 0, 255))
            .draw_text("A", point(45, 30));
        // The canvas' active clip must exclude this image completely.
        event.g.draw_img(image, point(120, 0));
        return true;
    });
    host.create();
    surface.set_parent(&host);
    surface.create();
    surface.show();
    host.show();
    flush_repaints();
    const WORD handle = wnd_bindings.handle_from_object(&host);
    auto work = work_rect(handle);
    auto bytes = capture();
    const int x = work.x1() + 12, y = work.y1() + 16;
    require(at(bytes, x + 5, y + 5) == rgba(241, 37, 83, 255),
            "GEM retains full image colour and correct BGRA channel order");
    require(at(bytes, x + 10, y + 8) == rgba(0, 255, 0, 255),
            "lines drawn after an image remain above it");
    require(at(bytes, x + 31, y + 31) == rgba(255, 255, 0, 255),
            "filled rectangles retain colour above images");
    const auto blended = at(bytes, x + 21, y + 21);
    require(blended.r == 120 && blended.g == 18 && blended.b == 169 &&
            blended.a == 255, "GEM image alpha uses source-over composition");
    require(at(bytes, x + 125, y + 5) == rgba(255, 255, 255, 255),
            "canvas images cannot paint into their parent");
    img text(8, 16);
    text.get_gpx().clear(rgba(241, 37, 83, 255))
        .set_ink(rgba(0, 0, 0, 255)).draw_text("A", point(0, 0));
    for (int ty = 0; ty < 16; ++ty)
        for (int tx = 0; tx < 8; ++tx)
            require(at(bytes, x + 45 + tx, y + 30 + ty) ==
                    text.pixels()[ty * 8 + tx],
                    "text drawn after an image remains above it");

    image.get_gpx().clear(rgba(20, 190, 210, 255));
    surface.invalidate(rect(4, 4, 4, 4));
    flush_repaints();
    bytes = capture();
    require(at(bytes, x + 5, y + 5) == rgba(20, 190, 210, 255) &&
            at(bytes, x + 15, y + 5) == rgba(241, 37, 83, 255),
            "partial uploads update only invalidated pixels");

    // A real AES window has no Native overlay. Its background and frame
    // must still occlude colour pixels, even before application repaint.
    WORD cover = wind_create(NAME | MOVER, x, y, 90, 70);
    require(cover > 0, "native covering window created");
    wind_open(cover, x, y, 90, 70);
    GRECT cover_work{};
    wind_get(cover, WF_WORKXYWH, &cover_work.g_x, &cover_work.g_y,
             &cover_work.g_w, &cover_work.g_h);
    auto covered = capture();
    const int cx = cover_work.g_x + 4, cy = cover_work.g_y + 4;
    require(at(covered, cx, cy).r == at(covered, cx, cy).g &&
            at(covered, cx, cy).g == at(covered, cx, cy).b,
            "native AES windows occlude the colour overlay");
    wind_close(cover);
    wind_delete(cover);
    host.invalidate();
    flush_repaints();
    bytes = capture();
    require(at(bytes, x + 5, y + 5) == rgba(20, 190, 210, 255),
            "closing a covering window restores colour content");

    host.set_bounds(rect(300, 130, 260, 200));
    host.invalidate();
    flush_repaints();
    work = work_rect(handle);
    bytes = capture();
    require(at(bytes, work.x1() + 17, work.y1() + 21) ==
                rgba(20, 190, 210, 255),
            "overlay follows window movement and resize");
    require(at(bytes, x + 5, y + 5) != rgba(20, 190, 210, 255),
            "moving the overlay leaves no colour at its old origin");

    // Dirty rectangles ending at the final row must use packed uploads;
    // a full-image stride starting at nonzero x would overread that row.
    host.invalidate(rect(work.w() - 4, work.h() - 2, 3, 2));
    flush_repaints();
    bytes = capture();
    require(at(bytes, work.x2() - 3, work.y2() - 1) ==
                rgba(255, 255, 255, 255),
            "bottom-edge dirty uploads retain exact bounds");

    // Exercise the public immediate graphics context and retained repaint.
    surface.get_gpx().set_ink(rgba(77, 88, 99, 255))
        .draw_rect(rect(5, 5, 2, 2), true);
    bytes = capture();
    require(at(bytes, work.x1() + 17, work.y1() + 21) ==
                rgba(77, 88, 99, 255), "get_gpx presents colour immediately");
    host.destroy();
    bytes = capture();
    require(at(bytes, work.x1() + 17, work.y1() + 21) !=
                rgba(77, 88, 99, 255), "destroy releases the colour overlay");

    // More creations than the SDK's eight simultaneous layer slots prove
    // teardown releases slots rather than leaving invisible overlays.
    for (int i = 0; i < 10; ++i) {
        modeless_wnd temporary(owner, "Reuse", 260, 100, 80, 60);
        temporary.on_wnd_paint.connect([&](wnd_paint_event event) {
            event.g.draw_img(image, point(0, 0));
            return true;
        });
        temporary.create();
        temporary.show();
        flush_repaints();
        temporary.destroy();
    }
}

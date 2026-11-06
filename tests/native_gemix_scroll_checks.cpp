//
// Drives AES window scrollbars with real Rasta pointer input and checks
// that their work rectangle contains a gap-free document status band.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "native_gemix_control_checks.h"
#include "../lib/native/toolkits/gemix/globals.h"
#include <stdexcept>

namespace
{
    void expect(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }
}

void check_gemix_window_scrollbars(native::app_wnd &owner,
                                   const gem_control_driver &driver) {
    using namespace native;
    using namespace linux::gemix;
    modeless_wnd host(owner, "AES document scrolling", 60, 60, 440, 320);
    canvas document;
    status_bar status(host, document);
    rect work, outer;
    point down, right, vthumb, hthumb, vpage;
    int changes = 0, document_clicks = 0;
    const auto click = [&](point p) {
        driver.pointer(3, p); driver.pointer(10, p); driver.pointer(11, p);
    };
    const auto drag = [&](point from, point to) {
        driver.pointer(3, from); driver.pointer(10, from);
        driver.pointer(3, to); driver.pointer(11, to);
    };
    try {
        driver.wait([&] {
            document.set_parent(&host);
            host.set_scroll_content(&document);
            host.on_wnd_resize.connect([&](size) {
                document.set_bounds(host.get_client_bounds()); return true;
            });
            document.on_scroll.connect([&](canvas_scroll_position) {
                ++changes; return true;
            });
            document.on_mouse_click.connect([&](mouse_event) {
                ++document_clicks; return true;
            });
            document.on_wnd_paint.connect([](wnd_paint_event e) {
                e.g.clear({255, 255, 255, 255}); return true;
            });
            host.create();
            status.set_extent(theme::create(host.get_gpx())->get_status_bar_height());
            status.set_text("Native AES scrollbars");
            document.set_bounds(host.get_client_bounds());
            document.create(); document.show();
            document.set_content_bounds({0, 0, 1800, 2000});
            host.show();
            const WORD handle = wnd_bindings.handle_from_object(&host);
            work = work_rect(handle); outer = outer_rect(handle);
            WORD kind = 0, unused = 0;
            wind_get(handle, WF_KIND, &kind, &unused, &unused, &unused);
            const WORD required = UPARROW | DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE;
            expect((kind & required) == required, "AES owns both scrollbar axes");
            expect(document.get_window_scrollbars(), "canvas uses frame scrollbars");
            expect(outer.x2() - work.x2() > 16 && outer.y2() - work.y2() > 16,
                   "AES frame bars have native size, not the 16-pixel painted size");
            expect(status.get_bounds().y2() == work.h() &&
                   status.get_bounds().x2() == work.w(),
                   "status meets native tracks without an extra bottom or right strip");
            const int vx = work.x2() + (outer.x2() - work.x2()) / 2;
            const int hy = work.y2() + (outer.y2() - work.y2()) / 2;
            down = point(vx, work.y2() - 8);
            right = point(work.x2() - 8, hy);
            vthumb = point(vx, work.y1() + 30);
            hthumb = point(work.x1() + 30, hy);
            vpage = point(vx, work.y2() - 40);
            flush_repaints();
            return true;
        }, "create document with genuine AES scrollbars");
        click(down);
        driver.wait([&] { return document.get_scroll_position().y > 0 && changes > 0; },
                    "AES down arrow emits document scroll");
        click(right);
        driver.wait([&] { return document.get_scroll_position().x > 0; },
                    "AES right arrow emits document scroll");
        driver.wait([&] { document.set_scroll_position({0, 0}); return true; },
                    "reset document before native track click");
        click(vpage);
        driver.wait([&] {
            return document.get_scroll_position().y == document.get_client_bounds().h();
        }, "AES trough pages by document viewport");
        driver.wait([&] { document.set_scroll_position({0, 0}); return true; },
                    "reset document before native thumb drags");
        drag(vthumb, point(vthumb.x, work.y2() - 8));
        driver.wait([&] {
            return document.get_scroll_position().y == 2000 - document.get_client_bounds().h();
        }, "AES vertical thumb reaches the end");
        drag(hthumb, point(work.x2() - 8, hthumb.y));
        driver.wait([&] {
            return document.get_scroll_position().x == 1800 - document.get_client_bounds().w();
        }, "AES horizontal thumb reaches the end");
        driver.wait([&] {
            expect(document_clicks == 0, "frame gestures never click through to document");
            const int previous = changes;
            document.set_content_bounds({0, 0, 10, 10});
            expect(document.get_scroll_position().x == 0 && document.get_scroll_position().y == 0,
                   "short content clamps both axes");
            const WORD handle = wnd_bindings.handle_from_object(&host);
            WORD value = 0, unused = 0;
            wind_get(handle, WF_HSLSIZ, &value, &unused, &unused, &unused);
            expect(value == 1000, "short document fills native horizontal track");
            wind_get(handle, WF_VSLSIZ, &value, &unused, &unused, &unused);
            expect(value == 1000, "short document fills native vertical track");
            expect(previous == changes, "programmatic range changes remain silent");
            host.set_dimensions({480, 350});
            expect(status.get_bounds().y2() == document.get_dimensions().h,
                   "status stays flush after resize");
            document.destroy();
            expect(!dispatch_window_scroll(host, WM_VSLID, 500),
                   "destroyed canvas rejects stale scroll messages");
            host.destroy();
            return true;
        }, "verify scroll range, resize and destruction");
    } catch (...) {
        driver.wait([&] { host.destroy(); return true; }, "close failed AES scroll test");
        throw;
    }
}

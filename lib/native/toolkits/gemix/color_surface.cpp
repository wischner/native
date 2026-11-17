//
// Composes GEM work areas in RGBA and uploads BGRA through gemd or the
// in-process Rasta presenter. Direct AES topology changes refresh alpha
// masks from its visible rectangles, including menus and modal forms.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "color_surface.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include <gem/overlay.h>

#include "globals.h"

#ifndef NATIVE_GEMD
// The hosted SDK exports these presenter entry points from librasta.
// Its installed public headers currently expose only the gemd wrapper.
extern "C" {
    int gem_raster_overlay_create(std::uint32_t, int, int,
                                 std::uint16_t, std::uint16_t);
    int gem_raster_overlay_update(std::uint32_t, int, int,
                                 std::uint16_t, std::uint16_t,
                                 const std::uint8_t *, std::uint16_t);
    void gem_raster_overlay_destroy(std::uint32_t);
    void gem_raster_overlay_set_visible(std::uint32_t, int);
}
#endif

namespace
{
    // Encode a clipped rectangle without altering the retained RGBA image.
    void encode(linux::gemix::color_surface &surface,
                const native::rect &area, bool packed = false) {
        const int width = surface.pixels->w();
        for (int y = area.y1(); y < area.y2(); ++y) {
            for (int x = area.x1(); x < area.x2(); ++x) {
                const auto index = std::size_t(y) * width + x;
                const auto pixel = surface.pixels->pixels()[index];
                const auto destination = packed
                    ? std::size_t(y - area.y1()) * area.w() + x - area.x1()
                    : index;
                surface.bgra[destination * 4] = pixel.b;
                surface.bgra[destination * 4 + 1] = pixel.g;
                surface.bgra[destination * 4 + 2] = pixel.r;
                surface.bgra[destination * 4 + 3] = pixel.a;
            }
        }
    }
} // namespace

namespace linux::gemix
{
    color_surface::~color_surface() {
        if (!overlay) return;
#ifdef NATIVE_GEMD
        gem_overlay_destroy(overlay);
#else
        gem_raster_overlay_destroy(overlay);
#endif
    }

    void prepare_color_surface(native::app_wnd &owner) {
        auto &surface = window_states.object_from_handle(&owner)->color;
        const WORD handle = wnd_bindings.handle_from_object(&owner);
        const auto work = work_rect(handle);
        surface.work = work;
        if (surface.pixels && surface.pixels->w() == work.w() &&
            surface.pixels->h() == work.h()) return;
        auto pixels = std::make_unique<native::img>(work.w(), work.h());
        pixels->get_gpx().clear(native::rgba(255, 255, 255, 255));
        std::vector<std::uint8_t> bgra;
        if (surface.overlay) {
            if (work.w() > 16383 ||
                std::size_t(work.w()) * work.h() > 2U * 1024U * 1024U)
                throw std::runtime_error("GEM colour surface exceeds SDK limits");
            bgra.resize(std::size_t(work.w()) * work.h() * 4);
#ifdef NATIVE_GEMD
            if (!gem_overlay_resize(surface.overlay, work.w(), work.h()))
                throw std::runtime_error("GEM colour overlay resize failed");
#else
            const auto id = 0x4e000000U + static_cast<unsigned>(handle);
            if (!gem_raster_overlay_create(id, work.x1(), work.y1(),
                                           work.w(), work.h()))
                throw std::runtime_error("GEM colour overlay creation failed");
            surface.overlay = id;
#endif
        }
        surface.pixels = std::move(pixels);
        surface.bgra = std::move(bgra);
    }

    void present_color_surface(native::app_wnd &owner,
                               const native::rect &dirty) {
        auto *state = window_states.object_from_handle(&owner);
        if (!state || !state->color.overlay) return;
        auto &surface = state->color;
        const native::rect bounds(0, 0, surface.pixels->w(),
                                   surface.pixels->h());
        const auto area = dirty.intersect(bounds);
        if (!area.w() || !area.h()) return;
#ifdef NATIVE_GEMD
        // libgem serializes pitch * height bytes, including row padding.
        // Pack dirty rectangles tightly to avoid reading beyond the last row.
        const auto pitch = static_cast<std::uint16_t>(area.w() * 4);
        encode(surface, area, true);
        const auto *data = surface.bgra.data();
        if (!gem_overlay_update(surface.overlay, area.x1(), area.y1(),
                                 area.w(), area.h(), data, pitch))
            throw std::runtime_error("GEM colour overlay upload failed");
#else
        const auto pitch = static_cast<std::uint16_t>(bounds.w() * 4);
        const WORD handle = wnd_bindings.handle_from_object(&owner);
        const auto work = work_rect(handle);
        if (!gem_raster_overlay_create(surface.overlay, work.x1(), work.y1(),
                                       bounds.w(), bounds.h()))
            throw std::runtime_error("GEM colour overlay placement failed");
        // Zero covered pixels before restoring only the visible pieces.
        // This also clips old storage immediately when AES shrinks a window.
        for (int y = area.y1(); y < area.y2(); ++y)
            std::fill_n(surface.bgra.data() +
                (std::size_t(y) * bounds.w() + area.x1()) * 4,
                std::size_t(area.w()) * 4, 0);
        GRECT box{};
        wind_get(handle, WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);
        while (box.g_w > 0 && box.g_h > 0) {
            const auto visible = native::rect(box.g_x, box.g_y,
                box.g_w, box.g_h).intersect(work);
            encode(surface, native::rect(visible.x1() - work.x1(),
                visible.y1() - work.y1(), visible.w(), visible.h())
                .intersect(area));
            wind_get(handle, WF_NEXTXYWH,
                     &box.g_x, &box.g_y, &box.g_w, &box.g_h);
        }
        const auto *data = surface.bgra.data() +
            (std::size_t(area.y1()) * bounds.w() + area.x1()) * 4;
        if (!gem_raster_overlay_update(surface.overlay, area.x1(), area.y1(),
                                        area.w(), area.h(), data, pitch))
            throw std::runtime_error("GEM colour overlay upload failed");
        gem_raster_overlay_set_visible(surface.overlay, 1);
#endif
    }

    color_target::color_target(native::wnd &window,
                               native::point screen_offset,
                               native::gpx &source) {
        // Explicit out-of-paint screen contexts draw AES frame/reference
        // pixels through VDI. Public get_gpx() uses the default offset.
        if (!runtime.painting && (screen_offset.x || screen_offset.y)) return;
        _owner = dynamic_cast<native::app_wnd *>(root_of(&window));
        if (!_owner || !window_states.object_from_handle(_owner)) return;
        if (!runtime.painting) prepare_color_surface(*_owner);
        const auto work = window_states.object_from_handle(_owner)->color.work;
        if (!screen_offset.x && !screen_offset.y) {
            offset = origin_in_root(window);
            screen_offset = native::point(work.x1() + offset.x,
                                           work.y1() + offset.y);
        } else {
            offset = native::point(screen_offset.x - work.x1(),
                                    screen_offset.y - work.y1());
        }
        const auto clip = source.get_clip();
        auto screen_clip = native::rect(screen_offset.x + clip.x1(),
            screen_offset.y + clip.y1(), clip.w(), clip.h()).intersect(work);
        if (runtime.painting)
            screen_clip = screen_clip.intersect(runtime.paint_clip);
        _clip = native::rect(screen_clip.x1() - work.x1(),
            screen_clip.y1() - work.y1(), screen_clip.w(), screen_clip.h());
        graphics = &window_states.object_from_handle(_owner)->color
                        .pixels->get_gpx();
        graphics->set_ink(source.get_ink()).set_paper(source.get_paper())
            .set_pen(source.get_pen()).set_font(source.get_font()).set_clip(_clip);
    }

    void color_target::enable_color() {
        if (!graphics) return;
        auto &surface = window_states.object_from_handle(_owner)->color;
        if (surface.overlay) return;
        const auto work = surface.work;
        // Both presenter transports have an 8 MiB per-layer limit and
        // a 16-bit byte pitch. Reject unsupported sizes before allocation.
        if (!work.w() || !work.h() || work.w() > 16383 ||
            std::size_t(work.w()) * work.h() > 2U * 1024U * 1024U)
            throw std::runtime_error("GEM colour surface exceeds SDK limits");
        surface.bgra.resize(std::size_t(work.w()) * work.h() * 4);
        const WORD handle = wnd_bindings.handle_from_object(_owner);
#ifdef NATIVE_GEMD
        surface.overlay = gem_overlay_create(handle, work.w(), work.h());
        if (!surface.overlay)
            throw std::runtime_error("GEM colour overlay creation failed");
#else
        const auto id = 0x4e000000U + static_cast<unsigned>(handle);
        if (!gem_raster_overlay_create(id, work.x1(), work.y1(),
                                       work.w(), work.h()))
            throw std::runtime_error("GEM colour overlay creation failed");
        surface.overlay = id;
#endif
        // The first colour operation needs the preceding drawing too.
        // Publish it with this region's completed paint transaction.
    }

    bool color_target::present() {
        if (!graphics) return false;
        const auto &surface = window_states.object_from_handle(_owner)->color;
        if (!surface.overlay) return false;
        if (!runtime.painting)
            present_color_surface(*_owner, _clip);
        return true;
    }
} // namespace linux::gemix

#ifndef NATIVE_GEMD
// AES invokes its exported weak hook before presenting topology damage.
// Never allow a C++ exception to cross the native callback boundary.
extern "C" void gemd_overlay_window_top_changed() {
    for (auto *owner : linux::gemix::windows) {
        auto *state = linux::gemix::window_states.object_from_handle(owner);
        if (!state || !state->color.overlay) continue;
        try {
            linux::gemix::present_color_surface(*owner, native::rect(
                0, 0, state->color.pixels->w(), state->color.pixels->h()));
        } catch (const std::exception &error) {
            gem_raster_overlay_set_visible(state->color.overlay, 0);
            std::fprintf(stderr, "Native GEM: %s\n", error.what());
        }
    }
}
#endif

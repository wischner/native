//
// Implements the Haiku window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <cstring>

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Bitmap.h>
#include <Region.h>
#include <String.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_bview_state(BView *view,
                              native::gpx_wnd *self,
                              haiku::haiku_gpx *cache) {
    if (!view || !cache)
        return;

    // Native widgets and BControlLook share this view. Their drawing state
    // is not represented by our cache, so establish it for every primitive.
    {
        native::rgba c = self->get_ink();
        rgb_color color = {c.r, c.g, c.b, c.a};
        view->SetHighColor(color);
        cache->current_fg = self->get_ink();
        cache->current_fg_valid = true;
    }

    // Reestablish the pen for the same reason as the foreground.
    {
        view->SetPenSize(self->get_pen());
        cache->current_thickness = self->get_pen();
    }

    // Set clip region
    BRect clip_rect(self->get_clip().p.x,
                    self->get_clip().p.y,
                    self->get_clip().x2() - 1,
                    self->get_clip().y2() - 1);
    BRegion region(clip_rect);
    view->ConstrainClippingRegion(&region);
}

template <typename function_type>
static void with_locked_view(BView *view, function_type &&function) {
    if (!view)
        return;

    BLooper *looper = view->Looper();
    if (!looper)
        return;

    const bool already_locked = looper->IsLocked();
    if (!already_locked && !looper->Lock())
        return;

    view->PushState();
    function(view);
    view->PopState();

    if (!already_locked)
        looper->Unlock();
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        BView *control_view = haiku::view_from_control(_wnd);
        BWindow *bwin = control_view
                            ? control_view->Window()
                            : haiku::wnd_bindings.handle_from_object(_wnd);
        if (!bwin)
            throw std::runtime_error(
                "Haiku: No BWindow available for gpx_wnd");

        // Get or create view
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache) {
            cache = new haiku::haiku_gpx();

            const bool already_locked = bwin->IsLocked();
            if (!already_locked && !bwin->Lock())
                throw std::runtime_error(
                    "Haiku: Failed to lock BWindow while creating "
                    "gpx_wnd.");

            cache->view = control_view ? control_view : bwin->ChildAt(0);
            if (!cache->view) {
                BRect bounds = bwin->Bounds();
                cache->view = new BView(
                    bounds, "MainView", B_FOLLOW_ALL, B_WILL_DRAW);
                bwin->AddChild(cache->view);
            }

            if (!already_locked)
                bwin->Unlock();

            haiku::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return;

        // The BWindow owns the cached view.
        delete cache;
        haiku::wnd_gpx_bindings.unregister_by_handle(_wnd);
    }

    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);
            rgb_color c = {color.r, color.g, color.b, color.a};
            view->SetHighColor(c);

            BRect rect(
                _clip.p.x, _clip.p.y, _clip.x2() - 1, _clip.y2() - 1);
            view->FillRect(rect);
        });

        cache->current_fg = color;
        cache->current_fg_valid = true;

        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);
            view->StrokeLine(BPoint(from.x, from.y),
                             BPoint(to.x, to.y));
        });

        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);

            BRect rect(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);

            if (filled)
                view->FillRect(rect);
            else
                view->StrokeRect(rect);
        });

        return *this;
    }

    gpx &gpx_wnd::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);

            auto *fh = haiku::font_bindings.object_from_handle(
                get_font().id());
            if (fh)
                view->SetFont(&fh->bfont);

            view->SetDrawingMode(B_OP_OVER);
            view->DrawString(
                text.c_str(),
                BPoint(p.x, p.y + get_font_metrics().ascent));
        });

        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        auto *cache = haiku::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);

            BRect bounds(0, 0, src.w() - 1, src.h() - 1);
            BBitmap bitmap(bounds, B_RGBA32);
            if (!bitmap.IsValid())
                return;

            auto *base = static_cast<std::uint8_t *>(bitmap.Bits());
            for (int y = 0; y < src.h(); ++y) {
                auto *row = base + y * bitmap.BytesPerRow();
                for (int x = 0; x < src.w(); ++x) {
                    const rgba pixel = src.pixels()[y * src.w() + x];
                    row[x * 4] = pixel.b;
                    row[x * 4 + 1] = pixel.g;
                    row[x * 4 + 2] = pixel.r;
                    row[x * 4 + 3] = pixel.a;
                }
            }
            const drawing_mode old_mode = view->DrawingMode();
            source_alpha old_source;
            alpha_function old_function;
            view->GetBlendingMode(&old_source, &old_function);
            view->SetDrawingMode(B_OP_ALPHA);
            view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            view->DrawBitmap(&bitmap, BPoint(dst.x, dst.y));
            view->SetBlendingMode(old_source, old_function);
            view->SetDrawingMode(old_mode);
        });

        return *this;
    }

} // namespace native

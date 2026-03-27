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

static void apply_bview_state(BView *view, native::gpx_wnd *self, haiku::haikugpx *cache)
{
    if (!view || !cache)
        return;

    // Set high color (foreground) if changed
    if (cache->current_fg != self->ink())
    {
        native::rgba c = self->ink();
        rgb_color color = {c.r, c.g, c.b, c.a};
        view->SetHighColor(color);
        cache->current_fg = self->ink();
    }

    // Set pen size if changed
    if (cache->current_thickness != self->pen())
    {
        view->SetPenSize(self->pen());
        cache->current_thickness = self->pen();
    }

    // Set clip region
    BRect clip_rect(
        self->clip().p.x,
        self->clip().p.y,
        self->clip().x2(),
        self->clip().y2());
    BRegion region(clip_rect);
    view->ConstrainClippingRegion(&region);
}

template <typename Fn>
static void with_locked_view(BView *view, Fn &&fn)
{
    if (!view)
        return;

    BLooper *looper = view->Looper();
    if (!looper)
        return;

    const bool already_locked = looper->IsLocked();
    if (!already_locked && !looper->Lock())
        return;

    fn(view);

    if (!already_locked)
        looper->Unlock();
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        BWindow *bwin = haiku::wnd_bindings.from_b(_wnd);
        if (!bwin)
            throw std::runtime_error("Haiku: No BWindow available for gpx_wnd");

        // Get or create view
        auto *cache = haiku::wnd_gpx_bindings.from_a(_wnd);
        if (!cache)
        {
            cache = new haiku::haikugpx();

            const bool already_locked = bwin->IsLocked();
            if (!already_locked && !bwin->Lock())
                throw std::runtime_error("Haiku: Failed to lock BWindow while creating gpx_wnd.");

            cache->view = bwin->ChildAt(0);
            if (!cache->view)
            {
                BRect bounds = bwin->Bounds();
                cache->view = new BView(bounds, "MainView", B_FOLLOW_ALL, B_WILL_DRAW);
                bwin->AddChild(cache->view);
            }

            if (!already_locked)
                bwin->Unlock();

            haiku::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
    }

    gpx_wnd::~gpx_wnd() = default;

    gpx &gpx_wnd::set_clip(const rect &r)
    {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::clip() const
    {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color)
    {
        auto *cache = haiku::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            rgb_color c = {color.r, color.g, color.b, color.a};
            view->SetHighColor(c);

            BRect rect(_clip.p.x, _clip.p.y, _clip.x2(), _clip.y2());
            view->FillRect(rect);
        });

        cache->current_fg = color;

        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        auto *cache = haiku::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);
            view->StrokeLine(BPoint(from.x, from.y), BPoint(to.x, to.y));
        });

        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        auto *cache = haiku::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);

            BRect rect(r.p.x, r.p.y, r.x2(), r.y2());

            if (filled)
                view->FillRect(rect);
            else
                view->StrokeRect(rect);
        });

        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        auto *cache = haiku::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);

            auto *fh = haiku::font_bindings.from_a(font().id());
            if (fh) view->SetFont(&fh->bfont);

            view->DrawString(text.c_str(), BPoint(p.x, p.y));
        });

        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        auto *cache = haiku::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        with_locked_view(cache->view, [&](BView *view) {
            apply_bview_state(view, this, cache);

            BRect bounds(0, 0, src.w() - 1, src.h() - 1);
            BBitmap bitmap(bounds, B_RGBA32);
            if (!bitmap.IsValid())
                return;

            std::memcpy(bitmap.Bits(), src.pixels(), static_cast<std::size_t>(src.w()) * src.h() * 4);
            view->DrawBitmap(&bitmap, BPoint(dst.x, dst.y));
        });

        return *this;
    }

} // namespace native

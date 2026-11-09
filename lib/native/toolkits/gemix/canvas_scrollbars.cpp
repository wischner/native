//
// Adapts canvas scroll state to platform-owned window scrollbars.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../canvas_scrollbars.h"
#include "globals.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
    // The AES slider protocol is 0..1000; keep products in 64 bits.
    struct axis_range {
        std::int64_t origin;
        std::int64_t extent;
        std::int64_t page;
        std::int64_t value;
        std::int64_t travel() const {
            return std::min<std::int64_t>(
                std::max<std::int64_t>(0, extent - page),
                std::int64_t(std::numeric_limits<std::int32_t>::max()) - origin);
        }
        int position() const {
            return travel() ? int(1000 * (value - origin) / travel()) : 0;
        }
        int size() const { return extent ? int(std::min<std::int64_t>(1000, 1000 * page / extent)) : 1000; }
    };

    axis_range range(const native::canvas &content, bool horizontal) {
        const auto bounds = content.get_content_bounds();
        const auto viewport = content.get_client_bounds();
        const auto scroll = content.get_scroll_position();
        return horizontal ? axis_range{bounds.x, bounds.width, viewport.w(), scroll.x}
                          : axis_range{bounds.y, bounds.height, viewport.h(), scroll.y};
    }

    void publish(WORD handle, WORD field, int value, int &cached) {
        if (cached == value) return;
        wind_set(handle, field, WORD(value), 0, 0, 0);
        cached = value;
    }
}

namespace linux::gemix
{
    WORD window_scroll_kind(const native::app_wnd &owner) {
        const auto *content = owner.get_scroll_content();
        if (!content || content->get_parent() != &owner ||
            owner.get_modal() || !owner.get_native_title_visible()) return 0;
        WORD kind = 0;
        if (content->get_horizontal_scrollbar_policy() != native::scrollbar_policy::never)
            kind |= LFARROW | RTARROW | HSLIDE;
        if (content->get_vertical_scrollbar_policy() != native::scrollbar_policy::never)
            kind |= UPARROW | DNARROW | VSLIDE;
        return kind;
    }

    bool dispatch_window_scroll(native::app_wnd &owner, WORD message, WORD value) {
        auto *content = owner.get_scroll_content();
        if (!content || !content->get_created() || !content->get_visible() ||
            !content->get_input_enabled() || !content->get_window_scrollbars()) return false;
        if (message != WM_ARROWED && message != WM_HSLID && message != WM_VSLID) return false;
        if (message == WM_ARROWED && (value < 0 || value > 7)) return false;
        const bool horizontal = message == WM_HSLID || (message == WM_ARROWED && value >= 4);
        const auto axis = range(*content, horizontal);
        std::int64_t next = axis.value;
        if (message == WM_ARROWED) {
            const int action = value % 4;
            const auto step = action < 2 ? std::max<std::int64_t>(1, axis.page)
                : std::max(1, native::theme::create(content->get_gpx())->get_list_item_height());
            next += (action % 2) ? step : -step;
        } else {
            next = axis.origin + axis.travel() * std::clamp<int>(value, 0, 1000) / 1000;
        }
        next = std::clamp(next, axis.origin, axis.origin + axis.travel());
        next = std::clamp<std::int64_t>(next, std::numeric_limits<std::int32_t>::min(),
                                       std::numeric_limits<std::int32_t>::max());
        auto scroll = content->get_scroll_position();
        if (horizontal) scroll.x = static_cast<std::int32_t>(next);
        else scroll.y = static_cast<std::int32_t>(next);
        content->on_native_scroll(scroll);
        return true;
    }
}

namespace native::detail
{
    bool canvas_window_scrollbars(const canvas &content) {
        const auto *owner = dynamic_cast<const app_wnd *>(content.get_parent());
        if (!owner || owner->get_scroll_content() != &content) return false;
        const auto *state = linux::gemix::window_states.object_from_handle(
            const_cast<app_wnd *>(owner));
        return state && state->scroll_kind;
    }

    void synchronize_canvas_scrollbars(canvas &content) {
        if (!canvas_window_scrollbars(content)) return;
        auto *owner = static_cast<app_wnd *>(content.get_parent());
        auto *state = linux::gemix::window_states.object_from_handle(owner);
        const WORD handle = linux::gemix::wnd_bindings.handle_from_object(owner);
        const auto horizontal = range(content, true);
        const auto vertical = range(content, false);
        if (state->scroll_kind & HSLIDE) {
            publish(handle, WF_HSLSIZ, horizontal.size(), state->hsize);
            publish(handle, WF_HSLIDE, horizontal.position(), state->hslide);
        }
        if (state->scroll_kind & VSLIDE) {
            publish(handle, WF_VSLSIZ, vertical.size(), state->vsize);
            publish(handle, WF_VSLIDE, vertical.position(), state->vslide);
        }
    }
}

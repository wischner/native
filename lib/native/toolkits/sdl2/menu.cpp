//
// Implements the SDL2 menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

namespace linux::sdl2 {

static int text_width_est(const std::string &s) {
    return linux::sdl2::text_width(s) + 16;
}

static const int menu_item_height  = 20;
static const int popup_width = 180;

static int hit_top_index(sdl2_menu *m, int x) {
    if (!m)
        return -1;

    for (int i = 0; i < static_cast<int>(m->tops.size()); ++i) {
        const auto &top = m->tops[i];
        if (x >= top.x0 && x < top.x1)
            return i;
    }

    return -1;
}

static int hit_popup_item_index(sdl2_menu *m, int x, int y) {
    if (!m || m->open_idx < 0 || m->open_idx >= static_cast<int>(m->tops.size()))
        return -1;

    const auto &top = m->tops[m->open_idx];
    const int popup_h = static_cast<int>(top.items.size()) * menu_item_height + 2;

    if (!(x >= m->popup_x && x < m->popup_x + popup_width &&
          y >= m->popup_y && y < m->popup_y + popup_h))
        return -1;

    const int idx = (y - (m->popup_y + 1)) / menu_item_height;
    if (idx < 0 || idx >= static_cast<int>(top.items.size()))
        return -1;
    return idx;
}

// ---------------------------------------------------------------------------
// Public render_menu — called from app.cpp render loop
// ---------------------------------------------------------------------------

void render_menu(
    sdl2_menu *m,
    native::gpx &g,
    int win_w,
    int /*win_h*/) {
    if (!m) return;

    auto painter = native::theme::create(g);
    painter->draw_menu_bar(native::rect(
        0,
        0,
        static_cast<native::dim>(win_w),
        static_cast<native::dim>(menu_bar_height)));

    for (int i = 0; i < static_cast<int>(m->tops.size()); ++i) {
        auto &top = m->tops[i];
        native::theme::state st;
        st.selected = (m->open_idx == i);
        st.hot = (m->hover_top == i);
        painter->draw_menu_title(
            native::rect(
                top.x0,
                1,
                static_cast<native::dim>(top.x1 - top.x0),
                static_cast<native::dim>(menu_bar_height - 2)),
            top.title,
            st);
    }

    // Draw open popup
    if (m->open_idx >= 0 && m->open_idx < static_cast<int>(m->tops.size())) {
        auto &top   = m->tops[m->open_idx];
        int popup_h = static_cast<int>(top.items.size()) * menu_item_height + 2;
        painter->draw_popup_frame(native::rect(
            m->popup_x,
            m->popup_y,
            static_cast<native::dim>(popup_width),
            static_cast<native::dim>(popup_h)));

        for (int i = 0; i < static_cast<int>(top.items.size()); ++i) {
            native::theme::state st;
            st.selected = (m->hover_item == i);
            st.hot = (m->hover_item == i);
            painter->draw_menu_item(native::rect(m->popup_x + 1, m->popup_y + 1 + i * menu_item_height,
                                           static_cast<native::dim>(popup_width - 2),
                                           static_cast<native::dim>(menu_item_height)),
                              top.items[i].second,
                              st);
        }
    }
}

// ---------------------------------------------------------------------------
// handle_menu_motion — updates hover/open menu tracking
// ---------------------------------------------------------------------------

bool handle_menu_motion(sdl2_menu *m, int x, int y, int /*win_w*/) {
    if (!m) return false;

    const int old_hover_top = m->hover_top;
    const int old_hover_item = m->hover_item;
    const int old_open_idx = m->open_idx;

    if (y >= 0 && y < menu_bar_height) {
        m->hover_top = hit_top_index(m, x);
        if (m->open_idx >= 0 && m->hover_top >= 0 && m->hover_top != m->open_idx) {
            m->open_idx = m->hover_top;
            m->popup_x = m->tops[m->open_idx].x0;
            m->popup_y = menu_bar_height;
        }
    }
    else {
        m->hover_top = -1;
    }

    m->hover_item = hit_popup_item_index(m, x, y);

    return m->hover_top != old_hover_top ||
           m->hover_item != old_hover_item ||
           m->open_idx != old_open_idx;
}

// ---------------------------------------------------------------------------
// handle_menu_click — returns true if click was consumed by menu
// ---------------------------------------------------------------------------

bool handle_menu_click(sdl2_menu *m, int x, int y, int /*win_w*/) {
    if (!m) return false;

    // Click in menu bar area
    if (y >= 0 && y < menu_bar_height) {
        int found = hit_top_index(m, x);
        if (found >= 0) {
            if (m->open_idx == found) {
                m->open_idx = -1;
                m->hover_item = -1;
            }
            else {
                m->open_idx = found;
                m->popup_x  = m->tops[found].x0;
                m->popup_y  = menu_bar_height;
                m->hover_item = -1;
            }
            return true;
        }

        // Click in bar but not on a title — close any open menu
        m->open_idx = -1;
        m->hover_item = -1;
        return true;
    }

    // Click in open popup area
    if (m->open_idx >= 0) {
        auto &top = m->tops[m->open_idx];
        const int item_idx = hit_popup_item_index(m, x, y);
        if (item_idx >= 0 && item_idx < static_cast<int>(top.items.size())) {
            int item_id  = top.items[item_idx].first;
            m->open_idx  = -1;
            m->hover_item = -1;
            if (m->owner)
                m->owner->on_menu.emit(item_id);
            return true;
        }

        // Click outside popup — close it
        m->open_idx = -1;
        m->hover_item = -1;
        return false;
    }

    return false;
}

} // namespace linux::sdl2

// ---------------------------------------------------------------------------
// native::main_menu platform implementation for SDL2
// ---------------------------------------------------------------------------

namespace native {

main_menu::~main_menu() {
    detach();
}

void main_menu::detach() {
    if (!_id) {
        _owner = nullptr;
        return;
    }

    auto *m = linux::sdl2::menu_bindings.object_from_handle(_id);
    if (m) delete m;
    linux::sdl2::menu_bindings.unregister_by_handle(_id);
    _id = 0;
    _owner = nullptr;
}

void main_menu::attach(app_wnd &owner) {
    if (_id || _tops.empty()) return;
    _owner = &owner;

    auto *sm  = new linux::sdl2::sdl2_menu();
    sm->owner = &owner;

    int x = 0;
    for (const auto &top : _tops) {
        linux::sdl2::sdl2_menu::top_entry te;
        te.title = top.title;
        te.x0    = x;
        te.x1    = x + linux::sdl2::text_width_est(top.title);
        x        = te.x1;
        for (const auto &item : top.items)
            te.items.push_back({item.id, item.label});
        sm->tops.push_back(std::move(te));
    }

    _id = next_id();
    linux::sdl2::menu_bindings.register_pair(_id, sm);
}

} // namespace native

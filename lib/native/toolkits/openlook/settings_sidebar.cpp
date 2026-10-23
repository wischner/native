//
// Composes native XView settings navigation with labeled search,
// indented rows and bold inactive section headings.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../picker_peer.h"
#include <xview/font.h>
namespace
{
    using namespace native;
    struct navigation final : detail::settings_sidebar_resources
    {
        settings_sidebar &owner;
        Panel_item search = XV_NULL, list = XV_NULL;
        std::vector<int> ids;
        std::vector<settings_page> items;
        bool updating = false;
        explicit navigation(settings_sidebar &value)
            : owner(value) {}
        void close() noexcept override {
            if (search) {
                xv_set(search, PANEL_CLIENT_DATA, nullptr, nullptr);
                xv_destroy_safe(search);
            }
            if (list) {
                xv_set(list, PANEL_CLIENT_DATA, nullptr, nullptr);
                xv_destroy_safe(list);
            }
            search = list = XV_NULL;
        }
    };
    Panel_setting search_changed(Panel_item item, Event *event) {
        auto *state = reinterpret_cast<navigation *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (!state || state->updating)
            return PANEL_NONE;
        const detail::window_guard guard(state->owner);
        auto *owner = &state->owner;
        app::post([owner, guard] {
            if (!guard.alive())
                return;
            auto *state = static_cast<navigation *>(
                detail::peer_state<detail::settings_sidebar_resources>(
                    *owner));
            const char *text = reinterpret_cast<const char *>(
                xv_get(state->search, PANEL_VALUE));
            owner->on_native_search(text ? text : "");
        });
        return panel_text_notify(item, event);
    }
    int selected(Panel_item item,
                 char *,
                 Xv_opaque,
                 Panel_list_op operation,
                 Event *,
                 int row) {
        auto *state = reinterpret_cast<navigation *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (state && !state->updating &&
            operation == PANEL_LIST_OP_SELECT && row >= 0 &&
            row < int(state->ids.size()) && state->ids[row])
            state->owner.on_native_selection(state->ids[row]);
        return XV_OK;
    }
} // namespace
namespace native::detail
{
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        auto state = std::make_unique<navigation>(owner);
        const auto panel =
            linux::openlook::wnd_bindings.handle_from_object(&owner);
        state->search = xv_create(panel,
                                  PANEL_TEXT,
                                  PANEL_LABEL_STRING,
                                  "Search:",
                                  PANEL_NOTIFY_LEVEL,
                                  PANEL_ALL,
                                  PANEL_NOTIFY_PROC,
                                  search_changed,
                                  PANEL_CLIENT_DATA,
                                  state.get(),
                                  XV_SHOW,
                                  FALSE,
                                  nullptr);
        state->list = xv_create(panel,
                                PANEL_LIST,
                                PANEL_CHOOSE_ONE,
                                TRUE,
                                PANEL_CHOOSE_NONE,
                                TRUE,
                                PANEL_NOTIFY_PROC,
                                selected,
                                PANEL_CLIENT_DATA,
                                state.get(),
                                XV_SHOW,
                                FALSE,
                                nullptr);
        std::unique_ptr<settings_sidebar_resources> base =
            std::move(state);
        assign_peer_state(owner, base.release());
        return true;
    }
    bool settings_sidebar_uses_native_search() {
        return true;
    }
    bool settings_sidebar_uses_list() {
        return false;
    }
    int settings_sidebar_navigation_padding() {
        return 6;
    }
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *state = static_cast<navigation *>(
            peer_state<settings_sidebar_resources>(owner));
        if (!state)
            return;
        state->updating = true;
        const auto bounds = settings_navigation_bounds(owner);
        const auto visible = settings_visible_items(owner);
        bool rebuild = visible.size() != state->items.size();
        for (std::size_t i = 0; !rebuild && i < visible.size(); ++i)
            rebuild = visible[i].id != state->items[i].id ||
                      visible[i].title != state->items[i].title ||
                      visible[i].section != state->items[i].section ||
                      visible[i].enabled != state->items[i].enabled;
        if (rebuild) {
            const int count = xv_get(state->list, PANEL_LIST_NROWS);
            if (count)
                xv_set(state->list,
                       PANEL_LIST_DELETE_ROWS,
                       0,
                       count,
                       nullptr);
            state->ids.clear();
            const auto panel =
                linux::openlook::wnd_bindings.handle_from_object(
                    &owner);
            const auto bold = xv_get(panel, PANEL_BOLD_FONT);
            const auto *metrics = bold
                                      ? reinterpret_cast<XFontStruct *>(
                                            xv_get(bold, FONT_INFO))
                                      : nullptr;
            const int font_height =
                metrics ? metrics->ascent + metrics->descent : 0;
            xv_set(state->list,
                   PANEL_LIST_ROW_HEIGHT,
                   std::max(20, font_height + 4),
                   nullptr);
            const int row_height =
                xv_get(state->list, PANEL_LIST_ROW_HEIGHT);
            const auto add =
                [&](const std::string &text, int id, bool enabled) {
                    const int row = state->ids.size();
                    state->ids.push_back(id);
                    xv_set(state->list,
                           PANEL_LIST_INSERT,
                           row,
                           PANEL_LIST_STRING,
                           row,
                           text.c_str(),
                           PANEL_LIST_INACTIVE,
                           row,
                           !enabled,
                           nullptr);
                    if (!id && bold && font_height <= row_height)
                        xv_set(state->list,
                               PANEL_LIST_FONT,
                               row,
                               bold,
                               nullptr);
                };
            std::string section;
            for (const auto &item : visible) {
                if (!item.section.empty() && section != item.section)
                    add(item.section, 0, false);
                section = item.section;
                add("  " + item.title, item.id, item.enabled);
            }
            state->items = visible;
        }
        const auto scrollbar =
            xv_get(state->list, PANEL_LIST_SCROLLBAR);
        const int scrollbar_width =
            scrollbar ? int(xv_get(scrollbar, XV_WIDTH)) : 0;
        const int row_height = std::max(
            1, int(xv_get(state->list, PANEL_LIST_ROW_HEIGHT)));
        const int rows = xv_get(state->list, PANEL_LIST_DISPLAY_ROWS);
        const int chrome = std::max(
            0, int(xv_get(state->list, XV_HEIGHT)) - rows * row_height);
        xv_set(state->list,
               XV_X,
               bounds.x1(),
               XV_Y,
               bounds.y1(),
               PANEL_LIST_WIDTH,
               std::max(1, int(bounds.w()) - scrollbar_width),
               PANEL_LIST_DISPLAY_ROWS,
               std::max(1, (int(bounds.h()) - chrome) / row_height),
               XV_SHOW,
               TRUE,
               nullptr);
        for (int i = 0; i < int(state->ids.size()); ++i)
            xv_set(state->list,
                   PANEL_LIST_SELECT,
                   i,
                   state->ids[i] &&
                       state->ids[i] == owner.get_selected_item(),
                   nullptr);
        const auto text = reinterpret_cast<const char *>(
            xv_get(state->search, PANEL_VALUE));
        if (owner.get_search_text() != (text ? text : ""))
            xv_set(state->search,
                   PANEL_VALUE,
                   owner.get_search_text().c_str(),
                   nullptr);
        xv_set(state->search,
               XV_X,
               bounds.x1(),
               XV_Y,
               6,
               PANEL_VALUE_DISPLAY_WIDTH,
               std::max(1, int(bounds.w()) - 60),
               XV_SHOW,
               owner.get_search_visible(),
               nullptr);
        state->updating = false;
    }
} // namespace native::detail

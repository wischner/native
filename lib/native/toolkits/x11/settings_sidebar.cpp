//
// Composes settings navigation from native Athena Toggle and Label
// rows. Disabled headings/items use Xaw's stippled text; selection
// fills each row.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "scale.h"
#include "../../picker_peer.h"
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Toggle.h>
#include <algorithm>

namespace
{
    struct navigation_state final
        : native::detail::settings_sidebar_resources
    {
        struct row
        {
            native::settings_sidebar *owner;
            int id;
            Widget widget;
        };
        Widget host = nullptr;
        std::vector<std::unique_ptr<row>> rows;
        std::vector<native::settings_page> items;
        void close() noexcept override {
            if (host)
                XtDestroyWidget(host);
            host = nullptr;
            rows.clear();
        }
    };
} // namespace
namespace native::detail
{
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        auto state = std::make_unique<navigation_state>();
        state->host = XtVaCreateManagedWidget(
            "settingsNavigation",
            linux::x11::layout_host_class(),
            linux::x11::wnd_bindings.handle_from_object(&owner),
            XtNborderWidth,
            1,
            XtNdefaultDistance,
            0,
            nullptr);
        std::unique_ptr<settings_sidebar_resources> resources =
            std::move(state);
        assign_peer_state(owner, resources.release());
        return true;
    }
    bool settings_sidebar_uses_native_search() {
        return false;
    }
    bool settings_sidebar_uses_list() {
        return false;
    }
    int settings_sidebar_navigation_padding() {
        return 6;
    }
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *state = static_cast<navigation_state *>(
            peer_state<settings_sidebar_resources>(owner));
        if (!state || !state->host)
            return;
        const auto visible = settings_visible_items(owner);
        const bool rebuild =
            state->items.size() != visible.size() ||
            !std::equal(visible.begin(),
                        visible.end(),
                        state->items.begin(),
                        [](const auto &a, const auto &b) {
                            return a.id == b.id && a.title == b.title &&
                                   a.section == b.section &&
                                   a.enabled == b.enabled;
                        });
        if (rebuild) {
            for (auto &row : state->rows)
                XtDestroyWidget(row->widget);
            state->rows.clear();
            std::string section;
            const auto add = [&](int id,
                                 const std::string &text,
                                 bool enabled,
                                 int indent) {
                auto row = std::make_unique<navigation_state::row>();
                row->owner = &owner;
                row->id = id;
                row->widget = XtVaCreateManagedWidget(
                    id ? "settingsItem" : "settingsHeading",
                    id ? toggleWidgetClass : labelWidgetClass,
                    state->host,
                    XtNlabel,
                    text.c_str(),
                    XtNsensitive,
                    enabled,
                    XtNjustify,
                    XtJustifyLeft,
                    XtNinternalWidth,
                    linux::x11::to_device(indent),
                    XtNborderWidth,
                    0,
                    XtNresize,
                    False,
                    nullptr);
                if (id)
                    XtAddCallback(
                        row->widget,
                        XtNcallback,
                        +[](Widget, XtPointer data, XtPointer) {
                            const auto row =
                                *static_cast<navigation_state::row *>(
                                    data);
                            row.owner->on_native_selection(row.id);
                        },
                        row.get());
                state->rows.push_back(std::move(row));
            };
            for (const auto &page : visible) {
                if (!page.section.empty() && page.section != section)
                    add(0, page.section, false, 6);
                section = page.section;
                add(page.id,
                    page.title,
                    page.enabled,
                    section.empty() ? 6 : 18);
            }
            state->items = visible;
        }
        const auto bounds = linux::x11::rect_to_device(
            settings_navigation_bounds(owner));
        XtVaSetValues(state->host,
                      XtNhorizDistance,
                      bounds.x1(),
                      XtNvertDistance,
                      bounds.y1(),
                      XtNwidth,
                      std::max(1, int(bounds.w()) - 2),
                      XtNheight,
                      std::max(1, int(bounds.h()) - 2),
                      nullptr);
        const int row_height = linux::x11::to_device(
            theme::create(owner.get_gpx())->get_text_edit_height());
        int top = 0;
        for (const auto &row : state->rows) {
            XtVaSetValues(row->widget,
                          XtNhorizDistance,
                          0,
                          XtNvertDistance,
                          top,
                          XtNwidth,
                          std::max(1, int(bounds.w()) - 2),
                          XtNheight,
                          row_height,
                          nullptr);
            if (row->id)
                XtVaSetValues(row->widget,
                              XtNstate,
                              row->id == owner.get_selected_item(),
                              nullptr);
            top += row_height;
        }
    }
} // namespace native::detail

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
#include <native/text_edit.h>
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
        Widget prompt = nullptr;
        std::unique_ptr<native::text_edit> search;
        std::vector<std::unique_ptr<row>> rows;
        std::vector<native::settings_page> items;
        void close() noexcept override {
            search.reset();
            if (prompt)
                XtDestroyWidget(prompt);
            prompt = nullptr;
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
        state->prompt = XtVaCreateWidget(
            "searchPrompt",
            labelWidgetClass,
            linux::x11::wnd_bindings.handle_from_object(&owner),
            XtNlabel,
            "Search:",
            XtNborderWidth,
            0,
            XtNinternalWidth,
            0,
            XtNinternalHeight,
            0,
            XtNjustify,
            XtJustifyLeft,
            XtNresize,
            False,
            nullptr);
        state->search = std::make_unique<text_edit>();
        state->search->set_parent(&owner);
        state->search->on_change.connect(
            [&owner](const std::string &text) {
                owner.on_native_search(text);
                return true;
            });
        std::unique_ptr<settings_sidebar_resources> resources =
            std::move(state);
        assign_peer_state(owner, resources.release());
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
        const auto area = owner.get_client_bounds();
        const int column =
            std::min(owner.get_sidebar_width(), int(area.w()));
        const int padding =
            std::min({6, column / 2, int(area.h()) / 2});
        const int input_height =
            theme::create(owner.get_gpx())->get_text_edit_height();
        const int search_height =
            owner.get_search_visible() ? input_height + padding : 0;
        const int width = std::max(0, column - 2 * padding);
        if (owner.get_search_visible()) {
            const int prompt_width = font_t::stock(font_role::control)
                                         .measure_text("Search:")
                                         .width +
                                     padding;
            const auto row = linux::x11::rect_to_device(
                rect(area.x1() + padding,
                     area.y1() + padding,
                     std::min(prompt_width, width),
                     input_height));
            XtVaSetValues(state->prompt,
                          XtNhorizDistance,
                          row.x1(),
                          XtNvertDistance,
                          row.y1(),
                          XtNwidth,
                          std::max(1, int(row.w())),
                          XtNheight,
                          std::max(1, int(row.h())),
                          nullptr);
            const rect desired(area.x1() + padding +
                                   std::min(prompt_width, width),
                               area.y1() + padding,
                               std::max(0, width - prompt_width),
                               input_height);
            const auto current = state->search->get_bounds();
            if (current.x1() != desired.x1() ||
                current.y1() != desired.y1() ||
                current.w() != desired.w() ||
                current.h() != desired.h())
                state->search->set_bounds(desired);
            if (state->search->get_text() != owner.get_search_text())
                state->search->set_text(owner.get_search_text());
            if (!state->search->get_created())
                state->search->create();
            if (!state->search->get_visible())
                state->search->show();
            XtManageChild(state->prompt);
        } else {
            state->search->destroy();
            XtUnmanageChild(state->prompt);
        }
        const auto bounds = linux::x11::rect_to_device(rect(
            area.x1() + padding,
            area.y1() + padding + search_height,
            width,
            std::max(0, int(area.h()) - 2 * padding - search_height)));
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

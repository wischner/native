//
// Uses a native WINGs list and search field for settings navigation.
// Section rows are inactive bold labels; page rows retain optional
// images without suggesting that these fixed groups can be collapsed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../picker_peer.h"
#include "../../platforms/linux/x_image.h"
#include <WINGs/WINGsP.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

namespace
{
    using namespace native;
    struct navigation_row
    {
        int id = 0;
        bool enabled = false;
        std::shared_ptr<img> icon;
    };
    struct navigation final : detail::settings_sidebar_resources
    {
        settings_sidebar &owner;
        WMList *list = nullptr;
        WMTextField *search = nullptr;
        WMLabel *prompt = nullptr;
        WMTextFieldDelegate delegate{};
        std::vector<settings_page> items;
        std::vector<navigation_row> rows;
        bool updating = false;
        explicit navigation(settings_sidebar &value)
            : owner(value) {}
        ~navigation() override {
            close();
        }
        // Release native children before the owning structural frame.
        void close() noexcept override {
            if (search)
                WMSetTextFieldDelegate(search, nullptr);
            if (list)
                WMDestroyWidget(list);
            if (search)
                WMDestroyWidget(search);
            if (prompt)
                WMDestroyWidget(prompt);
            list = nullptr;
            search = nullptr;
            prompt = nullptr;
            rows.clear();
            items.clear();
        }
    };
    // WINGs owns scrolling, hit testing, selection and the surrounding
    // frame.
    void draw_row(WMList *list,
                  int index,
                  Drawable target,
                  char *text,
                  int flags,
                  WMRect *bounds) {
        const auto *item = WMGetListItem(list, index);
        const auto *entry =
            item ? static_cast<navigation_row *>(item->clientData)
                 : nullptr;
        if (!entry || !bounds)
            return;
        auto *screen =
            reinterpret_cast<W_Screen *>(WMWidgetScreen(list));
        const bool selected = entry->id && (flags & WLDSSelected);
        auto *paper = selected
                          ? linux::wmaker::list_selection_background
                          : screen->white;
        auto *ink = selected ? linux::wmaker::list_selection_text
                    : !entry->id || entry->enabled ? screen->black
                                                   : screen->darkGray;
        XFillRectangle(screen->display,
                       target,
                       WMColorGC(paper),
                       bounds->pos.x,
                       bounds->pos.y,
                       bounds->size.width,
                       bounds->size.height);
        int x = bounds->pos.x + (entry->id ? 8 : 4);
        if (entry->icon) {
            detail::blend_x_image(
                screen->display,
                target,
                WMColorGC(ink),
                *entry->icon,
                point(x,
                      bounds->pos.y +
                          (int(bounds->size.height) - 16) / 2),
                rect(bounds->pos.x,
                     bounds->pos.y,
                     bounds->size.width,
                     bounds->size.height),
                size(WMWidgetWidth(list), WMWidgetHeight(list)));
            x += 20;
        }
        auto *font = entry->id ? screen->normalFont : screen->boldFont;
        W_PaintText(
            WMWidgetView(list),
            target,
            font,
            x,
            bounds->pos.y +
                (int(bounds->size.height) - WMFontHeight(font)) / 2,
            std::max(0,
                     int(bounds->size.width) - x + bounds->pos.x - 4),
            WALeft,
            ink,
            False,
            text,
            std::strlen(text));
    }
    // Defer page changes until native list input dispatch has returned.
    void selected(WMWidget *, void *data) {
        auto &state = *static_cast<navigation *>(data);
        if (state.updating ||
            !linux::wmaker::permit_input(&state.owner))
            return;
        const int index = WMGetListSelectedItemRow(state.list);
        if (index < 0 || index >= int(state.rows.size()))
            return;
        const auto entry = state.rows[index];
        if (!entry.id || !entry.enabled)
            return;
        const detail::window_guard guard(state.owner);
        linux::wmaker::defer(
            [owner = &state.owner, guard, id = entry.id] {
                if (guard.alive())
                    owner->on_native_selection(id);
            });
    }
    // Filter through the public model without rebuilding a live text
    // delegate.
    void searched(WMTextFieldDelegate *delegate, WMNotification *) {
        auto &state = *static_cast<navigation *>(delegate->data);
        if (state.updating)
            return;
        char *value = WMGetTextFieldText(state.search);
        const std::string text = value ? value : "";
        std::free(value);
        const detail::window_guard guard(state.owner);
        linux::wmaker::defer([owner = &state.owner, guard, text] {
            if (guard.alive())
                owner->on_native_search(text);
        });
    }
    // Preserve native entry selection/caret across filtering refreshes.
    void set_search(navigation &state) {
        char *value = WMGetTextFieldText(state.search);
        const bool changed =
            state.owner.get_search_text() != (value ? value : "");
        std::free(value);
        if (changed)
            WMSetTextFieldText(state.search,
                               state.owner.get_search_text().c_str());
    }
} // namespace
namespace native::detail
{
    // Own all native controls through the sidebar's opaque resource
    // peer.
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        auto state = std::make_unique<navigation>(owner);
        auto *parent =
            linux::wmaker::wnd_bindings.handle_from_object(&owner);
        state->list = WMCreateList(parent);
        state->search = WMCreateTextField(parent);
        state->prompt = WMCreateLabel(parent);
        if (!state->list || !state->search || !state->prompt)
            throw std::runtime_error(
                "Window Maker: cannot create settings controls.");
        WMSetLabelText(state->prompt, "Search:");
        WMSetLabelTextAlignment(state->prompt, WALeft);
        WMSetListAllowMultipleSelection(state->list, False);
        WMSetListAllowEmptySelection(state->list, True);
        WMSetListUserDrawProc(state->list, draw_row);
        WMSetListAction(state->list, selected, state.get());
        auto *screen =
            reinterpret_cast<W_Screen *>(WMWidgetScreen(parent));
        WMSetWidgetBackgroundColor(state->list, screen->white);
        WMSetListUserDrawItemHeight(
            state->list,
            std::max(22, int(WMFontHeight(screen->normalFont)) + 8));
        W_SetViewCursor(WMWidgetView(state->search),
                        screen->textCursor);
        state->delegate.data = state.get();
        state->delegate.didChange = searched;
        WMSetTextFieldDelegate(state->search, &state->delegate);
        for (WMWidget *child : {static_cast<WMWidget *>(state->list),
                                static_cast<WMWidget *>(state->search),
                                static_cast<WMWidget *>(state->prompt)})
            WMRealizeWidget(child);
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
    // Rebuild changed rows and preserve native search caret/selection.
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *state = static_cast<navigation *>(
            peer_state<settings_sidebar_resources>(owner));
        if (!state)
            return;
        state->updating = true;
        const auto visible = settings_visible_items(owner);
        bool rebuild = visible.size() != state->items.size();
        for (std::size_t i = 0; !rebuild && i < visible.size(); ++i)
            rebuild = visible[i].id != state->items[i].id ||
                      visible[i].title != state->items[i].title ||
                      visible[i].section != state->items[i].section ||
                      visible[i].image != state->items[i].image ||
                      visible[i].enabled != state->items[i].enabled;
        if (rebuild) {
            WMClearList(state->list);
            state->rows.clear();
            state->rows.reserve(visible.size() * 2);
            const auto add = [&](const std::string &title,
                                 navigation_row entry) {
                state->rows.push_back(std::move(entry));
                auto *item = WMAddListItem(state->list, title.c_str());
                item->clientData = &state->rows.back();
                item->disabled = !state->rows.back().enabled;
            };
            std::string section;
            for (const auto &page : visible) {
                if (!page.section.empty() && page.section != section)
                    add(page.section, {});
                section = page.section;
                std::shared_ptr<img> icon;
                if (page.image) {
                    icon = std::make_shared<img>(16, 16);
                    icon->get_gpx()
                        .clear(rgba(0, 0, 0, 0))
                        .draw_img(*page.image, rect(0, 0, 16, 16));
                }
                add(page.title,
                    {page.id, page.enabled, std::move(icon)});
            }
            state->items = visible;
        }
        int selection = -1;
        for (std::size_t i = 0; i < state->rows.size(); ++i)
            if (state->rows[i].id == owner.get_selected_item() &&
                state->rows[i].id)
                selection = i;
        if (WMGetListSelectedItemRow(state->list) != selection) {
            WMUnselectAllListItems(state->list);
            if (selection >= 0)
                WMSelectListItem(state->list, selection);
        }
        const auto area = settings_navigation_bounds(owner);
        WMMoveWidget(state->list, area.x1(), area.y1());
        WMResizeWidget(state->list,
                       std::max(1, int(area.w())),
                       std::max(1, int(area.h())));
        WMMapWidget(state->list);
        const int height =
            theme::create(owner.get_gpx())->get_text_edit_height();
        const int prompt = font_t::stock(font_role::control)
                               .measure_text("Search:")
                               .width +
                           6;
        WMMoveWidget(state->prompt, area.x1(), 6);
        WMResizeWidget(state->prompt, prompt, height);
        WMMoveWidget(state->search, area.x1() + prompt, 6);
        WMResizeWidget(
            state->search, std::max(1, int(area.w()) - prompt), height);
        set_search(*state);
        if (owner.get_search_visible()) {
            WMMapWidget(state->prompt);
            WMMapWidget(state->search);
        } else {
            WMUnmapWidget(state->prompt);
            WMUnmapWidget(state->search);
        }
        state->updating = false;
    }
} // namespace native::detail

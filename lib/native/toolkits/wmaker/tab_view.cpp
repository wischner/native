// Implements tab_view with Window Maker's native WINGs WMTabView.

#include <stdexcept>
#include <WINGs/WINGs.h>
#include <native.h>
#include "../../control_render_access.h"
#include "collection_host.h"
#include "globals.h"

namespace
{
    linux::wmaker::native_tab_view *binding(native::tab_view &owner) {
        return linux::wmaker::tab_view_bindings.object_from_handle(&owner);
    }

    void selected(WMTabViewDelegate *delegate,
                  WMTabView *,
                  WMTabViewItem *item) {
        auto *owner = delegate
            ? static_cast<native::tab_view *>(delegate->data)
            : nullptr;
        auto *state = owner ? binding(*owner) : nullptr;
        if (owner && state && !state->suppress && item)
            owner->on_native_selection(WMGetTabViewItemIdentifier(item));
    }

    Bool may_select(WMTabViewDelegate *delegate,
                    WMTabView *,
                    WMTabViewItem *item) {
        auto *owner = delegate
            ? static_cast<native::tab_view *>(delegate->data)
            : nullptr;
        const int index = item ? WMGetTabViewItemIdentifier(item) : -1;
        return owner && index >= 0 &&
               static_cast<std::size_t>(index) < owner->get_item_count() &&
               owner->get_item(static_cast<std::size_t>(index)).get_enabled();
    }

    void destroy_borrowed_contents(native::tab_view &owner) {
        for (std::size_t index = 0; index < owner.get_item_count(); ++index) {
            native::wnd &content = owner.get_item(index).get_content();
            if (content.get_created()) content.destroy();
        }
    }

    void destroy_host(native::tab_view &owner,
                      linux::wmaker::native_tab_view &state) {
        destroy_borrowed_contents(owner);
        linux::wmaker::wnd_bindings.unregister_by_object(&owner);
        if (state.tabs) {
            WMDestroyWidget(state.tabs);
            state.tabs = nullptr;
        }
        if (state.portable) {
            if (state.portable->frame)
                WMDestroyWidget(state.portable->frame);
            delete state.portable;
            state.portable = nullptr;
        }
        state.items.clear();
        state.pages.clear();
    }

    void create_host(native::tab_view &owner,
                     linux::wmaker::native_tab_view &state) {
        WMScreen *screen = linux::wmaker::screen;
        WMFont *font = screen ? WMDefaultSystemFont(screen) : nullptr;
        const int height = font ? WMFontHeight(font) + 3 : 20;
        native::detail::control_render_access::configure_tab_layout(
            owner,
            height,
            8,
            30,
            10,
            1,
            2,
            1,
            owner.get_tab_placement() !=
                native::tab_placement::top,
            native::rgba(132, 132, 132, 255),
            native::rgba(217, 217, 217, 255));
        if (owner.get_tab_placement() != native::tab_placement::top ||
            !owner.get_page_frame_visible()) {
            state.portable =
                linux::wmaker::create_collection_frame(owner);
            return;
        }

        state.tabs = WMCreateTabView(
            linux::wmaker::parent_widget(&owner));
        if (!state.tabs)
            throw std::runtime_error(
                "Window Maker/WINGs: failed to create WMTabView.");
        const native::point position =
            linux::wmaker::control_position(&owner);
        const native::size dimensions = owner.get_dimensions();
        WMMoveWidget(state.tabs, position.x, position.y);
        WMResizeWidget(state.tabs, dimensions.w, dimensions.h);
        state.delegate.data = &owner;
        state.delegate.didSelectItem = selected;
        state.delegate.shouldSelectItem = may_select;
        WMSetTabViewDelegate(state.tabs, &state.delegate);
        linux::wmaker::wnd_bindings.register_pair(state.tabs, &owner);
    }
}

namespace native
{
    void tab_view::apply_items() {
        auto *state = binding(*this);
        if (!state)
            throw std::runtime_error("Window Maker/WINGs: missing tab view.");
        delete _gpx;
        _gpx = nullptr;
        destroy_host(*this, *state);
        create_host(*this, *state);
        state->suppress = true;
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            WMFrame *page = WMCreateFrame(
                state->tabs
                    ? reinterpret_cast<WMWidget *>(state->tabs)
                    : reinterpret_cast<WMWidget *>(state->portable->frame));
            WMSetFrameRelief(page, WRFlat);
            if (state->tabs) {
                WMTabViewItem *item = WMCreateTabViewItemWithIdentifier(
                    static_cast<int>(index));
                WMSetTabViewItemView(item, WMWidgetView(page));
                WMSetTabViewItemLabel(
                    item, get_item(index).get_title().c_str());
                WMSetTabViewItemEnabled(
                    item, get_item(index).get_enabled());
                WMAddItemInTabView(state->tabs, item);
                state->items.push_back(item);
            } else {
                const rect content = get_content_bounds();
                WMMoveWidget(page, content.p.x, content.p.y);
                WMResizeWidget(page, content.d.w, content.d.h);
            }
            state->pages.push_back(page);
        }
        // refresh_contents() creates and shows the selected page's
        // borrowed control immediately after apply_items().  WINGs cannot
        // realize that control until both the tab host and its page frame
        // have been realized, even though the top-level window already is.
        WMWidget *host = state->tabs
            ? reinterpret_cast<WMWidget *>(state->tabs)
            : reinterpret_cast<WMWidget *>(state->portable->frame);
        WMRealizeWidget(host);
        state->suppress = false;
    }

    void tab_view::apply_selected_index() {
        auto *state = binding(*this);
        if (!state)
            throw std::runtime_error("Window Maker/WINGs: missing tab view.");
        if (get_selected_index() < 0) return;
        state->suppress = true;
        if (state->tabs) {
            WMSelectTabViewItemAtIndex(
                state->tabs, get_selected_index());
        } else {
            for (std::size_t index = 0;
                 index < state->pages.size(); ++index) {
                if (static_cast<int>(index) == get_selected_index()) {
                    WMMapWidget(state->pages[index]);
                    WMRaiseWidget(state->pages[index]);
                } else {
                    WMUnmapWidget(state->pages[index]);
                }
            }
        }
        state->suppress = false;
    }

    void tab_view::create_native() {
        auto *self = this;
        auto *state = new linux::wmaker::native_tab_view();
        linux::wmaker::tab_view_bindings.register_pair(self, state);
        self->configure_page_host(true, true);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void tab_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state || (!state->tabs && !state->portable))
            throw std::runtime_error("Window Maker/WINGs: tab_view is not created.");
        WMWidget *host = state->tabs
            ? reinterpret_cast<WMWidget *>(state->tabs)
            : reinterpret_cast<WMWidget *>(state->portable->frame);
        WMRealizeWidget(host);
        WMMapWidget(host);
        const int selected_index = get_selected_index();
        if (selected_index >= 0) {
            wnd &content = get_item(static_cast<std::size_t>(selected_index)).get_content();
            if (content.get_created()) content.show();
        }
    }

    void tab_view::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = binding(*self);
        if (state) {
            destroy_host(*self, *state);
            linux::wmaker::tab_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

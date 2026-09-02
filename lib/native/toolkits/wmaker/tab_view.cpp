// Implements tab_view with Window Maker's native WINGs WMTabView.

#include <stdexcept>
#include <WINGs/WINGs.h>
#include <native.h>
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

    void clear_native_items(native::tab_view &owner,
                            linux::wmaker::native_tab_view &state) {
        for (std::size_t index = 0; index < owner.get_item_count(); ++index) {
            native::wnd &content = owner.get_item(index).get_content();
            if (content.get_created()) content.destroy();
        }
        for (WMTabViewItem *item : state.items) {
            WMRemoveTabViewItem(state.tabs, item);
            WMDestroyTabViewItem(item);
        }
        state.items.clear();
        state.pages.clear();
    }
}

namespace native
{
    void tab_view::apply_items() {
        auto *state = binding(*this);
        if (!state || !state->tabs)
            throw std::runtime_error("Window Maker/WINGs: missing tab view.");
        state->suppress = true;
        clear_native_items(*this, *state);
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            WMFrame *page = WMCreateFrame(state->tabs);
            WMSetFrameRelief(page, WRFlat);
            WMTabViewItem *item = WMCreateTabViewItemWithIdentifier(
                static_cast<int>(index));
            WMSetTabViewItemView(item, WMWidgetView(page));
            WMSetTabViewItemLabel(item, get_item(index).get_title().c_str());
            WMSetTabViewItemEnabled(item, get_item(index).get_enabled());
            WMAddItemInTabView(state->tabs, item);
            state->pages.push_back(page);
            state->items.push_back(item);
        }
        state->suppress = false;
    }

    void tab_view::apply_selected_index() {
        auto *state = binding(*this);
        if (!state || !state->tabs)
            throw std::runtime_error("Window Maker/WINGs: missing tab view.");
        if (get_selected_index() < 0) return;
        state->suppress = true;
        WMSelectTabViewItemAtIndex(state->tabs, get_selected_index());
        state->suppress = false;
    }

    void tab_view::create() const {
        if (_created) return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = new linux::wmaker::native_tab_view();
        state->tabs = WMCreateTabView(linux::wmaker::parent_widget(self));
        if (!state->tabs) {
            delete state;
            throw std::runtime_error("Window Maker/WINGs: failed to create WMTabView.");
        }
        const point position = linux::wmaker::control_position(self);
        WMMoveWidget(state->tabs, position.x, position.y);
        WMResizeWidget(state->tabs, _bounds.d.w, _bounds.d.h);
        state->delegate.data = self;
        state->delegate.didSelectItem = selected;
        state->delegate.shouldSelectItem = may_select;
        WMSetTabViewDelegate(state->tabs, &state->delegate);
        linux::wmaker::wnd_bindings.register_pair(state->tabs, self);
        linux::wmaker::tab_view_bindings.register_pair(self, state);
        WMRealizeWidget(state->tabs);
        _created = true;
        self->_content_host_is_page = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        auto *state = binding(*const_cast<tab_view *>(this));
        if (!_created || !state || !state->tabs)
            throw std::runtime_error("Window Maker/WINGs: tab_view is not created.");
        WMRealizeWidget(state->tabs);
        WMMapWidget(state->tabs);
        const int selected_index = get_selected_index();
        if (selected_index >= 0) {
            wnd &content = get_item(static_cast<std::size_t>(selected_index)).get_content();
            if (content.get_created()) content.show();
        }
    }

    void tab_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state) {
            clear_native_items(*self, *state);
            linux::wmaker::wnd_bindings.unregister_by_object(self);
            if (state->tabs) WMDestroyWidget(state->tabs);
            linux::wmaker::tab_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

//
// Implements tab_view with Haiku's native BTabView and BTab classes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cmath>
#include <stdexcept>

#include <InterfaceDefs.h>
#include <TabView.h>
#include <View.h>
#include <Window.h>

#include <native.h>

#include "globals.h"

namespace
{
    template <typename function_type>
    void locked(BWindow *window, function_type &&function) {
        if (!window)
            return;
        const bool held = window->IsLocked();
        if (!held && !window->Lock())
            return;
        function();
        if (!held)
            window->Unlock();
    }

    class native_tab_view final : public BTabView
    {
    public:
        explicit native_tab_view(native::tab_view *owner)
            : BTabView("native_tab_view", B_WIDTH_AS_USUAL)
            , _owner(owner) {}

        void Select(int32 index) override {
            const int32 previous = Selection();
            BTabView::Select(index);
            if (!_suppress && _owner && Selection() != previous)
                _owner->on_native_selection(Selection());
        }

        bool _suppress = false;

    private:
        native::tab_view *_owner;
    };

    native_tab_view *native_view(native::tab_view &control) {
        auto *binding =
            haiku::tab_view_bindings.object_from_handle(&control);
        return binding
                   ? static_cast<native_tab_view *>(binding->view)
                   : nullptr;
    }

    void rebuild(native::tab_view &control,
                 native_tab_view &view) {
        for (std::size_t index = 0;
             index < control.get_item_count(); ++index) {
            native::wnd &content = control.get_item(index).get_content();
            if (content.get_created())
                content.destroy();
        }
        auto *binding =
            haiku::tab_view_bindings.object_from_handle(&control);
        if (binding)
            binding->pages.clear();
        view._suppress = true;
        while (view.CountTabs() > 0) {
            BTab *tab = view.RemoveTab(view.CountTabs() - 1);
            delete tab;
        }

        for (std::size_t index = 0;
             index < control.get_item_count();
             ++index) {
            native::tab_item &item = control.get_item(index);
            BView *page = new BView(
                "native_tab_page",
                B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
            page->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
            BTab *tab = new BTab(page);
            tab->SetLabel(item.get_title().c_str());
            tab->SetEnabled(item.get_enabled());
            view.AddTab(page, tab);
            if (binding)
                binding->pages.push_back(page);
        }

        const int selected = control.get_selected_index();
        if (selected >= 0)
            view.Select(selected);
        view._suppress = false;
        view.Invalidate();
    }
} // namespace

namespace native
{
    void tab_view::apply_items() {
        native_tab_view *view = native_view(*this);
        if (!view)
            throw std::runtime_error("Haiku: missing tab-view binding.");
        locked(view->Window(), [&] { rebuild(*this, *view); });
    }

    void tab_view::apply_selected_index() {
        native_tab_view *view = native_view(*this);
        if (!view)
            throw std::runtime_error("Haiku: missing tab-view binding.");
        locked(view->Window(), [&] {
            view->_suppress = true;
            if (get_selected_index() >= 0)
                view->Select(get_selected_index());
            view->_suppress = false;
        });
    }

    void tab_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        BView *parent = haiku::parent_view(get_parent(), self);
        if (!parent || !parent->Window()) {
            throw std::runtime_error(
                "Haiku: tab_view requires a created parent.");
        }

        native_tab_view *view = nullptr;
        locked(parent->Window(), [&] {
            view = new native_tab_view(self);
            view->MoveTo(_bounds.p.x, _bounds.p.y);
            view->ResizeTo(
                std::max(0, static_cast<int>(_bounds.d.w) - 1),
                std::max(0, static_cast<int>(_bounds.d.h) - 1));
            parent->AddChild(view);
        });
        if (!view)
            throw std::runtime_error("Haiku: failed to create tab_view.");

        auto *binding = new haiku::haiku_tab_view();
        binding->view = view;
        haiku::tab_view_bindings.register_pair(self, binding);
        _created = true;
        self->_content_host_is_page = true;
        self->_tab_height = std::max(
            1, static_cast<int>(std::ceil(view->TabHeight())));
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        auto *binding = haiku::tab_view_bindings.object_from_handle(
            const_cast<tab_view *>(this));
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: tab_view is not created.");
        locked(binding->view->Window(), [&] { binding->view->Show(); });
    }

    void tab_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        auto *binding =
            haiku::tab_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding && binding->view) {
            BWindow *window = binding->view->Window();
            locked(window, [&] {
                binding->view->RemoveSelf();
                delete binding->view;
            });
        }
        haiku::tab_view_bindings.unregister_by_handle(self);
        delete binding;
    }
} // namespace native

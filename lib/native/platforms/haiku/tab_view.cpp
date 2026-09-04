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

#include "collection_view.h"
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

        void DrawBox(BRect selected_tab) override {
            if (!_owner || _owner->get_page_frame_visible()) {
                BTabView::DrawBox(selected_tab);
                return;
            }
            const BRect bounds = Bounds();
            PushState();
            SetHighColor(ui_color(B_CONTROL_BORDER_COLOR));
            switch (TabSide()) {
            case kTopSide:
                StrokeLine(BPoint(0, TabHeight()),
                           BPoint(bounds.right, TabHeight()));
                break;
            case kBottomSide: {
                const float y = bounds.bottom - TabHeight();
                StrokeLine(BPoint(0, y), BPoint(bounds.right, y));
                break;
            }
            case kLeftSide:
                StrokeLine(BPoint(TabHeight(), 0),
                           BPoint(TabHeight(), bounds.bottom));
                break;
            case kRightSide: {
                const float x = bounds.right - TabHeight();
                StrokeLine(BPoint(x, 0), BPoint(x, bounds.bottom));
                break;
            }
            }
            PopState();
        }

        bool _suppress = false;

    private:
        native::tab_view *_owner;
    };

    native_tab_view *native_view(native::tab_view &control) {
        auto *binding =
            haiku::tab_view_bindings.object_from_handle(&control);
        return binding
                   ? static_cast<native_tab_view *>(binding->tabs)
                   : nullptr;
    }

    bool uses_native_tabs(const native::tab_view &control) {
        return control.get_tab_placement() ==
                   native::tab_placement::top ||
               control.get_tab_placement() ==
                   native::tab_placement::bottom;
    }

    void destroy_borrowed_contents(native::tab_view &control) {
        for (std::size_t index = 0;
             index < control.get_item_count(); ++index) {
            native::wnd &content = control.get_item(index).get_content();
            if (content.get_created())
                content.destroy();
        }
    }

    void destroy_host(native::tab_view &control,
                      haiku::haiku_tab_view &binding) {
        destroy_borrowed_contents(control);
        BWindow *window = binding.view ? binding.view->Window() : nullptr;
        locked(window, [&] {
            if (binding.view) {
                binding.view->RemoveSelf();
                delete binding.view;
            }
        });
        binding.view = nullptr;
        binding.tabs = nullptr;
        binding.pages.clear();
        binding.visible_page = -1;
    }

    void create_host(native::tab_view &control,
                     haiku::haiku_tab_view &binding) {
        if (!uses_native_tabs(control)) {
            binding.view = haiku::create_collection_view(control);
            return;
        }
        BView *parent = haiku::parent_view(
            control.get_parent(), &control);
        if (!parent || !parent->Window()) {
            throw std::runtime_error(
                "Haiku: tab_view requires a created parent.");
        }
        native_tab_view *view = nullptr;
        locked(parent->Window(), [&] {
            view = new native_tab_view(&control);
            view->SetTabSide(
                control.get_tab_placement() ==
                        native::tab_placement::bottom
                    ? BTabView::kBottomSide
                    : BTabView::kTopSide);
            view->SetBorder(control.get_page_frame_visible()
                                ? B_FANCY_BORDER
                                : B_NO_BORDER);
            const native::rect bounds = control.get_bounds();
            view->MoveTo(bounds.p.x, bounds.p.y);
            view->ResizeTo(
                std::max(0, static_cast<int>(bounds.d.w) - 1),
                std::max(0, static_cast<int>(bounds.d.h) - 1));
            view->Hide();
            parent->AddChild(view);
        });
        if (!view)
            throw std::runtime_error("Haiku: failed to create tab_view.");
        binding.view = view;
        binding.tabs = view;
    }

    void rebuild_native(native::tab_view &control,
                        native_tab_view &view) {
        view.SetTabSide(
            control.get_tab_placement() == native::tab_placement::bottom
                ? BTabView::kBottomSide
                : BTabView::kTopSide);
        view.SetBorder(control.get_page_frame_visible()
                           ? B_FANCY_BORDER
                           : B_NO_BORDER);
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
        if (binding)
            binding->visible_page = -1;
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

    void rebuild_portable(native::tab_view &control,
                          haiku::haiku_tab_view &binding) {
        destroy_borrowed_contents(control);
        BWindow *window = binding.view ? binding.view->Window() : nullptr;
        locked(window, [&] {
            for (BView *page : binding.pages) {
                page->RemoveSelf();
                delete page;
            }
            binding.pages.clear();
            const native::rect content = control.get_content_bounds();
            for (std::size_t index = 0;
                 index < control.get_item_count(); ++index) {
                BView *page = new BView(
                    BRect(content.p.x,
                          content.p.y,
                          std::max<int>(content.p.x,
                                        content.x2() - 1),
                          std::max<int>(content.p.y,
                                        content.y2() - 1)),
                    "native_tab_page",
                    B_FOLLOW_NONE,
                    B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
                page->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
                page->Hide();
                binding.view->AddChild(page);
                binding.pages.push_back(page);
            }
        });
        binding.visible_page = -1;
    }
} // namespace

namespace native
{
    void tab_view::apply_items() {
        native_tab_view *view = native_view(*this);
        auto *binding = haiku::tab_view_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error("Haiku: missing tab-view binding.");
        const bool native = uses_native_tabs(*this);
        if (!binding->view || native != (binding->tabs != nullptr)) {
            if (binding->view) {
                delete _gpx;
                _gpx = nullptr;
                destroy_host(*this, *binding);
            }
            create_host(*this, *binding);
            // The binding is installed before create_host() because the
            // refresh path needs to find it.  Publish the host afterwards
            // so custom left/right tab views can obtain their BWindow while
            // painting instead of falling through to the top-level lookup.
            native::detail::wnd_peer_access::assign_content(
                *this, binding->view);
        }
        view = native_view(*this);
        if (view) {
            locked(view->Window(), [&] {
                rebuild_native(*this, *view);
            });
            _tab_height = std::max(
                1, static_cast<int>(std::ceil(view->TabHeight())));
        } else {
            rebuild_portable(*this, *binding);
        }
    }

    void tab_view::apply_selected_index() {
        native_tab_view *view = native_view(*this);
        auto *binding = haiku::tab_view_bindings.object_from_handle(this);
        if (!binding || !binding->view)
            throw std::runtime_error("Haiku: missing tab-view binding.");
        locked(binding->view->Window(), [&] {
            if (view) {
                view->_suppress = true;
                if (get_selected_index() >= 0)
                    view->Select(get_selected_index());
                view->_suppress = false;
                return;
            }
            const int selected = get_selected_index();
            if (binding->visible_page == selected)
                return;
            if (binding->visible_page >= 0 &&
                binding->visible_page <
                    static_cast<int>(binding->pages.size())) {
                binding->pages[static_cast<std::size_t>(
                    binding->visible_page)]->Hide();
            }
            if (selected >= 0 &&
                selected < static_cast<int>(binding->pages.size())) {
                binding->pages[static_cast<std::size_t>(selected)]->Show();
            }
            binding->visible_page = selected;
        });
    }

    void tab_view::create_native() {
        auto *self = this;
        auto *binding = new haiku::haiku_tab_view();
        haiku::tab_view_bindings.register_pair(self, binding);
        self->configure_page_host(true, true);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void tab_view::show_native() {
        auto *binding = haiku::tab_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: tab_view is not created.");
        locked(binding->view->Window(), [&] { binding->view->Show(); });
        this->apply_selected_index();
    }

    void tab_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            haiku::tab_view_bindings.object_from_handle(self);
        if (binding)
            destroy_host(*self, *binding);
        haiku::tab_view_bindings.unregister_by_handle(self);
        delete binding;
    }
} // namespace native

// Implements split_view with Haiku's native BSplitView.

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <InterfaceDefs.h>
#include <SplitView.h>
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

    class native_split_view final : public BSplitView
    {
    public:
        native_split_view(native::split_view *owner,
                          orientation direction)
            : BSplitView(direction, 0)
            , _owner(owner) {}

        void AttachedToWindow() override {
            BSplitView::AttachedToWindow();
            // The portable window canvas is transparent. Do not adopt that
            // as the splitter base: native BSplitView shades ViewColor().
            SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
            SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
            Invalidate();
        }

        void MouseMoved(BPoint where,
                        uint32 transit,
                        const BMessage *message) override {
            BSplitView::MouseMoved(where, transit, message);
            refresh_portable_panes();
        }

        void MouseUp(BPoint where) override {
            BSplitView::MouseUp(where);
            if (!_owner || _suppress || CountItems() < 2)
                return;
            const float first = ItemWeight(0);
            const float second = ItemWeight(1);
            if (first + second > 0)
                _owner->on_native_ratio(first/(first+second));
        }

        bool _suppress = false;

    private:
        native::split_view *_owner;

        void refresh_portable_panes() {
            if (!_owner)
                return;
            auto *state = haiku::split_view_bindings
                              .object_from_handle(_owner);
            if (!state || !state->first || !state->second)
                return;
            const BRect first = state->first->Bounds();
            const BRect second = state->second->Bounds();
            _owner->get_first().set_bounds(native::rect(
                0,
                0,
                static_cast<native::dim>(
                    std::max(0.0f, first.Width() + 1.0f)),
                static_cast<native::dim>(
                    std::max(0.0f, first.Height() + 1.0f))));
            _owner->get_second().set_bounds(native::rect(
                0,
                0,
                static_cast<native::dim>(
                    std::max(0.0f, second.Width() + 1.0f)),
                static_cast<native::dim>(
                    std::max(0.0f, second.Height() + 1.0f))));
        }
    };

    haiku::haiku_split_view *binding(native::split_view &owner) {
        return haiku::split_view_bindings.object_from_handle(&owner);
    }

    orientation direction(native::split_orientation value) {
        return value == native::split_orientation::horizontal
            ? B_HORIZONTAL : B_VERTICAL;
    }
} // namespace

namespace native
{
    void split_view::apply_orientation() {
        auto *state = binding(*this);
        if (!state || !state->view)
            throw std::runtime_error("Haiku: missing split-view binding.");
        locked(state->view->Window(), [&] {
            state->view->SetOrientation(direction(get_orientation()));
        });
    }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state || !state->view)
            throw std::runtime_error("Haiku: missing split-view binding.");
        locked(state->view->Window(), [&] {
            state->suppress = true;
            static_cast<native_split_view *>(state->view)->_suppress = true;
            state->view->SetItemWeight(0, get_ratio(), false);
            state->view->SetItemWeight(1, 1.0f-get_ratio(), true);
            static_cast<native_split_view *>(state->view)->_suppress = false;
            state->suppress = false;
        });
    }

    void split_view::apply_minimums() {
        auto *state = binding(*this);
        if (!state)
            return;
        const bool horizontal =
            get_orientation() == split_orientation::horizontal;
        state->first->SetExplicitMinSize(horizontal
            ? BSize(get_first_minimum(), B_SIZE_UNSET)
            : BSize(B_SIZE_UNSET, get_first_minimum()));
        state->second->SetExplicitMinSize(horizontal
            ? BSize(get_second_minimum(), B_SIZE_UNSET)
            : BSize(B_SIZE_UNSET, get_second_minimum()));
    }

    void split_view::apply_splitter_size() {
        auto *state = binding(*this);
        if (state && state->view)
            state->view->SetSplitterSize(get_splitter_size());
    }

    void split_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<split_view *>(this);
        BView *parent = haiku::parent_view(get_parent(), self);
        if (!parent || !parent->Window())
            throw std::runtime_error(
                "Haiku: split_view requires a created parent.");

        auto *state = new haiku::haiku_split_view();
        locked(parent->Window(), [&] {
            auto *view = new native_split_view(
                self, direction(get_orientation()));
            view->MoveTo(_bounds.p.x, _bounds.p.y);
            view->ResizeTo(
                std::max(0, static_cast<int>(_bounds.d.w)-1),
                std::max(0, static_cast<int>(_bounds.d.h)-1));
            view->SetInsets(0);
            view->SetSplitterSize(get_splitter_size());
            auto *first = new BView("native_split_first", B_WILL_DRAW);
            auto *second = new BView("native_split_second", B_WILL_DRAW);
            first->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
            second->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
            view->AddChild(first, get_ratio());
            view->AddChild(second, 1.0f-get_ratio());
            view->SetCollapsible(false);
            parent->AddChild(view);
            state->view = view;
            state->first = first;
            state->second = second;
        });
        if (!state->view) {
            delete state;
            throw std::runtime_error("Haiku: failed to create split_view.");
        }
        haiku::split_view_bindings.register_pair(self, state);
        _created = true;
        self->_content_hosts_are_panes = true;
        self->apply_minimums();
        self->refresh_contents();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = binding(*const_cast<split_view *>(this));
        if (!_created || !state || !state->view)
            throw std::runtime_error("Haiku: split_view is not created.");
        locked(state->view->Window(), [&] { state->view->Show(); });
        get_first().show();
        get_second().show();
    }

    void split_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<split_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state && state->view) {
            BWindow *window = state->view->Window();
            locked(window, [&] {
                state->view->RemoveSelf();
                delete state->view;
            });
        }
        haiku::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native

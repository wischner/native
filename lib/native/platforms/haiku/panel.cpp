//
// Implements the Haiku structural container as a plain child BView.
// The view is a real native parent for every supported control, and
// the app_server paints its panel background.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <InterfaceDefs.h>
#include <View.h>
#include <Window.h>

#include <native.h>
#include <native/panel.h>

#include "globals.h"

namespace
{
    // A structural host draws no content of its own. Leaving out
    // B_WILL_DRAW lets the app_server fill the view color directly, so
    // exposed panel space never shows stale pixels.
    class panel_view final : public BView
    {
    public:
        panel_view(native::panel &owner, BRect frame)
            : BView(frame,
                    "native_panel",
                    B_FOLLOW_NONE,
                    B_FRAME_EVENTS)
            , _owner(owner) {
            SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
            SetLowColor(ViewColor());
        }

        void MouseDown(BPoint where) override {
            _owner.on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y))));
        }

        void MouseUp(BPoint where) override {
            _owner.on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y))));
        }

        void MouseMoved(BPoint where,
                        uint32 transit,
                        const BMessage *message) override {
            (void)transit;
            (void)message;
            _owner.on_native_mouse_move(native::point(
                static_cast<native::coord>(where.x),
                static_cast<native::coord>(where.y)));
        }

    private:
        native::panel &_owner;
    };
} // namespace

namespace native
{
    void panel::create() const {
        if (_created)
            return;

        auto *self = const_cast<panel *>(this);
        BView *parent = haiku::parent_view(get_parent(), self);
        BWindow *window = parent ? parent->Window() : nullptr;
        if (!parent || !window)
            throw std::runtime_error(
                "Haiku: panel requires a created parent.");

        const bool locked = window->IsLocked();
        if (!locked && !window->Lock())
            throw std::runtime_error(
                "Haiku: failed to lock panel parent.");
        auto *view = new panel_view(*self,
                                    BRect(_bounds.p.x,
                                          _bounds.p.y,
                                          _bounds.x2() - 1,
                                          _bounds.y2() - 1));
        view->Hide();
        parent->AddChild(view);
        if (!locked)
            window->Unlock();

        auto *binding = new haiku::haiku_surface();
        binding->view = view;

        // Children resolve their parent view through this registry, so
        // the binding has to exist before on_wnd_create runs.
        haiku::panel_bindings.register_pair(self, binding);
        _created = true;
        self->on_native_create();
    }

    void panel::show() const {
        auto *binding = haiku::panel_bindings.object_from_handle(
            const_cast<panel *>(this));
        if (!_created || !binding || !binding->view)
            throw std::runtime_error("Haiku: panel is not created.");

        BWindow *window = binding->view->Window();
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            binding->view->Show();
            if (!locked)
                window->Unlock();
        }
    }

    void panel::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<panel *>(this);
        auto *binding = haiku::panel_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            if (binding->view) {
                BWindow *window = binding->view->Window();
                const bool locked = window && window->IsLocked();
                if (window && (locked || window->Lock())) {
                    binding->view->RemoveSelf();
                    delete binding->view;
                    if (!locked)
                        window->Unlock();
                }
            }
            haiku::panel_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

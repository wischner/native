//
// Implements the Haiku button-control backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <Button.h>
#include <Window.h>

#include <native.h>
#include <native/button.h>

#include "../../control_render_access.h"
#include "globals.h"

namespace
{
    template <typename function_type>
    void with_locked_window(BWindow *window, function_type &&function) {
        if (!window)
            return;

        const bool already_locked = window->IsLocked();
        if (!already_locked && !window->Lock())
            return;

        function(window);

        if (!already_locked)
            window->Unlock();
    }

    class native_button_view : public BButton
    {
    public:
        native_button_view(BRect frame,
                           const char *label,
                           BMessage *message,
                           native::button *owner)
            : BButton(frame, "native_button", label, message)
            , _owner(owner) {}

        void Draw(BRect update) override {
            if (!_owner || !_owner->get_created()) {
                BButton::Draw(update);
                return;
            }
            native::gpx &graphics = _owner->get_gpx();
            auto appearance = native::theme::create(graphics);
            const BRect frame = Bounds();
            const native::rect bounds(
                0,
                0,
                static_cast<native::dim>(frame.IntegerWidth() + 1),
                static_cast<native::dim>(frame.IntegerHeight() + 1));
            graphics.set_clip(native::rect(
                static_cast<native::coord>(update.left),
                static_cast<native::coord>(update.top),
                static_cast<native::dim>(update.IntegerWidth() + 1),
                static_cast<native::dim>(update.IntegerHeight() + 1)));
            native::theme::state state;
            state.disabled = !IsEnabled();
            state.focused = IsFocus();
            state.pressed = Value() == B_CONTROL_ON;
            native::detail::control_render_access::draw(
                *_owner, graphics, *appearance, bounds, state);
        }

    private:
        native::button *_owner;
    };

} // namespace

namespace native
{
    void button::apply_text() {
        auto *binding = haiku::button_bindings.object_from_handle(this);
        if (!binding || !binding->button)
            throw std::runtime_error("Haiku: Missing BButton binding.");

        BWindow *window = binding->button->Window();
        with_locked_window(window, [&](BWindow *) {
            binding->button->SetLabel(_text.c_str());
        });
    }

    void button::create() const {
        if (_created)
            return;

        wnd *p = get_parent();
        if (!p)
            throw std::runtime_error(
                "Haiku: button requires a parent window.");
        if (!p->get_created())
            throw std::runtime_error(
                "Haiku: button parent is not created.");

        auto *self = const_cast<button *>(this);
        BView *parent = haiku::parent_view(p, self);
        BWindow *window = parent ? parent->Window() : nullptr;
        if (!parent || !window)
            throw std::runtime_error(
                "Haiku: button parent is not created.");


        BButton *btn = nullptr;
        with_locked_window(window, [&](BWindow *) {
            BRect frame(
                static_cast<float>(_bounds.p.x),
                static_cast<float>(_bounds.p.y),
                static_cast<float>(_bounds.p.x + _bounds.d.w - 1),
                static_cast<float>(_bounds.p.y + _bounds.d.h - 1));

            BMessage *message = new BMessage(haiku::button_message);
            message->AddPointer(haiku::control_owner_field, self);
            btn = new native_button_view(
                frame, _text.c_str(), message, self);
            parent->AddChild(btn);
        });

        if (!btn)
            throw std::runtime_error("Haiku: Failed to create button.");

        auto *h = new haiku::haiku_button();
        h->button = btn;
        h->owner = self;
        haiku::button_bindings.register_pair(self, h);

        _created = true;
        self->on_native_create();
    }

    void button::show() const {
        if (!_created)
            throw std::runtime_error(
                "Haiku: Cannot show button before it is created.");

        auto *h = haiku::button_bindings.object_from_handle(
            const_cast<button *>(this));
        if (!h || !h->button)
            throw std::runtime_error("Haiku: Missing BButton binding.");

        BWindow *window = h->button->Window();
        with_locked_window(window, [&](BWindow *) {
            h->button->Show();
        });
    }

    void button::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = haiku::button_bindings.object_from_handle(self);
        self->on_native_destroy();

        if (h) {
            if (h->button) {
                BWindow *window = h->button->Window();
                with_locked_window(window, [&](BWindow *) {
                    h->button->RemoveSelf();
                    delete h->button;
                });
            }
            haiku::button_bindings.unregister_by_handle(self);
            delete h;
        }
    }
} // namespace native

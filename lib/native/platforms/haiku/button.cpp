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

        BView *parent = haiku::parent_view(p);
        BWindow *window = parent ? parent->Window() : nullptr;
        if (!parent || !window)
            throw std::runtime_error(
                "Haiku: button parent is not created.");

        auto *self = const_cast<button *>(this);

        BButton *btn = nullptr;
        with_locked_window(window, [&](BWindow *) {
            BRect frame(
                static_cast<float>(_bounds.p.x),
                static_cast<float>(_bounds.p.y),
                static_cast<float>(_bounds.p.x + _bounds.d.w - 1),
                static_cast<float>(_bounds.p.y + _bounds.d.h - 1));

            BMessage *message = new BMessage(haiku::button_message);
            message->AddPointer(haiku::control_owner_field, self);
            btn = new BButton(frame,
                              "native_button",
                              _text.c_str(),
                              message);
            parent->AddChild(btn);
        });

        if (!btn)
            throw std::runtime_error("Haiku: Failed to create button.");

        auto *h = new haiku::haiku_button();
        h->button = btn;
        h->owner = self;
        haiku::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
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

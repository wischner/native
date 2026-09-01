//
// Implements the native Haiku radio control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <RadioButton.h>
#include <Window.h>
#include <native.h>
#include <native/radio.h>
#include "globals.h"
namespace
{
    template <typename function_type>
    void locked(BWindow *window, function_type function) {
        if (!window)
            return;
        bool held = window->IsLocked();
        if (!held && !window->Lock())
            return;
        function();
        if (!held)
            window->Unlock();
    }
    BView *parent_view(native::radio *c) {
        auto *p = c->get_parent();
        BView *view = haiku::parent_view(p);
        if (!p || !p->get_created() || !view || !view->Window())
            throw std::runtime_error(
                "Haiku: radio requires a created parent.");
        return view;
    }
    class native_radio_view : public BRadioButton
    {
    public:
        native_radio_view(BRect r, const char *l, native::radio *o)
            : BRadioButton(r, "native_radio", l, new BMessage('nrad'))
            , _owner(o) {}
        status_t Invoke(BMessage *m = nullptr) override {
            if (_owner)
                _owner->on_native_selected();
            return BRadioButton::Invoke(m);
        }

    private:
        native::radio *_owner;
    };
} // namespace
namespace native
{
    void radio::apply_text() {
        auto *b = haiku::radio_bindings.object_from_handle(this);
        if (!b || !b->view)
            throw std::runtime_error("Haiku: Missing radio binding.");
        locked(b->view->Window(), [&] {
            b->view->SetLabel(_text.c_str());
        });
    }
    void radio::apply_selected() {
        auto *b = haiku::radio_bindings.object_from_handle(this);
        if (!b || !b->view)
            throw std::runtime_error("Haiku: Missing radio binding.");
        locked(b->view->Window(), [&] {
            b->view->SetValue(_selected ? B_CONTROL_ON : B_CONTROL_OFF);
        });
    }
    void radio::create() const {
        if (_created)
            return;
        auto *self = const_cast<radio *>(this);
        BView *parent = parent_view(self);
        BWindow *w = parent->Window();
        native_radio_view *v = nullptr;
        locked(w, [&] {
            v = new native_radio_view(BRect(_bounds.p.x,
                                            _bounds.p.y,
                                            _bounds.x2() - 1,
                                            _bounds.y2() - 1),
                                      _text.c_str(),
                                      self);
            v->SetValue(_selected ? B_CONTROL_ON : B_CONTROL_OFF);
            parent->AddChild(v);
        });
        if (!v)
            throw std::runtime_error("Haiku: Failed to create radio.");
        auto *b = new haiku::haiku_radio();
        b->view = v;
        haiku::radio_bindings.register_pair(self, b);
        _created = true;
        self->on_wnd_create.emit();
    }
    void radio::show() const {
        auto *b = haiku::radio_bindings.object_from_handle(
            const_cast<radio *>(this));
        if (!_created || !b || !b->view)
            throw std::runtime_error("Haiku: radio is not created.");
        locked(b->view->Window(), [&] {
            b->view->Show();
        });
    }
    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        auto *b = haiku::radio_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (b) {
            BWindow *w = b->view ? b->view->Window() : nullptr;
            locked(w, [&] {
                b->view->RemoveSelf();
                delete b->view;
            });
            haiku::radio_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native

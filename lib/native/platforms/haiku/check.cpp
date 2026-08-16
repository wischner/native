//
// Implements the native Haiku check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <CheckBox.h>
#include <Window.h>
#include <native.h>
#include <native/check.h>
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
    BWindow *parent(native::check *c) {
        auto *p = c->get_parent();
        BWindow *w =
            p ? haiku::wnd_bindings.handle_from_object(p) : nullptr;
        if (!p || !p->get_created() || !w)
            throw std::runtime_error(
                "Haiku: check requires a created parent.");
        return w;
    }
    class native_check_view : public BCheckBox
    {
    public:
        native_check_view(BRect r, const char *l, native::check *o)
            : BCheckBox(r, "native_check", l, new BMessage('nchk'))
            , _owner(o) {}
        status_t Invoke(BMessage *m = nullptr) override {
            if (_owner)
                _owner->on_native_checked(Value() == B_CONTROL_ON);
            return BCheckBox::Invoke(m);
        }

    private:
        native::check *_owner;
    };
} // namespace
namespace native
{
    void check::apply_text() {
        auto *b = haiku::check_bindings.object_from_handle(this);
        if (!b || !b->view)
            throw std::runtime_error("Haiku: Missing check binding.");
        locked(b->view->Window(), [&] {
            b->view->SetLabel(_text.c_str());
        });
    }
    void check::apply_checked() {
        auto *b = haiku::check_bindings.object_from_handle(this);
        if (!b || !b->view)
            throw std::runtime_error("Haiku: Missing check binding.");
        locked(b->view->Window(), [&] {
            b->view->SetValue(_checked ? B_CONTROL_ON : B_CONTROL_OFF);
        });
    }
    void check::create() const {
        if (_created)
            return;
        auto *self = const_cast<check *>(this);
        BWindow *w = parent(self);
        native_check_view *v = nullptr;
        locked(w, [&] {
            BView *content = haiku::content_view(w);
            if (!content)
                return;

            v = new native_check_view(BRect(_bounds.p.x,
                                            _bounds.p.y,
                                            _bounds.x2() - 1,
                                            _bounds.y2() - 1),
                                      _text.c_str(),
                                      self);
            v->SetValue(_checked ? B_CONTROL_ON : B_CONTROL_OFF);
            content->AddChild(v);
        });
        if (!v)
            throw std::runtime_error("Haiku: Failed to create check.");
        auto *b = new haiku::haiku_check();
        b->view = v;
        haiku::check_bindings.register_pair(self, b);
        _created = true;
        self->on_wnd_create.emit();
    }
    void check::show() const {
        auto *b = haiku::check_bindings.object_from_handle(
            const_cast<check *>(this));
        if (!_created || !b || !b->view)
            throw std::runtime_error("Haiku: check is not created.");
        locked(b->view->Window(), [&] {
            b->view->Show();
        });
    }
    void check::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<check *>(this);
        auto *b = haiku::check_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (b) {
            BWindow *w = b->view ? b->view->Window() : nullptr;
            locked(w, [&] {
                b->view->RemoveSelf();
                delete b->view;
            });
            haiku::check_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native

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
#include "../../control_render_access.h"
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
    BView *parent_view(native::check *c) {
        auto *p = c->get_parent();
        BView *view = haiku::parent_view(p);
        if (!p || !p->get_created() || !view || !view->Window())
            throw std::runtime_error(
                "Haiku: check requires a created parent.");
        return view;
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

        void Draw(BRect update) override {
            if (!_owner || !_owner->get_created()) {
                BCheckBox::Draw(update);
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
        BView *parent = parent_view(self);
        BWindow *w = parent->Window();
        native_check_view *v = nullptr;
        locked(w, [&] {
            v = new native_check_view(BRect(_bounds.p.x,
                                            _bounds.p.y,
                                            _bounds.x2() - 1,
                                            _bounds.y2() - 1),
                                      _text.c_str(),
                                      self);
            v->SetValue(_checked ? B_CONTROL_ON : B_CONTROL_OFF);
            parent->AddChild(v);
        });
        if (!v)
            throw std::runtime_error("Haiku: Failed to create check.");
        auto *b = new haiku::haiku_check();
        b->view = v;
        haiku::check_bindings.register_pair(self, b);
        _created = true;
        self->on_native_create();
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

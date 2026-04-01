#include <stdexcept>
#include <utility>

#include <Button.h>
#include <Window.h>

#include <native.h>

#include "globals.h"

namespace
{
    template <typename Fn>
    void with_locked_window(BWindow *window, Fn &&fn)
    {
        if (!window)
            return;

        const bool already_locked = window->IsLocked();
        if (!already_locked && !window->Lock())
            return;

        fn(window);

        if (!already_locked)
            window->Unlock();
    }

    class NativeButton : public BButton
    {
    public:
        NativeButton(BRect frame, const char *name, const char *label, native::button *owner)
            : BButton(frame, name, label, new BMessage('nbtn')),
              _owner(owner)
        {
        }

        status_t Invoke(BMessage *message = nullptr) override
        {
            if (_owner)
                _owner->on_click.emit();
            return BButton::Invoke(message);
        }

    private:
        native::button *_owner;
    };
} // namespace

namespace native
{
    button::button(std::string text, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _text(std::move(text))
    {
    }

    button::button(const std::string &text, const point &pos, const size &dim)
        : button(text, pos.x, pos.y, dim.w, dim.h)
    {
    }

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    const std::string &button::text() const
    {
        return _text;
    }

    button &button::set_text(const std::string &text)
    {
        _text = text;

        if (_created)
        {
            auto *h = haiku::button_bindings.from_a(const_cast<button *>(this));
            if (h && h->button)
            {
                BWindow *window = h->button->Window();
                with_locked_window(window, [&](BWindow *) {
                    h->button->SetLabel(_text.c_str());
                });
            }
        }

        return *this;
    }

    void button::create() const
    {
        if (_created)
            return;

        wnd *p = parent();
        if (!p)
            throw std::runtime_error("Haiku: button requires a parent window.");

        BWindow *window = haiku::wnd_bindings.from_b(p);
        if (!window)
            throw std::runtime_error("Haiku: button parent is not created.");

        auto *self = const_cast<button *>(this);

        NativeButton *btn = nullptr;
        with_locked_window(window, [&](BWindow *locked_window) {
            BRect frame(
                static_cast<float>(_bounds.p.x),
                static_cast<float>(_bounds.p.y),
                static_cast<float>(_bounds.p.x + _bounds.d.w - 1),
                static_cast<float>(_bounds.p.y + _bounds.d.h - 1));

            btn = new NativeButton(frame, "native_button", _text.c_str(), self);
            locked_window->AddChild(btn);
        });

        if (!btn)
            throw std::runtime_error("Haiku: Failed to create button.");

        auto *h = new haiku::haikubutton();
        h->button = btn;
        h->owner = self;
        haiku::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const
    {
        if (!_created)
            throw std::runtime_error("Haiku: Cannot show button before it is created.");

        auto *h = haiku::button_bindings.from_a(const_cast<button *>(this));
        if (!h || !h->button)
            throw std::runtime_error("Haiku: Missing BButton binding.");

        BWindow *window = h->button->Window();
        with_locked_window(window, [&](BWindow *) {
            h->button->Show();
        });
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = haiku::button_bindings.from_a(self);

        if (h)
        {
            if (h->button)
            {
                BWindow *window = h->button->Window();
                with_locked_window(window, [&](BWindow *) {
                    h->button->RemoveSelf();
                    delete h->button;
                });
            }
            haiku::button_bindings.unregister_by_a(self);
            delete h;
        }

        _created = false;
    }
} // namespace native

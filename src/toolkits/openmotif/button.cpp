#include <stdexcept>
#include <utility>

#include <X11/Intrinsic.h>
#include <Xm/PushB.h>
#include <Xm/Xm.h>

#include <native.h>

#include "globals.h"

namespace
{
    void button_activate(Widget, XtPointer client_data, XtPointer)
    {
        auto *owner = static_cast<native::button *>(client_data);
        if (owner)
            owner->on_click.emit();
    }
}

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
            auto *h = motif::button_bindings.from_a(const_cast<button *>(this));
            if (h && h->widget)
            {
                XmString label = XmStringCreateLocalized(const_cast<char *>(_text.c_str()));
                XtVaSetValues(h->widget, XmNlabelString, label, nullptr);
                XmStringFree(label);
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
            throw std::runtime_error("Motif: button requires a parent window.");

        Widget parent_widget = motif::wnd_bindings.from_b(p);
        if (!parent_widget)
            throw std::runtime_error("Motif: button parent is not created.");

        XmString label = XmStringCreateLocalized(const_cast<char *>(_text.c_str()));

        Arg args[5];
        int n = 0;
        XtSetArg(args[n], XmNx, _bounds.p.x); ++n;
        XtSetArg(args[n], XmNy, _bounds.p.y); ++n;
        XtSetArg(args[n], XmNwidth, _bounds.d.w); ++n;
        XtSetArg(args[n], XmNheight, _bounds.d.h); ++n;
        XtSetArg(args[n], XmNlabelString, label); ++n;

        Widget btn = XmCreatePushButton(
            parent_widget,
            const_cast<char *>("button"),
            args,
            n);

        XmStringFree(label);

        if (!btn)
            throw std::runtime_error("Motif: Failed to create button.");

        auto *self = const_cast<button *>(this);
        XtAddCallback(btn, XmNactivateCallback, button_activate, self);

        motif::wnd_bindings.register_pair(btn, self);

        auto *h = new motif::motifbutton();
        h->widget = btn;
        h->owner = self;
        motif::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const
    {
        if (!_created)
            throw std::runtime_error("Motif: Cannot show button before it is created.");

        auto *h = motif::button_bindings.from_a(const_cast<button *>(this));
        if (!h || !h->widget)
            throw std::runtime_error("Motif: Missing button widget binding.");

        XtManageChild(h->widget);
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = motif::button_bindings.from_a(self);

        if (h)
        {
            if (h->widget)
            {
                motif::wnd_bindings.unregister_by_a(h->widget);
                XtDestroyWidget(h->widget);
            }
            motif::button_bindings.unregister_by_a(self);
            delete h;
        }

        _created = false;
    }
} // namespace native

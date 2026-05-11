#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native.h>

#include "globals.h"

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
        invalidate();
        return *this;
    }

    void button::create() const
    {
        if (_created)
            return;

        if (!parent())
            throw std::runtime_error("GEMix: button requires a parent.");

        _created = true;
        gemix::buttons.push_back(const_cast<button *>(this));
        const_cast<button *>(this)->on_wnd_create.emit();
    }

    void button::show() const
    {
        invalidate();
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        gemix::buttons.erase(
            std::remove(gemix::buttons.begin(), gemix::buttons.end(), self),
            gemix::buttons.end());
        _created = false;
    }
}

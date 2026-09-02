// Implements the portable splitter in the GEM structural backend.

#include <stdexcept>
#include <native.h>

namespace native
{
    void split_view::apply_orientation() { invalidate(); }
    void split_view::apply_ratio() { invalidate(); }
    void split_view::apply_minimums() { invalidate(); }
    void split_view::apply_splitter_size() { invalidate(); }

    void split_view::create() const {
        if (_created) return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error("GEMix: split_view requires a created parent.");
        auto *self = const_cast<split_view *>(this);
        _created = true;
        self->refresh_contents();
        self->on_native_create();
    }

    void split_view::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: split_view is not created.");
        get_first().show();
        get_second().show();
        invalidate();
    }

    void split_view::destroy() const {
        if (!_created) return;
        const_cast<split_view *>(this)->on_native_destroy();
    }
} // namespace native

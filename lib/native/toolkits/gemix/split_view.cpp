// Implements the portable splitter in the GEM structural backend.

#include <stdexcept>
#include <native.h>

namespace native
{
    void split_view::apply_orientation() { invalidate(); }
    void split_view::apply_ratio() { invalidate(); }
    void split_view::apply_minimums() { invalidate(); }
    void split_view::apply_splitter_size() { invalidate(); }

    void split_view::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error("GEMix: split_view requires a created parent.");
        auto *self = this;
        self->refresh_contents();
    }

    void split_view::show_native() {
        if (!_created)
            throw std::runtime_error("GEMix: split_view is not created.");
        get_first().show();
        get_second().show();
        invalidate();
    }

    void split_view::destroy_native() {
        if (!_created) return;
    }
} // namespace native

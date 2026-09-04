// Implements a Win32 splitter host (Win32 has no stock splitter class).

#include <stdexcept>
#include <windows.h>
#include <native.h>
#include "globals.h"

namespace
{
    HWND handle(native::split_view *owner) {
        return windows::wnd_bindings.handle_from_object(owner);
    }
}

namespace native
{
    void split_view::apply_orientation() { invalidate(); }
    void split_view::apply_ratio() { invalidate(); }
    void split_view::apply_minimums() { invalidate(); }
    void split_view::apply_splitter_size() { invalidate(); }

    void split_view::create_native() {
        wnd *parent = get_parent();
        HWND parent_window = parent
            ? windows::wnd_bindings.handle_from_object(parent)
            : nullptr;
        if (!parent || !parent->get_created() || !parent_window)
            throw std::runtime_error(
                "Windows: split_view requires a created parent.");
        windows::register_window_class();
        auto *self = this;
        HWND window = CreateWindowExW(
            0,
            windows::class_name,
            L"",
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            _bounds.p.x,
            _bounds.p.y,
            _bounds.d.w,
            _bounds.d.h,
            parent_window,
            nullptr,
            GetModuleHandle(nullptr),
            self);
        if (!window)
            throw std::runtime_error(
                "Windows: failed to create split_view host.");
        self->refresh_contents();
    }

    void split_view::show_native() {
        HWND window = handle(this);
        if (!_created || !window)
            throw std::runtime_error("Windows: split_view is not created.");
        ShowWindow(window, SW_SHOW);
        get_first().show();
        get_second().show();
        UpdateWindow(window);
    }

    void split_view::destroy_native() {
        if (!_created) return;
        HWND window = handle(this);
        if (window) DestroyWindow(window);
    }
} // namespace native

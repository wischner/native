//
// Implements the Win32 structural container and paintable child
// surface. Both are child windows of the shared Native class, so the
// routed window procedure already supplies paint, size, and pointer
// notifications.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <windows.h>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "globals.h"

namespace
{
    const wchar_t panel_class_name[] = L"native_panel_class";

    //
    // Register the structural container class.
    //
    // Notes:
    //      It shares the routed procedure with every other Native
    //      window and differs only in its background brush: a panel
    //      hosts controls, so it takes the system's control-host
    //      color rather than the document color.
    //
    void register_panel_class() {
        static bool registered = false;
        if (registered)
            return;

        WNDCLASSW description = {};
        description.lpfnWndProc = windows::routed_wnd_proc;
        description.hInstance = GetModuleHandle(nullptr);
        description.lpszClassName = panel_class_name;
        description.hCursor = LoadCursor(nullptr, IDC_ARROW);
        description.hbrBackground =
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

        if (!RegisterClassW(&description))
            throw std::runtime_error(
                "Windows: failed to register panel class.");

        registered = true;
    }

    HWND resolve_parent(native::wnd *control, const char *what) {
        native::wnd *parent = control->get_parent();
        HWND handle =
            parent ? windows::wnd_bindings.handle_from_object(parent)
                   : nullptr;
        if (!parent || !parent->get_created() || !handle)
            throw std::runtime_error(
                std::string("Windows: ") + what +
                " requires a created parent.");
        return handle;
    }
} // namespace

namespace native
{
    void panel::create_native() {
        auto *self = this;
        HWND parent_hwnd = resolve_parent(self, "panel");
        register_panel_class();

        // WM_NCCREATE registers the binding, so children can resolve
        // this host before on_wnd_create runs.
        HWND hwnd = CreateWindowExW(0,
                                    panel_class_name,
                                    L"",
                                    WS_CHILD | WS_CLIPCHILDREN,
                                    _bounds.p.x,
                                    _bounds.p.y,
                                    _bounds.d.w,
                                    _bounds.d.h,
                                    parent_hwnd,
                                    nullptr,
                                    GetModuleHandle(nullptr),
                                    self);
        if (!hwnd)
            throw std::runtime_error(
                "Windows: failed to create panel host.");

    }

    void panel::show_native() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(
            this);
        if (!_created || !hwnd)
            throw std::runtime_error("Windows: panel is not created.");
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    void panel::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        HWND hwnd = windows::wnd_bindings.handle_from_object(self);
        if (hwnd) {
            DestroyWindow(hwnd);
            windows::wnd_bindings.unregister_by_handle(hwnd);
        }
    }

    void canvas::create_native() {
        auto *self = this;
        HWND parent_hwnd = resolve_parent(self, "canvas");
        windows::register_window_class();

        HWND hwnd = CreateWindowExW(0,
                                    windows::class_name,
                                    L"",
                                    WS_CHILD | WS_CLIPCHILDREN,
                                    _bounds.p.x,
                                    _bounds.p.y,
                                    _bounds.d.w,
                                    _bounds.d.h,
                                    parent_hwnd,
                                    nullptr,
                                    GetModuleHandle(nullptr),
                                    self);
        if (!hwnd)
            throw std::runtime_error(
                "Windows: failed to create canvas surface.");

        self->synchronize_theme_metrics();
        self->relayout_children();
    }

    void canvas::show_native() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(
            this);
        if (!_created || !hwnd)
            throw std::runtime_error("Windows: canvas is not created.");
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    void canvas::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        HWND hwnd = windows::wnd_bindings.handle_from_object(self);
        if (hwnd) {
            DestroyWindow(hwnd);
            windows::wnd_bindings.unregister_by_handle(hwnd);
        }
    }
} // namespace native

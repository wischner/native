//
// Declares internal Windows shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <native.h>
#include <bindings.h>

namespace windows
{
    // Win32 callbacks carry handles rather than C++ object context, so
    // backend keeps process-wide registries for callback dispatch.
    // Platform handle for a font_t — owns the GDI HFONT.
    struct win_font
    {
        HFONT hfont = nullptr;
    };

    // Graphics cache structure for Windows GDI
    struct win_gpx
    {
        HDC hdc = nullptr;      // Cached device context
        HPEN pen = nullptr;     // Cached pen
        HBRUSH brush = nullptr; // Cached brush

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    };

    struct win_menu
    {
        HMENU hmenu = nullptr;
        native::app_wnd *owner = nullptr;
    };

    struct win_button
    {
        HWND hwnd = nullptr;
        native::button *owner = nullptr;
    };

    struct win_icon_view
    {
        HWND hwnd = nullptr;
        HIMAGELIST images = nullptr;
        int applied_scroll = 0;
        bool suppress = false;
    };

    // Stores native handles and stable identity for a Tree-View.
    struct win_tree_view
    {
        HWND hwnd = nullptr;
        HIMAGELIST images = nullptr;
        WNDPROC original_proc = nullptr;
        std::unordered_map<native::tree_item_id, HTREEITEM> items;
        std::unordered_map<HTREEITEM, native::tree_item_id> ids;
        bool suppress = false;
    };

    // Stores the owner-data/native-group state for a report ListView.
    struct win_table_view
    {
        HWND hwnd = nullptr;
        HIMAGELIST images = nullptr;
        std::unordered_map<const native::img *, int> image_indexes;
        std::vector<native::table_column_id> native_columns;
        bool owner_data = true;
        bool suppress = false;
        int horizontal_offset = 0;
    };

    // Stores the subclass state for one Win32 EDIT control.
    struct win_text_edit
    {
        HWND hwnd = nullptr;
        WNDPROC original_proc = nullptr;
        bool suppress = false;
    };

    extern native::bindings<HWND, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, win_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, win_font *> font_bindings;
    extern native::bindings<uint32_t, win_menu *> menu_bindings;
    extern native::bindings<native::button *, win_button *>
        button_bindings;
    extern native::bindings<native::text_edit *, win_text_edit *>
        text_edit_bindings;
    extern native::bindings<native::icon_view *, win_icon_view *>
        icon_view_bindings;
    extern native::bindings<native::tree_view *, win_tree_view *>
        tree_view_bindings;
    extern native::bindings<native::table_view *, win_table_view *>
        table_view_bindings;
    extern std::unordered_map<native::code_edit *, wchar_t>
        code_edit_high_surrogates;

    // Translate a report ListView or header notification.
    LRESULT handle_table_notify(native::table_view *table,
                                NMHDR *notification);

    // Translate a Tree-View notification into portable tree actions.
    LRESULT handle_tree_notify(native::tree_view *tree,
                               NMHDR *notification);

    // Register the routed window class used by custom native hosts.
    void register_window_class();

    // Name of the routed window class used by Native windows.
    extern const wchar_t class_name[];

    // Validate and cache one EN_CHANGE notification.
    void handle_text_edit_change(native::text_edit *editor);

    // Route Win32 messages to the C++ window registered for a handle.
    LRESULT CALLBACK routed_wnd_proc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

    // Convert a Win32 system color to the public RGBA representation.
    native::rgba rgba_from_sys_color(int idx);

    // Convert UTF-8 text to a Win32 wide string.
    std::wstring utf8_to_wide(const std::string &text);

    // Convert a Win32 wide string to UTF-8 text.
    std::string wide_to_utf8(const std::wstring &text);

    // Convert public rectangle coordinates to an inclusive Win32 RECT.
    RECT to_rect(const native::rect &r);

    // Resolve the native window handle behind a graphics context.
    HWND hwnd_from_gpx(native::gpx &g);

    // Borrow the device context supplied by a native custom-draw
    // notification while portable painting hooks are running.
    class scoped_gpx_dc
    {
    public:
        scoped_gpx_dc(native::gpx &graphics, HDC hdc);
        ~scoped_gpx_dc();

        scoped_gpx_dc(const scoped_gpx_dc &) = delete;
        scoped_gpx_dc &operator=(const scoped_gpx_dc &) = delete;

    private:
        native::gpx &_graphics;
        HDC _previous;
        HDC _borrowed;
        int _saved_state;
    };

    // Acquire and release the effective device context for a portable
    // window graphics object. A scoped custom-draw DC takes precedence.
    HDC acquire_gpx_dc(native::gpx &graphics);
    void release_gpx_dc(native::gpx &graphics, HDC hdc);

    // Return the same system-selected font used by native controls and
    // theme painters.
    HFONT control_font();
} // namespace windows

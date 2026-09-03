//
// Declares internal Haiku shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Button.h>
#include <native.h>
#include <bindings.h>

class BMenuBar;
class BCheckBox;
class BRadioButton;
class BListView;
class BOptionPopUp;
class BTextControl;
class BOutlineListView;
class BListItem;
class BTextView;
class BScrollView;
class BFilePanel;
class BRefFilter;
class BColumn;
class BColumnListView;
class BRow;
class BTabView;
class BSplitView;

namespace haiku
{
    inline constexpr std::uint32_t button_message = 'nbtn';
    inline constexpr const char *control_owner_field =
        "native_control_owner";

    // BeAPI callbacks carry objects, so process-wide registries
    // recover the corresponding C++ objects during event dispatch.
    // Platform handle for a font_t — copies a BFont value.
    struct haiku_font
    {
        BFont bfont;
    };

    // Graphics cache structure for Haiku BeAPI
    struct haiku_gpx
    {
        BView *view = nullptr; // Cached BView for drawing

        // Cached draw parameters
        native::rgba current_fg = {};
        bool current_fg_valid = false;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    };

    // Temporarily route a portable control's graphics operations into the
    // native child view currently asking it to paint a row or cell.
    class scoped_gpx_target
    {
    public:
        scoped_gpx_target(native::wnd &owner, BView *target);
        ~scoped_gpx_target();

        scoped_gpx_target(const scoped_gpx_target &) = delete;
        scoped_gpx_target &operator=(const scoped_gpx_target &) = delete;

    private:
        haiku_gpx *_cache = nullptr;
        BView *_previous = nullptr;
    };

    struct haiku_menu
    {
        native::app_wnd *owner = nullptr;
        BMenuBar *bar = nullptr;
        std::set<int> item_ids;
    };

    struct haiku_button
    {
        BButton *button = nullptr;
        native::button *owner = nullptr;
    };

    // Owns the BeAPI objects associated with one file chooser.
    struct haiku_file_dialog
    {
        BFilePanel *panel = nullptr;
        BRefFilter *filter = nullptr;
        std::string default_extension;
    };

    extern BApplication *global_app;
    extern native::bindings<BWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, haiku_gpx *>
        wnd_gpx_bindings;
    extern native::bindings<uint32_t, haiku_font *> font_bindings;
    extern native::bindings<uint32_t, haiku_menu *> menu_bindings;
    extern native::bindings<native::app_wnd *, haiku_menu *>
        owner_menu_bindings;
    extern native::bindings<native::button *, haiku_button *>
        button_bindings;

    struct haiku_check
    {
        BCheckBox *view = nullptr;
    };
    struct haiku_radio
    {
        BRadioButton *view = nullptr;
    };
    struct haiku_list
    {
        BListView *view = nullptr;
    };
    struct haiku_tab_view
    {
        BView *view = nullptr;
        BTabView *tabs = nullptr;
        std::vector<BView *> pages;
        int visible_page = -1;
    };
    struct haiku_split_view
    {
        BSplitView *view = nullptr;
        BView *first = nullptr;
        BView *second = nullptr;
        bool suppress = false;
    };
    struct haiku_combo_box
    {
        BView *view = nullptr;
        BOptionPopUp *popup = nullptr;
        BTextControl *text = nullptr;
        bool suppress = false;
    };
    struct haiku_collection
    {
        BView *view = nullptr;
        BColumnListView *column_view = nullptr;
        std::vector<BRow *> rows;
        std::vector<native::table_row_id> row_ids;
        std::vector<BRow *> group_rows;
        std::vector<native::table_group_id> group_ids;
        bool native_table = false;
    };

    // A structural panel host and a paintable canvas each own one
    // plain child view; nothing else about them is backend state.
    struct haiku_surface
    {
        BView *view = nullptr;
    };

    struct haiku_tree_view
    {
        BOutlineListView *view = nullptr;
        BScrollView *scroll = nullptr;
        std::unordered_map<native::tree_item_id, BListItem *> items;
    };

    struct haiku_text_edit
    {
        BTextView *view = nullptr;
        BScrollView *scroll = nullptr;
    };

    extern native::bindings<native::panel *, haiku_surface *>
        panel_bindings;
    extern native::bindings<native::canvas *, haiku_surface *>
        canvas_bindings;
    extern native::bindings<native::check *, haiku_check *>
        check_bindings;
    extern native::bindings<native::radio *, haiku_radio *>
        radio_bindings;
    extern native::bindings<native::list *, haiku_list *> list_bindings;
    extern native::bindings<native::tab_view *, haiku_tab_view *>
        tab_view_bindings;
    extern native::bindings<native::split_view *, haiku_split_view *>
        split_view_bindings;
    extern native::bindings<native::combo_box *, haiku_combo_box *>
        combo_box_bindings;
    extern native::bindings<native::accordion *, haiku_collection *>
        accordion_bindings;
    extern native::bindings<native::icon_view *, haiku_collection *>
        icon_view_bindings;
    extern native::bindings<native::tree_view *, haiku_tree_view *>
        tree_view_bindings;
    extern native::bindings<native::table_view *, haiku_collection *>
        table_view_bindings;
    extern native::bindings<native::code_edit *, haiku_collection *>
        code_edit_bindings;
    extern native::bindings<native::text_edit *, haiku_text_edit *>
        text_edit_bindings;
    extern native::bindings<native::file_dialog *, haiku_file_dialog *>
        file_dialog_bindings;

    // Return the root content view owned by a Native application
    // window.
    BView *content_view(BWindow *window);

    // Publish the root content view's actual client dimensions.
    void report_client_dimensions(BWindow *window,
                                  native::app_wnd *owner);

    // Return the BView used by any public child control.
    BView *view_from_control(native::wnd *control);

    // Return the created content view which can own a child control.
    BView *parent_view(native::wnd *parent,
                       native::wnd *child = nullptr);
} // namespace haiku

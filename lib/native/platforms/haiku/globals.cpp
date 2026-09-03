//
// Implements the Haiku shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Application.h>
#include <CheckBox.h>
#include <TabView.h>
#include <SplitView.h>
#include <ListView.h>
#include <OptionPopUp.h>
#include <RadioButton.h>
#include <ScrollView.h>
#include <TextView.h>
#include <View.h>

#include <algorithm>
#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace haiku
{
    // Bind: application object.
    BApplication *global_app = nullptr;
    // Bind: BWindow to wnd.
    native::bindings<BWindow *, native::wnd *> wnd_bindings;
    // Bind: wnd to graphics cache.
    native::bindings<native::wnd *, haiku_gpx *> wnd_gpx_bindings;
    // Bind: font id to platform font handle.
    native::bindings<uint32_t, haiku_font *> font_bindings;
    // Bind: menu id to menu handle.
    native::bindings<uint32_t, haiku_menu *> menu_bindings;
    // Bind: owner app_wnd* to menu handle.
    native::bindings<native::app_wnd *, haiku_menu *>
        owner_menu_bindings;
    // Bind: button owner pointer to native button handle.
    native::bindings<native::button *, haiku_button *> button_bindings;
    // Bind: structural panel host to its container view.
    native::bindings<native::panel *, haiku_surface *> panel_bindings;
    // Bind: paintable canvas to its drawing view.
    native::bindings<native::canvas *, haiku_surface *> canvas_bindings;
    native::bindings<native::check *, haiku_check *> check_bindings;
    native::bindings<native::radio *, haiku_radio *> radio_bindings;
    native::bindings<native::list *, haiku_list *> list_bindings;
    native::bindings<native::tab_view *, haiku_tab_view *>
        tab_view_bindings;
    native::bindings<native::split_view *, haiku_split_view *>
        split_view_bindings;
    native::bindings<native::combo_box *, haiku_combo_box *>
        combo_box_bindings;
    native::bindings<native::accordion *, haiku_collection *>
        accordion_bindings;
    native::bindings<native::icon_view *, haiku_collection *>
        icon_view_bindings;
    native::bindings<native::tree_view *, haiku_tree_view *>
        tree_view_bindings;
    native::bindings<native::table_view *, haiku_collection *>
        table_view_bindings;
    native::bindings<native::code_edit *, haiku_collection *>
        code_edit_bindings;
    native::bindings<native::text_edit *, haiku_text_edit *>
        text_edit_bindings;
    native::bindings<native::file_dialog *, haiku_file_dialog *>
        file_dialog_bindings;

    scoped_gpx_target::scoped_gpx_target(native::wnd &owner,
                                         BView *target) {
        // Construct the owner's cache before replacing its normal host.
        owner.get_gpx();
        _cache = wnd_gpx_bindings.object_from_handle(&owner);
        if (!_cache || !target) {
            _cache = nullptr;
            return;
        }
        _previous = _cache->view;
        _cache->view = target;
        _cache->current_fg_valid = false;
        _cache->current_thickness = -1;
    }

    scoped_gpx_target::~scoped_gpx_target() {
        if (!_cache)
            return;
        _cache->view = _previous;
        _cache->current_fg_valid = false;
        _cache->current_thickness = -1;
    }

    BView *content_view(BWindow *window) {
        return window ? window->ChildAt(0) : nullptr;
    }

    void report_client_dimensions(BWindow *window,
                                  native::app_wnd *owner) {
        if (!window || !owner)
            return;
        BView *content = content_view(window);
        const BRect bounds = content ? content->Bounds()
                                     : window->Bounds();
        const float width = std::max(0.0f, bounds.Width() + 1.0f);
        const float height = std::max(0.0f, bounds.Height() + 1.0f);
        owner->on_native_resize(native::size(
            static_cast<native::dim>(width),
            static_cast<native::dim>(height)));
    }

    BView *view_from_control(native::wnd *control) {
        if (auto *host = dynamic_cast<native::panel *>(control)) {
            auto *binding = panel_bindings.object_from_handle(host);
            return binding ? binding->view : nullptr;
        }
        if (auto *surface = dynamic_cast<native::canvas *>(control)) {
            auto *binding = canvas_bindings.object_from_handle(surface);
            return binding ? binding->view : nullptr;
        }
        if (auto *button = dynamic_cast<native::button *>(control)) {
            auto *binding = button_bindings.object_from_handle(button);
            return binding ? binding->button : nullptr;
        }
        if (auto *check = dynamic_cast<native::check *>(control)) {
            auto *binding = check_bindings.object_from_handle(check);
            return binding ? binding->view : nullptr;
        }
        if (auto *radio = dynamic_cast<native::radio *>(control)) {
            auto *binding = radio_bindings.object_from_handle(radio);
            return binding ? binding->view : nullptr;
        }
        if (auto *list = dynamic_cast<native::list *>(control)) {
            auto *binding = list_bindings.object_from_handle(list);
            return binding ? binding->view : nullptr;
        }
        if (auto *combo = dynamic_cast<native::combo_box *>(control)) {
            auto *binding = combo_box_bindings.object_from_handle(combo);
            return binding ? binding->view : nullptr;
        }
        if (auto *tabs = dynamic_cast<native::tab_view *>(control)) {
            auto *binding = tab_view_bindings.object_from_handle(tabs);
            return binding ? binding->view : nullptr;
        }
        if (auto *split = dynamic_cast<native::split_view *>(control)) {
            auto *binding = split_view_bindings.object_from_handle(split);
            return binding ? binding->view : nullptr;
        }
        if (auto *accordion =
                dynamic_cast<native::accordion *>(control)) {
            auto *binding =
                accordion_bindings.object_from_handle(accordion);
            return binding ? binding->view : nullptr;
        }
        if (auto *icons = dynamic_cast<native::icon_view *>(control)) {
            auto *binding = icon_view_bindings.object_from_handle(icons);
            return binding ? binding->view : nullptr;
        }
        if (auto *tree = dynamic_cast<native::tree_view *>(control)) {
            auto *binding = tree_view_bindings.object_from_handle(tree);
            return binding ? static_cast<BView *>(binding->scroll)
                           : nullptr;
        }
        if (auto *table = dynamic_cast<native::table_view *>(control)) {
            auto *binding = table_view_bindings.object_from_handle(table);
            return binding ? binding->view : nullptr;
        }
        if (auto *editor = dynamic_cast<native::code_edit *>(control)) {
            auto *binding = code_edit_bindings.object_from_handle(editor);
            return binding ? binding->view : nullptr;
        }
        if (auto *editor =
                dynamic_cast<native::text_edit *>(control)) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (!binding)
                return nullptr;
            return binding->scroll
                       ? static_cast<BView *>(binding->scroll)
                       : static_cast<BView *>(binding->view);
        }
        return nullptr;
    }

    BView *parent_view(native::wnd *parent, native::wnd *child) {
        if (!parent || !parent->get_created())
            return nullptr;
        if (auto *tabs = dynamic_cast<native::tab_view *>(parent)) {
            auto *binding = tab_view_bindings.object_from_handle(tabs);
            const int selected = tabs->get_selected_index();
            return binding && selected >= 0 &&
                           selected < static_cast<int>(binding->pages.size())
                       ? binding->pages[static_cast<std::size_t>(selected)]
                       : nullptr;
        }
        if (auto *split = dynamic_cast<native::split_view *>(parent)) {
            auto *binding = split_view_bindings.object_from_handle(split);
            if (!binding)
                return nullptr;
            return child == &split->get_first()
                ? binding->first
                : child == &split->get_second()
                    ? binding->second
                    : nullptr;
        }
        if (BView *view = view_from_control(parent))
            return view;
        return content_view(wnd_bindings.handle_from_object(parent));
    }
} // namespace haiku

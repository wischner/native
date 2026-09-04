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
    // Bind: font id to platform font handle.
    native::bindings<uint32_t, haiku_font *> font_bindings;
    // Bind: menu id to menu handle.
    native::bindings<uint32_t, haiku_menu *> menu_bindings;
    // Bind: owner app_wnd* to menu handle.
    // Bind: button owner pointer to native button handle.
    // Bind: structural panel host to its container view.
    // Bind: paintable canvas to its drawing view.
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
        return control
                   ? static_cast<BView *>(
                         native::detail::wnd_peer_access::content(*control))
                   : nullptr;
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

//
// Implements native XView single-line and multiline text editors with
// deferred complete-value validation and portable clipboard commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/time.h>

#include <native.h>
#include <native/text_edit.h>

#if !__has_include(<xview/ev.h>)
// Tribblix exports textsw_set_selection but omits the private ev.h header
// shipped by the Linux XView build. Ask textsw.h for that declaration and
// supply the stable OpenWindows selection-mask values below.
#define _OTHER_TEXTSW_FUNCTIONS
#define NATIVE_XVIEW_SELECTION_COMPAT
#endif

#include <xview/xview.h>
#if __has_include(<xview/ev.h>)
#include <xview/ev.h>
#endif
#include <xview/notify.h>
#include <xview/panel.h>
#include <xview/sel_pkg.h>
#include <xview/textsw.h>
#include <xview/win_input.h>
#include <xview/window.h>

#include "globals.h"

#ifdef NATIVE_XVIEW_SELECTION_COMPAT
#define EV_SEL_PRIMARY 0x0001
#define EV_SEL_PD_PRIMARY 0x0010
#undef _OTHER_TEXTSW_FUNCTIONS
#undef NATIVE_XVIEW_SELECTION_COMPAT
#endif

namespace
{
    Attr_attribute text_edit_event_key() {
        static const Attr_attribute key = xv_unique_key();
        return key;
    }

    linux::openlook::openlook_text_edit *binding_for(
        native::text_edit *owner) {
        return linux::openlook::text_edit_bindings
            .object_from_handle(owner);
    }

    std::string current_text(Panel_item item) {
        const char *value = reinterpret_cast<const char *>(
            xv_get(item, PANEL_VALUE));
        return value ? value : "";
    }

    Notify_value synchronize(Notify_client client, int) {
        Panel_item item = static_cast<Panel_item>(client);
        auto *owner = reinterpret_cast<native::text_edit *>(
            xv_get(item, PANEL_CLIENT_DATA));
        auto *binding = owner ? binding_for(owner) : nullptr;
        if (!owner || !binding || binding->suppress)
            return NOTIFY_DONE;

        const std::string candidate = current_text(item);
        const bool changed = owner->get_text() != candidate;
        if (owner->on_native_text(candidate)) {
            if (changed)
                binding->all_selected = false;
            return NOTIFY_DONE;
        }

        binding->suppress = true;
        xv_set(item,
               PANEL_VALUE,
               owner->get_text().c_str(),
               nullptr);
        binding->suppress = false;
        return NOTIFY_DONE;
    }

    void schedule_synchronization(Panel_item item) {
        itimerval timer = {};
        timer.it_value.tv_usec = 1;
        notify_set_itimer_func(
            item,
            reinterpret_cast<Notify_func>(synchronize),
            ITIMER_REAL,
            &timer,
            nullptr);
    }

    bool is_shortcut(Event *event) {
        if (!event)
            return false;
        const int identifier = event_id(event);
        return event_ctrl_is_down(event) ||
               identifier < ' ' ||
               identifier > 255;
    }

    bool is_select_all(Event *event) {
        return event && is_shortcut(event) &&
               event_id(event) == 1;
    }

    bool perform_keyboard_command(
        native::text_edit *owner,
        Event *event) {
        if (!owner || !event)
            return false;

        const int action = event_action(event);
        const bool shortcut = is_shortcut(event);
        switch (action) {
        case ACTION_COPY:
            if (!shortcut)
                return false;
            if (event_is_down(event))
                owner->copy();
            return true;
        case ACTION_CUT:
            if (!shortcut)
                return false;
            if (event_is_down(event))
                owner->cut();
            return true;
        case ACTION_PASTE:
            if (!shortcut)
                return false;
            if (event_is_down(event))
                owner->paste();
            return true;
        default:
            return false;
        }
    }

    Notify_value panel_text_event(
        Notify_client client,
        Notify_event event,
        Notify_arg argument,
        Notify_event_type type) {
        Panel panel = static_cast<Panel>(client);
        Panel_item item = static_cast<Panel_item>(xv_get(
            panel, PANEL_CARET_ITEM));
        auto *owner = item
                          ? reinterpret_cast<native::text_edit *>(
                                xv_get(item, PANEL_CLIENT_DATA))
                          : nullptr;
        Event *input = reinterpret_cast<Event *>(event);
        if (owner && !linux::openlook::permit_input(owner))
            return NOTIFY_DONE;
        if (owner && event_is_down(input) &&
            event_action(input) == ACTION_SELECT) {
            auto *binding = binding_for(owner);
            if (binding)
                binding->all_selected = false;
        }
        if (owner && is_select_all(input)) {
            if (event_is_down(input))
                return NOTIFY_DONE;
            const Notify_value result = notify_next_event_func(
                client, event, argument, type);
            owner->select_all();
            return result;
        }
        if (perform_keyboard_command(
                owner, input)) {
            return NOTIFY_DONE;
        }
        return notify_next_event_func(
            client, event, argument, type);
    }

    Notify_value multiline_text_event(
        Notify_client client,
        Notify_event event,
        Notify_arg argument,
        Notify_event_type type) {
        auto *owner = reinterpret_cast<native::text_edit *>(xv_get(
            client, XV_KEY_DATA, text_edit_event_key()));
        Event *input = reinterpret_cast<Event *>(event);
        if (owner && !linux::openlook::permit_input(owner))
            return NOTIFY_DONE;
        if (owner && event_is_down(input) &&
            event_action(input) == ACTION_SELECT) {
            auto *binding = binding_for(owner);
            if (binding)
                binding->all_selected = false;
        }
        if (owner && is_select_all(input)) {
            if (event_is_down(input))
                return NOTIFY_DONE;
            const Notify_value result = notify_next_event_func(
                client, event, argument, type);
            owner->select_all();
            return result;
        }
        if (perform_keyboard_command(
                owner, input)) {
            return NOTIFY_DONE;
        }
        return notify_next_event_func(
            client, event, argument, type);
    }

    void install_event_handler(
        Panel panel,
        Panel_item item,
        bool multiline,
        native::text_edit *owner) {
        if (multiline) {
            Xv_Window view = static_cast<Xv_Window>(xv_get(
                item,
                PANEL_ITEM_NTH_WINDOW,
                static_cast<Attr_attribute>(0)));
            if (!view) {
                throw std::runtime_error(
                    "OpenLook/XView: missing multiline text view.");
            }
            xv_set(view,
                   XV_KEY_DATA,
                   text_edit_event_key(),
                   owner,
                   nullptr);
            if (notify_interpose_event_func(
                    view,
                    reinterpret_cast<Notify_func>(
                        multiline_text_event),
                    NOTIFY_SAFE) != NOTIFY_OK) {
                throw std::runtime_error(
                    "OpenLook/XView: failed to install text handler.");
            }
            return;
        }

        if (xv_get(panel, XV_KEY_DATA, text_edit_event_key()))
            return;
        if (notify_interpose_event_func(
                panel,
                reinterpret_cast<Notify_func>(panel_text_event),
                NOTIFY_SAFE) != NOTIFY_OK) {
            throw std::runtime_error(
                "OpenLook/XView: failed to install text handler.");
        }
        xv_set(panel,
               XV_KEY_DATA,
               text_edit_event_key(),
               TRUE,
               nullptr);
    }

    Panel_setting edited(Panel_item item, Event *event) {
        auto *owner = reinterpret_cast<native::text_edit *>(
            xv_get(item, PANEL_CLIENT_DATA));
        auto *binding = owner ? binding_for(owner) : nullptr;
        if (!owner || !binding || binding->suppress)
            return panel_text_notify(item, event);
        if (!linux::openlook::permit_input(owner))
            return PANEL_NONE;

        const Panel_setting result = panel_text_notify(item, event);
        if (result == PANEL_INSERT)
            schedule_synchronization(item);
        return result;
    }

    std::string primary_selection(Panel panel) {
        Selection_requestor requestor =
            static_cast<Selection_requestor>(xv_create(
                panel,
                SELECTION_REQUESTOR,
                SEL_RANK_NAME,
                "PRIMARY",
                SEL_TYPE_NAME,
                "STRING",
                nullptr));
        if (!requestor)
            return {};
        unsigned long length = 0;
        int format = 0;
        const char *data = reinterpret_cast<const char *>(xv_get(
            requestor, SEL_DATA, &length, &format));
        std::string result;
        if (length != static_cast<unsigned long>(SEL_ERROR) &&
            data && format == 8) {
            result.assign(data, static_cast<std::size_t>(length));
        }
        xv_destroy_safe(requestor);
        return result;
    }

    Textsw multiline_textsw(Panel_item item) {
        Xv_Window view = static_cast<Xv_Window>(xv_get(
            item,
            PANEL_ITEM_NTH_WINDOW,
            static_cast<Attr_attribute>(0)));
        return view
                   ? static_cast<Textsw>(xv_get(view, WIN_PARENT))
                   : XV_NULL;
    }

    void clear_selection(
        linux::openlook::openlook_text_edit *binding) {
        if (!binding->multiline)
            return;
        Textsw textsw = multiline_textsw(binding->item);
        if (textsw) {
            textsw_set_selection(
                textsw,
                TEXTSW_INFINITY,
                TEXTSW_INFINITY,
                EV_SEL_PRIMARY);
        }
    }

    std::size_t replacement_position(
        const std::string &value,
        const std::string &selection,
        Panel_item item,
        bool multiline) {
        if (selection.empty()) {
            if (multiline) {
                Textsw textsw = multiline_textsw(item);
                if (textsw) {
                    return static_cast<std::size_t>(xv_get(
                        textsw, TEXTSW_INSERTION_POINT));
                }
            }
            return value.size();
        }

        const std::size_t first = value.find(selection);
        return first == std::string::npos ? value.size() : first;
    }
} // namespace

namespace native
{
    void text_edit::apply_text() {
        auto *binding = binding_for(this);
        if (!binding || !binding->item) {
            throw std::runtime_error(
                "OpenLook/XView: missing text-edit binding.");
        }
        binding->suppress = true;
        binding->all_selected = false;
        xv_set(binding->item,
               PANEL_VALUE,
               _text.c_str(),
               nullptr);
        clear_selection(binding);
        binding->suppress = false;
    }

    void text_edit::apply_read_only() {
        auto *binding = binding_for(this);
        if (!binding || !binding->item) {
            throw std::runtime_error(
                "OpenLook/XView: missing text-edit binding.");
        }
        xv_set(binding->item,
               PANEL_READ_ONLY,
               _read_only ? TRUE : FALSE,
               nullptr);
    }

    void text_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        Panel panel = linux::openlook::parent_panel(self);
        const bool multiline =
            _mode == text_edit_mode::multi_line;
        Panel_item item = static_cast<Panel_item>(xv_create(
            panel,
            multiline ? PANEL_MULTILINE_TEXT : PANEL_TEXT,
            PANEL_LABEL_STRING,
            "",
            PANEL_VALUE,
            _text.c_str(),
            PANEL_READ_ONLY,
            _read_only ? TRUE : FALSE,
            PANEL_NOTIFY_LEVEL,
            PANEL_ALL,
            PANEL_NOTIFY_PROC,
            edited,
            PANEL_CLIENT_DATA,
            self,
            PANEL_VALUE_STORED_LENGTH,
            65535,
            PANEL_VALUE_DISPLAY_WIDTH,
            _bounds.d.w,
            XV_X,
            _bounds.p.x,
            XV_Y,
            _bounds.p.y,
            XV_WIDTH,
            _bounds.d.w,
            XV_HEIGHT,
            _bounds.d.h,
            XV_SHOW,
            FALSE,
            nullptr));
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create text_edit.");
        }
        try {
            install_event_handler(
                panel, item, multiline, self);
        } catch (...) {
            xv_destroy_safe(item);
            throw;
        }
        auto *binding = new linux::openlook::openlook_text_edit;
        binding->item = item;
        binding->multiline = multiline;
        try {
            linux::openlook::wnd_bindings.register_pair(item, self);
            linux::openlook::text_edit_bindings.register_pair(
                self, binding);
        } catch (...) {
            linux::openlook::wnd_bindings.unregister_by_object(self);
            xv_destroy_safe(item);
            delete binding;
            throw;
        }
        _created = true;
        self->on_native_create();
    }

    void text_edit::show() const {
        auto *binding = binding_for(const_cast<text_edit *>(this));
        if (!_created || !binding || !binding->item) {
            throw std::runtime_error(
                "OpenLook/XView: text_edit is not created.");
        }
        xv_set(binding->item, XV_SHOW, TRUE, nullptr);
        if (binding->multiline) {
            Panel panel = static_cast<Panel>(xv_get(
                binding->item, XV_OWNER));
            xv_set(panel, WIN_UNGRAB_SELECT, nullptr);
        }
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding = binding_for(self);
        self->on_native_destroy();
        if (binding) {
            linux::openlook::wnd_bindings.unregister_by_handle(
                binding->item);
            xv_destroy_safe(binding->item);
            linux::openlook::text_edit_bindings
                .unregister_by_handle(self);
            delete binding;
        }
    }

    std::string text_edit::selected_text() const {
        auto *binding = binding_for(const_cast<text_edit *>(this));
        if (!binding || !binding->item)
            return {};
        if (binding->all_selected)
            return current_text(binding->item);
        Panel panel = static_cast<Panel>(xv_get(
            binding->item, XV_OWNER));
        return primary_selection(panel);
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        auto *binding = binding_for(this);
        if (!binding || !binding->item || _read_only)
            return false;

        const std::string selection = selected_text();
        const std::string value = current_text(binding->item);
        const std::size_t begin = binding->all_selected
                                      ? 0
                                      : replacement_position(
                                            value,
                                            selection,
                                            binding->item,
                                            binding->multiline);
        std::string candidate = value;
        const std::size_t count = selection.empty()
                                      ? 0
                                      : selection.size();
        candidate.replace(begin, count, text);
        if (!validate(candidate))
            return false;

        binding->suppress = true;
        binding->all_selected = false;
        xv_set(binding->item,
               PANEL_VALUE,
               candidate.c_str(),
               nullptr);
        clear_selection(binding);
        binding->suppress = false;
        return on_native_text(candidate);
    }

    void text_edit::select_all_native() const {
        auto *binding = binding_for(const_cast<text_edit *>(this));
        if (!binding || !binding->item)
            return;
        binding->all_selected = true;
        if (binding->multiline) {
            Textsw textsw = multiline_textsw(binding->item);
            if (textsw) {
                textsw_set_selection(
                    textsw,
                    0,
                    static_cast<Textsw_index>(get_text().size()),
                    EV_SEL_PRIMARY | EV_SEL_PD_PRIMARY);
            }
        } else {
            xv_set(binding->item, PANEL_TEXT_SELECT_LINE, nullptr);
        }
    }
} // namespace native

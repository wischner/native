//
// Implements a focusable Haiku-native host for custom collections.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "collection_view.h"

#include <InterfaceDefs.h>
#include <Message.h>
#include <View.h>
#include <Window.h>

#include <stdexcept>
#include <string>

#include <native.h>

#include "globals.h"

namespace
{
    class collection_view final : public BView
    {
    public:
        collection_view(native::wnd &owner, BRect frame)
            : BView(frame,
                    "native_collection",
                    B_FOLLOW_NONE,
                    B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE)
            , _owner(owner) {
            SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        }

        void Draw(BRect update) override {
            // AddChild may schedule the first draw before the portable
            // create path has registered its backend binding. The next
            // invalidation after creation paints through the complete
            // lifecycle state.
            if (!_owner.get_created())
                return;
            native::rect invalid(
                static_cast<native::coord>(update.left),
                static_cast<native::coord>(update.top),
                static_cast<native::dim>(update.Width() + 1),
                static_cast<native::dim>(update.Height() + 1));
            auto &graphics = _owner.get_gpx().set_clip(invalid);
            native::wnd_paint_event event(invalid, graphics);
            _owner.on_wnd_paint.emit(event);
        }

        void MouseDown(BPoint where) override {
            MakeFocus(true);
            _owner.on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y))));
            int32 clicks = 1;
            if (BMessage *message = Window()->CurrentMessage())
                message->FindInt32("clicks", &clicks);
            if (clicks >= 2) {
                if (auto *table =
                        dynamic_cast<native::table_view *>(&_owner)) {
                    const auto selected = table->get_selected_rows();
                    if (!selected.empty())
                        table->on_native_activate(selected.back());
                }
                if (auto *icons =
                        dynamic_cast<native::icon_view *>(&_owner)) {
                    icons->on_native_activate(icons->item_at(
                        native::point(
                            static_cast<native::coord>(where.x),
                            static_cast<native::coord>(where.y))));
                }
            }
        }

        void MouseUp(BPoint where) override {
            _owner.on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y))));
        }

        void MouseMoved(BPoint where,
                        uint32 transit,
                        const BMessage *message) override {
            (void)transit;
            (void)message;
            _owner.on_mouse_move.emit(native::point(
                static_cast<native::coord>(where.x),
                static_cast<native::coord>(where.y)));
        }

        void MessageReceived(BMessage *message) override {
            if (message && message->what == B_MOUSE_WHEEL_CHANGED) {
                float delta = 0.0f;
                message->FindFloat("be:wheel_delta_y", &delta);
                _owner.on_mouse_wheel.emit(native::mouse_wheel_event(
                    native::point(),
                    static_cast<native::coord>(delta * 24.0f),
                    native::wheel_direction::vertical));
                return;
            }
            BView::MessageReceived(message);
        }

        void MakeFocus(bool focused = true) override {
            BView::MakeFocus(focused);
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(&_owner))
                accordion->on_native_focus(focused);
            if (auto *icons =
                    dynamic_cast<native::icon_view *>(&_owner))
                icons->on_native_focus(focused);
            if (auto *table =
                    dynamic_cast<native::table_view *>(&_owner))
                table->on_native_focus(focused);
            if (auto *editor =
                    dynamic_cast<native::code_edit *>(&_owner))
                editor->on_native_focus(focused);
        }

        void KeyDown(const char *bytes, int32 count) override {
            if (!bytes || count <= 0)
                return;
            const char key = bytes[0];
            if (auto *editor =
                    dynamic_cast<native::code_edit *>(&_owner)) {
                const uint32 keys = modifiers();
                const bool extend = (keys & B_SHIFT_KEY) != 0;
                const bool command =
                    (keys & (B_COMMAND_KEY | B_CONTROL_KEY)) != 0;
                if (command) {
                    const char lower = key >= 'A' && key <= 'Z'
                                           ? key - 'A' + 'a'
                                           : key;
                    if (lower == 'a')
                        editor->on_native_key(
                            native::code_edit_key::select_all);
                    else if (lower == 'c')
                        editor->on_native_key(
                            native::code_edit_key::copy);
                    else if (lower == 'x')
                        editor->on_native_key(
                            native::code_edit_key::cut);
                    else if (lower == 'v')
                        editor->on_native_key(
                            native::code_edit_key::paste);
                    else if (lower == 'z')
                        editor->on_native_key(
                            extend ? native::code_edit_key::redo
                                   : native::code_edit_key::undo);
                    else
                        BView::KeyDown(bytes, count);
                    return;
                }
                native::code_edit_key command_key;
                bool handled = true;
                if (key == B_LEFT_ARROW)
                    command_key = native::code_edit_key::left;
                else if (key == B_RIGHT_ARROW)
                    command_key = native::code_edit_key::right;
                else if (key == B_UP_ARROW)
                    command_key = native::code_edit_key::up;
                else if (key == B_DOWN_ARROW)
                    command_key = native::code_edit_key::down;
                else if (key == B_HOME)
                    command_key = native::code_edit_key::home;
                else if (key == B_END)
                    command_key = native::code_edit_key::end;
                else if (key == B_PAGE_UP)
                    command_key = native::code_edit_key::page_up;
                else if (key == B_PAGE_DOWN)
                    command_key = native::code_edit_key::page_down;
                else if (key == 8)
                    command_key = native::code_edit_key::backspace;
                else if (static_cast<unsigned char>(key) == 0x7f)
                    command_key =
                        native::code_edit_key::delete_forward;
                else if (key == B_ENTER)
                    command_key = native::code_edit_key::enter;
                else if (key == '\t')
                    command_key = native::code_edit_key::tab;
                else if (key == 27)
                    command_key = native::code_edit_key::escape;
                else
                    handled = false;
                if (handled)
                    editor->on_native_key(command_key, extend);
                else
                    editor->on_native_text_input(std::string(
                        bytes, static_cast<std::size_t>(count)));
                return;
            }
            if (auto *table =
                    dynamic_cast<native::table_view *>(&_owner)) {
                int32 modifier_value = 0;
                if (BMessage *message = Window()->CurrentMessage()) {
                    message->FindInt32("modifiers", &modifier_value);
                }
                const uint32 modifiers =
                    static_cast<uint32>(modifier_value);
                const bool extend = (modifiers & B_SHIFT_KEY) != 0;
                if ((modifiers & B_COMMAND_KEY) != 0 &&
                    (key == 'a' || key == 'A')) {
                    table->on_native_navigation(
                        native::table_navigation::select_all);
                } else if (key == B_UP_ARROW) {
                    table->on_native_navigation(
                        native::table_navigation::up, extend);
                } else if (key == B_DOWN_ARROW) {
                    table->on_native_navigation(
                        native::table_navigation::down, extend);
                } else if (key == B_HOME) {
                    table->on_native_navigation(
                        native::table_navigation::home, extend);
                } else if (key == B_END) {
                    table->on_native_navigation(
                        native::table_navigation::end, extend);
                } else if (key == B_PAGE_UP) {
                    table->on_native_navigation(
                        native::table_navigation::page_up, extend);
                } else if (key == B_PAGE_DOWN) {
                    table->on_native_navigation(
                        native::table_navigation::page_down, extend);
                } else if (key == B_LEFT_ARROW) {
                    table->on_native_navigation(
                        native::table_navigation::collapse);
                } else if (key == B_RIGHT_ARROW) {
                    table->on_native_navigation(
                        native::table_navigation::expand);
                } else if (key == B_SPACE) {
                    table->on_native_navigation(
                        native::table_navigation::toggle);
                } else if (key == B_ENTER) {
                    table->on_native_navigation(
                        native::table_navigation::activate);
                } else if (static_cast<unsigned char>(key) >= 0x20 &&
                           (modifiers & (B_COMMAND_KEY |
                                         B_CONTROL_KEY)) == 0) {
                    table->on_native_type_text(std::string(
                        bytes, static_cast<std::size_t>(count)));
                } else {
                    BView::KeyDown(bytes, count);
                }
                return;
            }
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(&_owner)) {
                if (key == B_UP_ARROW)
                    accordion->on_native_navigation(
                        native::accordion_navigation::previous);
                else if (key == B_DOWN_ARROW)
                    accordion->on_native_navigation(
                        native::accordion_navigation::next);
                else if (key == B_HOME)
                    accordion->on_native_navigation(
                        native::accordion_navigation::first);
                else if (key == B_END)
                    accordion->on_native_navigation(
                        native::accordion_navigation::last);
                else if (key == B_ENTER || key == B_SPACE)
                    accordion->on_native_navigation(
                        native::accordion_navigation::toggle);
                else
                    BView::KeyDown(bytes, count);
                return;
            }
            auto *icons = dynamic_cast<native::icon_view *>(&_owner);
            if (!icons)
                return;
            if (key == B_LEFT_ARROW)
                icons->on_native_navigation(
                    native::icon_view_navigation::left);
            else if (key == B_RIGHT_ARROW)
                icons->on_native_navigation(
                    native::icon_view_navigation::right);
            else if (key == B_UP_ARROW)
                icons->on_native_navigation(
                    native::icon_view_navigation::up);
            else if (key == B_DOWN_ARROW)
                icons->on_native_navigation(
                    native::icon_view_navigation::down);
            else if (key == B_HOME)
                icons->on_native_navigation(
                    native::icon_view_navigation::home);
            else if (key == B_END)
                icons->on_native_navigation(
                    native::icon_view_navigation::end);
            else if (key == B_PAGE_UP)
                icons->on_native_navigation(
                    native::icon_view_navigation::page_up);
            else if (key == B_PAGE_DOWN)
                icons->on_native_navigation(
                    native::icon_view_navigation::page_down);
            else if (key == B_ENTER)
                icons->on_native_activate(icons->get_selected_index());
            else
                BView::KeyDown(bytes, count);
        }

    private:
        native::wnd &_owner;
    };
} // namespace

namespace haiku
{
    BView *create_collection_view(native::wnd &owner) {
        BView *parent = parent_view(owner.get_parent());
        BWindow *window = parent ? parent->Window() : nullptr;
        if (!parent || !window)
            throw std::runtime_error(
                "Haiku: collection requires a created parent.");
        const native::rect bounds = owner.get_bounds();
        BView *view = nullptr;
        const bool locked = window->IsLocked();
        if (!locked && !window->Lock())
            throw std::runtime_error(
                "Haiku: failed to lock collection parent.");
        view = new collection_view(
            owner,
            BRect(bounds.p.x,
                  bounds.p.y,
                  bounds.x2() - 1,
                  bounds.y2() - 1));
        view->Hide();
        parent->AddChild(view);
        if (!locked)
            window->Unlock();
        return view;
    }
} // namespace haiku

//
// Implements the Haiku native-window bridge backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "native_window.h"

#include <interface/View.h>
#include <Application.h>
#include <AppDefs.h>
#include <InterfaceDefs.h>

#include <native.h>

#include "globals.h"

namespace
{
    native::mouse_button decode_button(uint32 buttons) {
        if (buttons & B_PRIMARY_MOUSE_BUTTON)
            return native::mouse_button::left;
        if (buttons & B_SECONDARY_MOUSE_BUTTON)
            return native::mouse_button::right;
        if (buttons & B_TERTIARY_MOUSE_BUTTON)
            return native::mouse_button::middle;
        return native::mouse_button::none;
    }

    class native_view : public BView
    {
    public:
        explicit native_view(native::app_wnd *owner, BRect frame)
            : BView(frame,
                    "native_canvas",
                    B_FOLLOW_ALL,
                    B_WILL_DRAW | B_FRAME_EVENTS)
            , _owner(owner)
            , _pressed_button(native::mouse_button::none) {
            SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        }

        void Draw(BRect update_rect) override {
            if (!_owner || !_owner->get_created())
                return;

            native::rect r(
                static_cast<native::coord>(update_rect.left),
                static_cast<native::coord>(update_rect.top),
                static_cast<native::dim>(update_rect.Width() + 1),
                static_cast<native::dim>(update_rect.Height() + 1));

            auto &g = _owner->get_gpx().set_clip(r);
            const rgb_color background =
                ui_color(B_PANEL_BACKGROUND_COLOR);
            g.clear(native::rgba(background.red,
                                 background.green,
                                 background.blue,
                                 background.alpha));

            native::wnd_paint_event e{r, g};
            _owner->on_native_paint(e);
        }

        void
        MouseMoved(BPoint where, uint32, const BMessage *) override {
            if (!_owner || !_owner->get_input_enabled())
                return;

            BPoint screen = where;
            ConvertToScreen(&screen);
            _owner->on_native_mouse_move(
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y)),
                native::point(static_cast<native::coord>(screen.x),
                              static_cast<native::coord>(screen.y)));
        }

        void MouseDown(BPoint where) override {
            if (!_owner || !_owner->get_input_enabled())
                return;

            // Keep receiving move/up events for drag interactions such
            // as the painter sample while the mouse button is held.
            SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);

            uint32 buttons = 0;
            if (BMessage *msg = Window()->CurrentMessage()) {
                int32 value = 0;
                if (msg->FindInt32("buttons", &value) == B_OK)
                    buttons = static_cast<uint32>(value);
            }
            if (buttons == 0)
                buttons = B_PRIMARY_MOUSE_BUTTON;

            _pressed_button = decode_button(buttons);
            if (_pressed_button == native::mouse_button::none)
                return;

            _owner->on_native_mouse_click(native::mouse_event(
                _pressed_button,
                native::mouse_action::press,
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y))));
        }

        void MouseUp(BPoint where) override {
            if (!_owner || !_owner->get_input_enabled() ||
                _pressed_button == native::mouse_button::none)
                return;

            _owner->on_native_mouse_click(native::mouse_event(
                _pressed_button,
                native::mouse_action::release,
                native::point(static_cast<native::coord>(where.x),
                              static_cast<native::coord>(where.y))));

            _pressed_button = native::mouse_button::none;
        }

    private:
        native::app_wnd *_owner;
        native::mouse_button _pressed_button;
    };
} // namespace

namespace haiku
{
    native_window::native_window(native::app_wnd *owner,
                                 BRect frame,
                                 const char *title,
                                 window_look look,
                                 window_feel feel)
        : BWindow(frame,
                  title,
                  look,
                  feel,
                  B_ASYNCHRONOUS_CONTROLS)
        , _owner(owner) {
        AddChild(new native_view(owner, Bounds()));
        wnd_bindings.register_pair(this, owner);
    }

    bool native_window::QuitRequested() {
        native::app_wnd *owner = _owner;
        _owner = nullptr;
        if (owner) {
            owner->on_native_destroy();
            wnd_bindings.unregister_by_handle(this);
        }

        if (owner == native::app::main_wnd() && be_app)
            be_app->Quit();

        return true;
    }

    void native_window::MessageReceived(BMessage *message) {
        if (message && message->what == haiku::button_message) {
            void *pointer = nullptr;
            if (message->FindPointer(haiku::control_owner_field,
                                     &pointer) == B_OK) {
                auto *owner = static_cast<native::button *>(pointer);
                auto *binding =
                    haiku::button_bindings.object_from_handle(owner);
                if (binding && binding->button &&
                    binding->button->Window() == this &&
                    owner->get_input_enabled()) {
                    owner->on_native_click();
                }
            }
            return;
        }

        // Check if this is a menu item message for our owner.
        if (message && _owner && _owner->get_input_enabled()) {
            auto *hm =
                haiku::owner_menu_bindings.object_from_handle(_owner);
            if (hm &&
                hm->item_ids.count(static_cast<int>(message->what))) {
                _owner->on_native_menu(
                    static_cast<int>(message->what));
                return;
            }
        }

        if (message && message->what == B_MOUSE_WHEEL_CHANGED &&
            _owner && _owner->get_input_enabled()) {
            float dx = 0.0f;
            float dy = 0.0f;
            message->FindFloat("be:wheel_delta_x", &dx);
            message->FindFloat("be:wheel_delta_y", &dy);

            BPoint where(0.0f, 0.0f);
            uint32 buttons = 0;
            if (ChildAt(0))
                ChildAt(0)->GetMouse(&where, &buttons, false);

            if (dx != 0.0f) {
                _owner->on_native_mouse_wheel(
                    native::mouse_wheel_event(
                    native::point(static_cast<native::coord>(where.x),
                                  static_cast<native::coord>(where.y)),
                    static_cast<native::coord>(dx * 120.0f),
                    native::wheel_direction::horizontal));
            }

            if (dy != 0.0f) {
                _owner->on_native_mouse_wheel(
                    native::mouse_wheel_event(
                    native::point(static_cast<native::coord>(where.x),
                                  static_cast<native::coord>(where.y)),
                    static_cast<native::coord>(dy * 120.0f),
                    native::wheel_direction::vertical));
            }
            return;
        }

        BWindow::MessageReceived(message);
    }

    void native_window::FrameMoved(BPoint new_position) {
        if (_owner) {
            native::point position(
                static_cast<native::coord>(new_position.x),
                static_cast<native::coord>(new_position.y));
            _owner->on_native_move(position);
        }

        BWindow::FrameMoved(new_position);
    }

    void native_window::FrameResized(float new_width,
                                     float new_height) {
        if (_owner) {
            native::size s(static_cast<native::dim>(new_width + 1.0f),
                           static_cast<native::dim>(new_height + 1.0f));
            _owner->on_native_resize(s);
        }

        BWindow::FrameResized(new_width, new_height);
    }

    native::app_wnd *native_window::owner() const {
        return _owner;
    }
} // namespace haiku

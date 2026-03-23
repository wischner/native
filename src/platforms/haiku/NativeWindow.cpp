#include "NativeWindow.h"

#include <interface/View.h>
#include <Application.h>
#include <AppDefs.h>

#include <native.h>

#include "globals.h"

namespace
{
    native::mouse_button decode_button(uint32 buttons)
    {
        if (buttons & B_PRIMARY_MOUSE_BUTTON)
            return native::mouse_button::left;
        if (buttons & B_SECONDARY_MOUSE_BUTTON)
            return native::mouse_button::right;
        if (buttons & B_TERTIARY_MOUSE_BUTTON)
            return native::mouse_button::middle;
        return native::mouse_button::none;
    }

    class NativeView : public BView
    {
    public:
        explicit NativeView(native::app_wnd *owner, BRect frame)
            : BView(frame, "native_canvas", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS),
              _owner(owner),
              _pressed_button(native::mouse_button::none)
        {
            SetViewColor(B_TRANSPARENT_COLOR);
        }

        void Draw(BRect update_rect) override
        {
            if (!_owner)
                return;

            native::rect r(
                static_cast<native::coord>(update_rect.left),
                static_cast<native::coord>(update_rect.top),
                static_cast<native::dim>(update_rect.Width() + 1),
                static_cast<native::dim>(update_rect.Height() + 1));

            auto &g = _owner->get_gpx().set_clip(r);
            g.clear(native::rgba(255, 255, 255, 255));

            native::wnd_paint_event e{r, g};
            _owner->on_wnd_paint.emit(e);
        }

        void MouseMoved(BPoint where, uint32, const BMessage *) override
        {
            if (!_owner)
                return;

            _owner->on_mouse_move.emit(
                native::point(
                    static_cast<native::coord>(where.x),
                    static_cast<native::coord>(where.y)));
        }

        void MouseDown(BPoint where) override
        {
            if (!_owner)
                return;

            uint32 buttons = 0;
            if (BMessage *msg = Window()->CurrentMessage())
            {
                int32 value = 0;
                if (msg->FindInt32("buttons", &value) == B_OK)
                    buttons = static_cast<uint32>(value);
            }
            if (buttons == 0)
                buttons = B_PRIMARY_MOUSE_BUTTON;

            _pressed_button = decode_button(buttons);
            if (_pressed_button == native::mouse_button::none)
                return;

            _owner->on_mouse_click.emit(
                native::mouse_event(
                    _pressed_button,
                    native::mouse_action::press,
                    native::point(
                        static_cast<native::coord>(where.x),
                        static_cast<native::coord>(where.y))));
        }

        void MouseUp(BPoint where) override
        {
            if (!_owner || _pressed_button == native::mouse_button::none)
                return;

            _owner->on_mouse_click.emit(
                native::mouse_event(
                    _pressed_button,
                    native::mouse_action::release,
                    native::point(
                        static_cast<native::coord>(where.x),
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
    NativeWindow::NativeWindow(native::app_wnd *owner, BRect frame, const char *title)
        : BWindow(frame, title, B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE),
          _owner(owner)
    {
        AddChild(new NativeView(owner, Bounds()));
        wnd_bindings.register_pair(this, owner);
    }

    bool NativeWindow::QuitRequested()
    {
        if (_owner)
        {
            if (auto *cache = wnd_gpx_bindings.from_a(_owner))
            {
                delete cache;
                wnd_gpx_bindings.unregister_by_a(_owner);
            }
            wnd_bindings.unregister_by_a(this);
        }

        be_app->PostMessage(B_QUIT_REQUESTED);
        return true;
    }

    void NativeWindow::MessageReceived(BMessage *message)
    {
        if (message && message->what == B_MOUSE_WHEEL_CHANGED && _owner)
        {
            float dx = 0.0f;
            float dy = 0.0f;
            message->FindFloat("be:wheel_delta_x", &dx);
            message->FindFloat("be:wheel_delta_y", &dy);

            BPoint where(0.0f, 0.0f);
            uint32 buttons = 0;
            if (ChildAt(0))
                ChildAt(0)->GetMouse(&where, &buttons, false);

            if (dx != 0.0f)
            {
                _owner->on_mouse_wheel.emit(
                    native::mouse_wheel_event(
                        native::point(static_cast<native::coord>(where.x), static_cast<native::coord>(where.y)),
                        static_cast<native::coord>(dx * 120.0f),
                        native::wheel_direction::horizontal));
            }

            if (dy != 0.0f)
            {
                _owner->on_mouse_wheel.emit(
                    native::mouse_wheel_event(
                        native::point(static_cast<native::coord>(where.x), static_cast<native::coord>(where.y)),
                        static_cast<native::coord>(dy * 120.0f),
                        native::wheel_direction::vertical));
            }
            return;
        }

        BWindow::MessageReceived(message);
    }

    void NativeWindow::FrameMoved(BPoint new_position)
    {
        if (_owner)
        {
            _owner->on_wnd_move.emit(
                native::point(
                    static_cast<native::coord>(new_position.x),
                    static_cast<native::coord>(new_position.y)));
        }

        BWindow::FrameMoved(new_position);
    }

    void NativeWindow::FrameResized(float new_width, float new_height)
    {
        if (_owner)
        {
            _owner->on_wnd_resize.emit(
                native::size(
                    static_cast<native::dim>(new_width + 1.0f),
                    static_cast<native::dim>(new_height + 1.0f)));
        }

        BWindow::FrameResized(new_width, new_height);
    }

    native::app_wnd *NativeWindow::owner() const
    {
        return _owner;
    }
} // namespace haiku

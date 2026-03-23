#pragma once

#include <Window.h>
#include <Rect.h>
#include <Message.h>

namespace native
{
    class app_wnd;
}

namespace haiku
{

    class NativeWindow : public BWindow
    {
    public:
        NativeWindow(native::app_wnd *owner, BRect frame, const char *title);
        virtual bool QuitRequested() override;
        virtual void MessageReceived(BMessage *message) override;
        virtual void FrameMoved(BPoint new_position) override;
        virtual void FrameResized(float new_width, float new_height) override;

        native::app_wnd *owner() const;

    private:
        native::app_wnd *_owner;
    };

} // namespace haiku

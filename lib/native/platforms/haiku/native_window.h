//
// Declares internal Haiku native-window bridge types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

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

    class native_window : public BWindow
    {
    public:
        // Create a BeAPI window that forwards events to its owner.
        native_window(native::app_wnd *owner,
                      BRect frame,
                      const char *title,
                      window_look look,
                      window_feel feel);
        // Handle the BeAPI close request and stop the application loop.
        bool QuitRequested() override;

        // Translate BeAPI menu and wheel messages to native signals.
        void MessageReceived(BMessage *message) override;

        // Forward a BeAPI movement notification to the owner.
        void FrameMoved(BPoint new_position) override;

        // Forward a BeAPI resize notification to the owner.
        void FrameResized(float new_width, float new_height) override;

        // Return the non-owning application-window owner.
        native::app_wnd *owner() const;

    private:
        native::app_wnd *_owner;
    };

} // namespace haiku

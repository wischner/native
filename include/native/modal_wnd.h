//
// Declares an independently positioned owned dialog that blocks its
// owner until the dialog is closed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "owned_wnd.h"

namespace native
{
    // Describes how a modal dialog session ended.
    enum class dialog_result
    {
        none,
        accepted,
        cancelled
    };

    // Represents a modal owned top-level dialog window.
    class modal_wnd : public owned_wnd
    {
    public:
        // Construct a modal window from scalar screen bounds.
        modal_wnd(app_wnd &owner,
                  std::string title,
                  coord x = 100,
                  coord y = 100,
                  dim width = 640,
                  dim height = 480);

        // Construct a modal window from screen position and size.
        modal_wnd(app_wnd &owner,
                  const std::string &title,
                  const point &position,
                  const size &dimensions);

        // Construct a modal window from complete screen bounds.
        modal_wnd(app_wnd &owner,
                  const std::string &title,
                  const rect &bounds);

        // Close an active modal session before base destruction.
        ~modal_wnd() override;

        // Return true for backend selection of native modal behavior.
        bool get_modal() const override;

        // Return whether this dialog currently blocks its owner.
        bool get_modal_active() const;

        // Return the current or most recent dialog result.
        dialog_result get_result() const;

        // Show the dialog and begin its owner-modal session.
        void show() const override;

        // Destroy the dialog and cancel an unfinished modal session.
        void destroy() const override;

        // End modality when the toolkit destroys the native dialog.
        void on_native_destroy() override;

        // End the dialog with an explicit accepted or cancelled result.
        void close(dialog_result result) const;

        // Notify completion of one native modal session.
        virtual void on_native_modal_close(dialog_result result);

        // Emits once when an active modal session has ended.
        signal<dialog_result> on_modal_close;

    protected:
        // Start modality for a derived native system-panel adapter.
        void begin_modal_session() const;

    private:
        mutable bool _modal_active;
        mutable dialog_result _result;

        // Unregister this dialog and optionally emit its result.
        bool end_modal_session() const;
    };
} // namespace native

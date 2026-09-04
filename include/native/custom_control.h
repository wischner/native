//
// Declares shared state for controls whose client surface is painted and
// whose semantic input is routed by Native on emulated backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "theme.h"
#include "wnd.h"

namespace native
{
    // Supplies common focus and theme synchronization to painted controls.
    class custom_control : public wnd
    {
    public:
        // Destroy shared custom-control state.
        ~custom_control() override;

        // Return whether the control currently has keyboard focus.
        bool get_focused() const;

        // Cache a backend focus transition and repaint the control.
        void on_native_focus(bool focused) override;

    protected:
        using wnd::wnd;

        // Refresh the cached metrics from the active backend theme.
        virtual void synchronize_theme_metrics();

        theme::metrics _theme_metrics;
        bool _focused = false;
    };
} // namespace native

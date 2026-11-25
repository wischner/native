//
// Keeps Xt shell icon pixmaps in the window peer. Athena and Motif
// share the WMShell iconPixmap/iconMask resources and EWMH images.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "xt_shell_icon.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include <native/wnd.h>

#include "../wnd_peer.h"
#include "x_window_icon.h"

namespace
{
    // Server resources retained until replacement or destruction.
    struct xt_shell_icon_state
    {
        Display *display = nullptr;
        native::detail::x_icon_pixmaps pixmaps;
    };
} // namespace

namespace native::detail
{
    void apply_xt_shell_icon(wnd &owner, Widget shell,
                             const img *icon) {
        if (!shell)
            return;
        Display *display = XtDisplay(shell);
        const int screen = XScreenNumberOfScreen(XtScreen(shell));
        auto *state = peer_state<xt_shell_icon_state>(owner);
        if (!state) {
            state = new xt_shell_icon_state;
            assign_peer_state(owner, state);
        }
        x_icon_pixmaps previous = state->pixmaps;
        state->display = display;
        state->pixmaps = icon ? create_x_icon_pixmaps(
                                    display, screen, *icon,
                                    x_icon_edge(display, screen))
                              : x_icon_pixmaps{};
        // Leave untouched hints alone when no icon was ever supplied.
        if (previous.image || state->pixmaps.image)
            XtVaSetValues(shell,
                          XtNiconPixmap, state->pixmaps.image,
                          XtNiconMask, state->pixmaps.mask,
                          nullptr);
        if (XtIsRealized(shell)) {
            if (!state->pixmaps.image)
                clear_x_icon_hints(display, XtWindow(shell));
            set_x_net_wm_icon(display, XtWindow(shell), icon);
        }
        free_x_icon_pixmaps(display, previous);
    }

    void publish_xt_shell_icon(Widget shell, const img *icon) {
        if (shell && XtIsRealized(shell))
            set_x_net_wm_icon(XtDisplay(shell), XtWindow(shell), icon);
    }

    void release_xt_shell_icon(wnd &owner) {
        if (auto *state = peer_state<xt_shell_icon_state>(owner))
            free_x_icon_pixmaps(state->display, state->pixmaps);
    }
} // namespace native::detail

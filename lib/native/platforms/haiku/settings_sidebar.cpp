//
// Composes Haiku settings navigation from the native outline list, with
// a framed viewport and equal panel margins around its scroll view.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../picker_peer.h"

namespace native::detail
{
    bool create_settings_sidebar_peer(settings_sidebar &) { return false; }
    bool settings_sidebar_uses_native_search() { return false; }
    bool settings_sidebar_uses_list() { return false; }
    int settings_sidebar_navigation_padding() { return 6; }
    void refresh_settings_sidebar_peer(settings_sidebar &) {}
}

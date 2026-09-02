//
// Implements backend-neutral directory-dialog configuration.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <utility>

#include <native/directory_dialog.h>

namespace native
{
    directory_dialog::directory_dialog(
        app_wnd &owner, std::string title)
        : file_dialog(owner, std::move(title)) {}

    bool directory_dialog::get_allow_multiple() const {
        return _allow_multiple;
    }

    directory_dialog &directory_dialog::set_allow_multiple(
        bool allow_multiple) {
        _allow_multiple = allow_multiple;
        return *this;
    }
} // namespace native

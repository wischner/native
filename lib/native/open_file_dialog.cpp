//
// Implements the backend-neutral configuration of the native file-open
// dialog. Selected backends provide its show operation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include <utility>

namespace native
{
    open_file_dialog::open_file_dialog(
        app_wnd &owner, std::string title)
        : file_dialog(owner, std::move(title))
        , _allow_multiple(false) {}

    bool open_file_dialog::get_allow_multiple() const {
        return _allow_multiple;
    }

    open_file_dialog &open_file_dialog::set_allow_multiple(
        bool allow_multiple) {
        _allow_multiple = allow_multiple;
        return *this;
    }
} // namespace native

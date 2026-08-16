//
// Implements backend-neutral configuration of the native file-save
// dialog. Selected backends provide its show operation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

#include <utility>

namespace native
{
    save_file_dialog::save_file_dialog(
        app_wnd &owner, std::string title)
        : file_dialog(owner, std::move(title))
        , _confirm_overwrite(true) {}

    const std::string &save_file_dialog::get_suggested_name() const {
        return _suggested_name;
    }

    save_file_dialog &save_file_dialog::set_suggested_name(
        const std::string &name) {
        _suggested_name = name;
        return *this;
    }

    const std::string &
    save_file_dialog::get_default_extension() const {
        return _default_extension;
    }

    save_file_dialog &save_file_dialog::set_default_extension(
        const std::string &extension) {
        _default_extension = extension;
        return *this;
    }

    bool save_file_dialog::get_confirm_overwrite() const {
        return _confirm_overwrite;
    }

    save_file_dialog &save_file_dialog::set_confirm_overwrite(
        bool confirm) {
        _confirm_overwrite = confirm;
        return *this;
    }
} // namespace native

//
// Implements backend-neutral file chooser state, logical resource
// lifetime, and owner-modal completion behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/file_dialog.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace native
{
    file_dialog::file_dialog(app_wnd &owner, std::string title)
        : modal_wnd(owner, std::move(title), 0, 0, 0, 0) {}

    file_dialog::~file_dialog() {
        destroy();
    }

    const std::filesystem::path &file_dialog::get_initial_path() const {
        return _initial_path;
    }

    file_dialog &file_dialog::set_initial_path(
        const std::filesystem::path &path) {
        _initial_path = path;
        return *this;
    }

    const std::vector<file_filter> &file_dialog::get_filters() const {
        return _filters;
    }

    file_dialog &file_dialog::set_filters(
        const std::vector<file_filter> &filters) {
        _filters = filters;
        return *this;
    }

    file_dialog &file_dialog::add_filter(const file_filter &filter) {
        _filters.push_back(filter);
        return *this;
    }

    file_dialog &file_dialog::clear_filters() {
        _filters.clear();
        return *this;
    }

    const std::filesystem::path &file_dialog::get_path() const {
        return _paths.empty() ? _empty_path : _paths.front();
    }

    const std::vector<std::filesystem::path> &
    file_dialog::get_paths() const {
        return _paths;
    }

    void file_dialog::on_native_accept(
        const std::vector<std::filesystem::path> &paths) {
        if (!get_modal_active())
            return;

        _paths.clear();
        std::copy_if(paths.begin(),
                     paths.end(),
                     std::back_inserter(_paths),
                     [](const std::filesystem::path &path) {
                         return !path.empty();
                     });

        if (_paths.empty()) {
            close(dialog_result::cancelled);
            return;
        }

        close(dialog_result::accepted);
    }

    void file_dialog::on_native_cancel() {
        if (get_modal_active())
            close(dialog_result::cancelled);
    }

    void file_dialog::create_native() {
        if (!get_owner() || !get_owner()->get_created())
            throw std::logic_error(
                "A file dialog requires a created owner.");

    }

    void file_dialog::destroy_native() {
        if (!_created && !get_modal_active())
            return;

        cancel_native_dialog();
    }

    bool file_dialog::begin_dialog() {
        if (!_created)
            throw std::logic_error(
                "A file dialog must be created before show().");
        if (!get_owner() || !get_owner()->get_created())
            throw std::logic_error(
                "A file dialog requires a created owner.");
        if (get_modal_active())
            return false;

        _paths.clear();
        begin_modal_session();
        return true;
    }

    void file_dialog::apply_position() {}

    void file_dialog::apply_dimensions() {}

    void file_dialog::apply_bounds() {}

    void file_dialog::apply_parent() {}

    void file_dialog::apply_title() {}
} // namespace native

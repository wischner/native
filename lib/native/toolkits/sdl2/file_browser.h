//
// Declares the private SDL2 file-browser shell and its filesystem-backed
// navigation state. No browser implementation is exposed by the public API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <SDL2/SDL.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <native/file_dialog.h>
#include <native/modal_wnd.h>
#include <native/table_store.h>
#include <native/table_view.h>
#include <native/text_edit.h>

#include "file_browser_widgets.h"

namespace linux::sdl2
{
    // Stores one filesystem object presented in the main file table.
    struct file_browser_entry
    {
        std::string label;
        std::filesystem::path path;
        bool directory = false;
        std::shared_ptr<const native::img> image;
        std::string type;
        std::string size;
    };

    // Stores one detected system location presented in the Places table.
    struct file_browser_place
    {
        std::string label;
        std::filesystem::path path;
        std::shared_ptr<const native::img> image;
    };

    // Owns one synchronous SDL2 open, save, or folder-selection session.
    class file_browser final
    {
    public:
        // Construct the mode-specific browser state without creating peers.
        file_browser(native::file_dialog &source,
                     bool save,
                     bool directory,
                     std::string suggested_name,
                     std::string default_extension,
                     bool confirm_overwrite);

        // Run the modal browser and return its accepted path, if any.
        bool run(std::vector<std::filesystem::path> &paths);

    private:
        native::file_dialog &_source;
        bool _save;
        bool _directory_mode;
        std::string _default_extension;
        bool _confirm_overwrite;
        native::modal_wnd _window;
        file_browser_icon_button _back;
        file_browser_icon_button _forward;
        file_browser_icon_button _up;
        file_browser_breadcrumb _breadcrumbs;
        native::text_edit _location;
        native::table_store _place_store;
        native::table_store _entry_store;
        native::table_view _places;
        native::table_view _entries;
        native::text_edit _name;
        native::button _accept;
        native::button _cancel;
        std::filesystem::path _current;
        std::vector<file_browser_place> _place_entries;
        std::vector<file_browser_entry> _browser_entries;
        std::vector<std::filesystem::path> _history;
        std::size_t _history_index = 0;
        std::vector<std::filesystem::path> _paths;
        std::map<std::string, std::shared_ptr<const native::img>>
            _icon_cache;
        std::string _status;
        std::filesystem::path _pending_overwrite;
        bool _show_hidden = false;
        bool _editing_location = false;

        std::vector<native::wnd *> controls();
        void build_places();
        void choose_initial_directory();
        void connect_events();
        bool layout(native::size dimensions);
        bool valid_place(native::table_row_id id) const;
        bool valid_entry(native::table_row_id id) const;
        std::shared_ptr<const native::img> icon_for(
            const file_browser_entry &entry);
        void populate();
        void show_directory(const std::filesystem::path &path);
        void navigate_to(const std::filesystem::path &path,
                         bool remember);
        bool move_history(int direction);
        void update_place_selection();
        void selection_changed(native::table_row_id id);
        bool activate_entry(native::table_row_id id);
        std::filesystem::path entered_location() const;
        std::filesystem::path entered_file() const;
        bool activate_location();
        std::filesystem::path selected_path() const;
        bool accept();
        bool accept_path(std::filesystem::path path);
        bool cancel();
        void show_location_editor(int pointer_x = -1);
        void hide_location_editor();
        bool editor_focused(native::text_edit &editor) const;
        void run_loop();
        void dispatch_pointer(int x,
                              int y,
                              bool pressed,
                              bool released,
                              int clicks);
        void dispatch(const SDL_Event &event);
    };
} // namespace linux::sdl2

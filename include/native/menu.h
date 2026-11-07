//
// Declares the menu model and stream-style menu construction helpers.
// Backends attach the model to a native menu while preserving item IDs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace native
{
    class app_wnd;

    // Inserts a non-command dividing line in a menu item group.
    struct menu_separator_t {};
    inline constexpr menu_separator_t menu_separator{};

    // Declares a command that toggles before on_menu is emitted.
    // Zero selects an automatic ID; checked is the initial state.
    struct menu_check_item
    {
        int id = 0;
        std::string label;
        bool checked = false;
    };

    // Stores one parsed command or structural separator.
    struct menu_entry
    {
        int id = 0;
        std::string label;
        std::size_t mnemonic_index = std::string::npos;
        std::string shortcut;
        bool separator = false;
        bool checkable = false;
        bool checked = false;
        bool enabled = true;
    };

    // Collects menu items for one top-level menu in the builder API.
    class menu_items_proxy
    {
    public:
        // Compatibility name for the shared parsed menu entry.
        using entry = menu_entry;

        // Items accumulated by the stream-style builder.
        std::vector<entry> entries;

        // Append a label with an automatically assigned command ID.
        menu_items_proxy &operator<<(const std::string &label);

        // Append a caller-supplied command ID and label.
        menu_items_proxy &operator<<(std::pair<int, std::string> item);

        // Append a checkable command with its initial state.
        menu_items_proxy &operator<<(menu_check_item item);

        // Append a native menu separator.
        menu_items_proxy &operator<<(menu_separator_t);
    };

    //
    // Begin a list of menu items with an automatically assigned ID.
    //
    // Returns:
    //      A proxy that accepts more items through operator<<().
    //
    menu_items_proxy menu_items(const std::string &first);

    // Begin an item list with a checkable command.
    menu_items_proxy menu_items(menu_check_item first);

    // Owns the backend-neutral description of an application menu bar.
    class main_menu
    {
    public:
        // Construct an empty menu model.
        main_menu();

        // Release any native menu attached by the active backend.
        ~main_menu();

        // Menu models own backend registrations and cannot be copied.
        main_menu(const main_menu &) = delete;

        // Menu models cannot be copy-assigned.
        main_menu &operator=(const main_menu &) = delete;

        // Attach this model after its owner window has been created.
        void attach(app_wnd &owner);

        // Silently update a checkable command before or after attachment.
        // Unknown IDs, separators and ordinary commands are unchanged.
        main_menu &set_checked(int command, bool checked);

        // Return a checkable command's state; otherwise return false.
        bool get_checked(int command) const;

        // Silently enable or disable a command before or after attachment.
        // A disabled command appears dimmed and ignores pointer, keyboard and
        // shortcut activation. Unknown IDs and separators are unchanged.
        main_menu &set_enabled(int command, bool enabled);

        // Return false only for a known command that is disabled.
        bool get_enabled(int command) const;

        // Completes alternating title and item-list insertion.
        class builder
        {
        public:
            // Construct a builder borrowing a menu model.
            explicit builder(main_menu &menu);

            // Append another top-level menu title.
            builder &operator<<(const std::string &top_title);

            // Append items to the most recently added top-level menu.
            builder &operator<<(const menu_items_proxy &proxy);

        private:
            main_menu &_menu;
        };

        // Begin stream-style construction with a top-level title.
        builder operator<<(const std::string &top_title);

        // Return the registry identifier, or zero if unattached.
        std::uint32_t id() const;

        // Compatibility name used by existing backend code.
        using menu_entry = native::menu_entry;

        // Stores one top-level title and its commands.
        struct top_entry
        {
            std::string title;
            std::size_t mnemonic_index = std::string::npos;
            std::vector<menu_entry> items;
        };

        // Return the menu model for backend implementation code.
        const std::vector<top_entry> &tops() const;

    private:
        // Release the backend resource but preserve the portable model.
        void detach();

        // Apply one cached check state to an attached backend resource.
        void apply_checked(int command);

        // Apply one cached enabled state to an attached backend resource.
        void apply_enabled(int command);

        // Toggle a checkable command before application notification.
        void activate(int command);

        // Add a new top-level menu to the model.
        void add_top(const std::string &title);

        // Find an unused command ID in the automatic-ID range.
        int next_auto_item_id() const;

        // Add a command to the most recent top-level menu.
        void add_item(int id,
                      const std::string &label,
                      std::size_t mnemonic_index,
                      const std::string &shortcut,
                      bool checkable,
                      bool checked);

        // Add a non-command separator to the most recent top-level menu.
        void add_separator();

        std::vector<top_entry> _tops;
        app_wnd *_owner = nullptr;
        std::uint32_t _id = 0;

        friend class builder;
        friend class wnd;
        friend class app_wnd;
    };
} // namespace native

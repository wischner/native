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

    // Collects menu items for one top-level menu in the builder API.
    class menu_items_proxy
    {
    public:
        // Stores the command identifier and visible label for an item.
        struct entry
        {
            int id = 0;
            std::string label;
            std::size_t mnemonic_index = std::string::npos;
            std::string shortcut;
            bool separator = false;
        };

        // Items accumulated by the stream-style builder.
        std::vector<entry> entries;

        // Append a label with an automatically assigned command ID.
        menu_items_proxy &operator<<(const std::string &label);

        // Append a caller-supplied command ID and label.
        menu_items_proxy &operator<<(std::pair<int, std::string> item);

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

        // Stores one command within a top-level menu.
        struct menu_entry
        {
            int id = 0;
            std::string label;
            std::size_t mnemonic_index = std::string::npos;
            std::string shortcut;
            bool separator = false;
        };

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

        // Add a new top-level menu to the model.
        void add_top(const std::string &title);

        // Find an unused command ID in the automatic-ID range.
        int next_auto_item_id() const;

        // Add a command to the most recent top-level menu.
        void add_item(int id,
                      const std::string &label,
                      std::size_t mnemonic_index,
                      const std::string &shortcut);

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

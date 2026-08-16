//
// Implements backend-neutral menu model construction and command IDs.
// Platform files attach the completed model to their native menu APIs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <utility>

#include <native/menu.h>

namespace native
{
    main_menu::main_menu() = default;

    menu_items_proxy &
    menu_items_proxy::operator<<(const std::string &label) {
        entries.push_back({0, label});
        return *this;
    }

    menu_items_proxy &
    menu_items_proxy::operator<<(std::pair<int, std::string> item) {
        entries.push_back({item.first, std::move(item.second)});
        return *this;
    }

    menu_items_proxy menu_items(const std::string &first) {
        menu_items_proxy proxy;
        proxy << first;
        return proxy;
    }

    main_menu::builder::builder(main_menu &menu)
        : _menu(menu) {}

    main_menu::builder &
    main_menu::builder::operator<<(const std::string &top_title) {
        _menu.add_top(top_title);
        return *this;
    }

    main_menu::builder &
    main_menu::builder::operator<<(const menu_items_proxy &proxy) {
        for (const auto &entry : proxy.entries)
            _menu.add_item(entry.id, entry.label);
        return *this;
    }

    main_menu::builder
    main_menu::operator<<(const std::string &top_title) {
        add_top(top_title);
        return builder(*this);
    }

    std::uint32_t main_menu::id() const {
        return _id;
    }

    const std::vector<main_menu::top_entry> &main_menu::tops() const {
        return _tops;
    }

    void main_menu::add_top(const std::string &title) {
        _tops.push_back({title, {}});
    }

    int main_menu::next_auto_item_id() const {
        // Keep automatic IDs above common hand-authored command IDs.
        int candidate = 10000;
        while (true) {
            bool used = false;
            for (const auto &top : _tops) {
                for (const auto &item : top.items) {
                    if (item.id == candidate) {
                        used = true;
                        break;
                    }
                }
                if (used)
                    break;
            }
            if (!used)
                return candidate;
            ++candidate;
        }
    }

    void main_menu::add_item(int id, const std::string &label) {
        if (id == 0)
            id = next_auto_item_id();
        if (!_tops.empty())
            _tops.back().items.push_back({id, label});
    }
} // namespace native

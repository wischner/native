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
    namespace
    {
        struct parsed_menu_text
        {
            std::string text;
            std::size_t mnemonic_index = std::string::npos;
            std::string shortcut;
        };

        parsed_menu_text parse_menu_text(const std::string &value) {
            parsed_menu_text result;
            const std::size_t tab = value.find('\t');
            const std::string visible = value.substr(0, tab);
            if (tab != std::string::npos)
                result.shortcut = value.substr(tab + 1);
            result.text.reserve(visible.size());
            for (std::size_t index = 0; index < visible.size(); ++index) {
                if (visible[index] == '&' && index + 1 < visible.size()) {
                    if (visible[index + 1] == '&') {
                        result.text.push_back('&');
                        ++index;
                    } else {
                        if (result.mnemonic_index == std::string::npos)
                            result.mnemonic_index = result.text.size();
                    }
                    continue;
                }
                result.text.push_back(visible[index]);
            }
            if (result.mnemonic_index == std::string::npos &&
                !result.text.empty())
                result.mnemonic_index = 0;
            return result;
        }
    } // namespace

    main_menu::main_menu() = default;

    menu_items_proxy &
    menu_items_proxy::operator<<(const std::string &label) {
        const parsed_menu_text parsed = parse_menu_text(label);
        entries.push_back({0,
                           parsed.text,
                           parsed.mnemonic_index,
                           parsed.shortcut,
                           false});
        return *this;
    }

    menu_items_proxy &
    menu_items_proxy::operator<<(std::pair<int, std::string> item) {
        const parsed_menu_text parsed = parse_menu_text(item.second);
        entries.push_back({item.first,
                           parsed.text,
                           parsed.mnemonic_index,
                           parsed.shortcut,
                           false});
        return *this;
    }

    menu_items_proxy &menu_items_proxy::operator<<(menu_separator_t) {
        entries.push_back({0, {}, std::string::npos, {}, true});
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
        for (const auto &entry : proxy.entries) {
            if (entry.separator)
                _menu.add_separator();
            else
                _menu.add_item(entry.id,
                               entry.label,
                               entry.mnemonic_index,
                               entry.shortcut);
        }
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
        const parsed_menu_text parsed = parse_menu_text(title);
        _tops.push_back({parsed.text, parsed.mnemonic_index, {}});
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

    void main_menu::add_item(int id,
                             const std::string &label,
                             std::size_t mnemonic_index,
                             const std::string &shortcut) {
        if (id == 0)
            id = next_auto_item_id();
        if (!_tops.empty())
            _tops.back().items.push_back(
                {id, label, mnemonic_index, shortcut, false});
    }

    void main_menu::add_separator() {
        if (!_tops.empty() && !_tops.back().items.empty() &&
            !_tops.back().items.back().separator)
            _tops.back().items.push_back(
                {0, {}, std::string::npos, {}, true});
    }
} // namespace native

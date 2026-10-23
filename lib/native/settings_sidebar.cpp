//
// Implements settings-page identity, borrowed lifecycle and navigation.
// Dedicated adapters supply Granite/AppKit navigation; other backends
// compose their existing tree or list controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/settings_sidebar.h>
#include <native/tree_view.h>
#include <native/list.h>
#include <native/text_edit.h>
#include "table_search.h"
#include "text_util.h"
#include <algorithm>
#include <stdexcept>
#include <optional>
#include "picker_peer.h"
#include "wnd_peer.h"
#include "input_geometry.h"

namespace native
{
    settings_page::settings_page(int value, std::string label, wnd &page,
        std::shared_ptr<const img> icon, std::string group, bool available)
        : id(value), title(std::move(label)), content(&page), image(std::move(icon)),
          section(std::move(group)), enabled(available) {}
    settings_sidebar::settings_sidebar(coord x, coord y, dim width, dim height)
        : panel(x, y, width, height) {}
    settings_sidebar::settings_sidebar(const point &position, const size &dimensions)
        : panel(position, dimensions) {}
    settings_sidebar::settings_sidebar(const rect &bounds) : panel(bounds) {}
    settings_sidebar::~settings_sidebar() { destroy(); clear_items(); }
    const std::vector<settings_page> &settings_sidebar::get_items() const { return _items; }
    int settings_sidebar::get_selected_item() const { return _selected; }
    int settings_sidebar::get_sidebar_width() const { return _sidebar_width; }
    settings_page &settings_sidebar::find(int id) {
        auto item = std::find_if(_items.begin(), _items.end(),
            [id](const auto &page) { return page.id == id; });
        if (item == _items.end()) throw std::out_of_range("Unknown settings page.");
        return *item;
    }
    const settings_page &settings_sidebar::get_item(int id) const {
        return const_cast<settings_sidebar *>(this)->find(id);
    }
    settings_sidebar &settings_sidebar::add_item(settings_page page) {
        if (page.id <= 0 || page.title.empty() || !page.content || page.content->get_created())
            throw std::invalid_argument("Invalid settings page or already-created content.");
        for (const auto &item : _items)
            if (item.id == page.id || item.content == page.content)
                throw std::invalid_argument("Duplicate settings page ID or content.");
        auto items = _items;
        items.push_back(page);
        page.content->set_parent(this);
        _items.swap(items);
        choose_enabled(); _items_dirty = true; refresh();
        return *this;
    }
    settings_sidebar &settings_sidebar::operator<<(settings_page page) { return add_item(std::move(page)); }
    settings_sidebar &settings_sidebar::remove_item(int id) {
        auto *content = find(id).content;
        content->destroy(); content->set_parent(nullptr);
        std::erase_if(_items, [id](const auto &page) { return page.id == id; });
        if (_selected == id) _selected = 0;
        choose_enabled(); _items_dirty = true; refresh(); return *this;
    }
    settings_sidebar &settings_sidebar::clear_items() {
        const auto items = _items;
        _items.clear(); _selected = 0; _items_dirty = true;
        for (const auto &item : items) {
            item.content->destroy(); item.content->set_parent(nullptr);
        }
        refresh(); return *this;
    }
    void settings_sidebar::choose_enabled() {
        for (const auto &item : _items)
            if (item.id == _selected && item.enabled) return;
        _selected = 0;
        for (const auto &item : _items)
            if (item.enabled) { _selected = item.id; return; }
    }
    settings_sidebar &settings_sidebar::set_item_enabled(int id, bool enabled) {
        find(id).enabled = enabled; choose_enabled();
        _items_dirty = true; refresh(); return *this;
    }
    settings_sidebar &settings_sidebar::set_item_text(int id, std::string title) {
        if (title.empty()) throw std::invalid_argument("Empty settings page title.");
        find(id).title = std::move(title); _items_dirty = true; refresh(); return *this;
    }
    settings_sidebar &settings_sidebar::set_item_image(int id, std::shared_ptr<const img> image) {
        find(id).image = std::move(image); _items_dirty = true; refresh(); return *this;
    }
    settings_sidebar &settings_sidebar::set_selected_item(int id) {
        if (id && !find(id).enabled) throw std::invalid_argument("Disabled settings page.");
        _selected = id; refresh(); return *this;
    }
    settings_sidebar &settings_sidebar::set_sidebar_width(int width) {
        if (width < 1 || width > 32767) throw std::invalid_argument("Invalid sidebar width.");
        _sidebar_width = width; refresh(); return *this;
    }
    bool settings_sidebar::get_search_visible() const { return _search_visible; }
    const std::string &settings_sidebar::get_search_text() const { return _search_text; }
    settings_sidebar &settings_sidebar::set_search_visible(bool visible) {
        _search_visible = visible; _items_dirty = true; refresh(); return *this;
    }
    settings_sidebar &settings_sidebar::set_search_text(std::string text) {
        if (!detail::valid_utf8(text) || text.find_first_of("\r\n") != std::string::npos ||
            text.find('\0') != std::string::npos)
            throw std::invalid_argument("Settings search requires single-line UTF-8 text.");
        _search_text = std::move(text); _items_dirty = true; refresh(); return *this;
    }
    void settings_sidebar::on_native_search(std::string text) {
        if (!get_input_enabled() || text == _search_text) return;
        const detail::window_guard guard(*this);
        set_search_text(std::move(text));
        if (guard.alive()) on_search_change.emit(_search_text);
    }
    rect settings_sidebar::get_content_bounds() const {
        const auto area = get_client_bounds();
        const int width = std::min(_sidebar_width, int(area.w()));
        return rect(area.x1() + width, area.y1(), int(area.w()) - width, area.h());
    }
    void settings_sidebar::on_native_selection(int id) {
        if (!id || id == _selected || !get_input_enabled()) return;
        const auto found = std::find_if(_items.begin(), _items.end(),
            [id](const auto &item) { return item.id == id && item.enabled; });
        if (found == _items.end()) return;
        const detail::window_guard guard(*this);
        _selected = id; refresh();
        if (guard.alive()) on_selection_change.emit(id);
    }
    void settings_sidebar::create_native() {
        _refreshing = false;
        panel::create_native();
        if (!detail::create_settings_sidebar_peer(*this)) {
            if (detail::settings_sidebar_uses_list()) {
                _list_navigation = std::make_unique<list>();
                _list_navigation->set_parent(this);
                _list_navigation->on_selection_change.connect([this](int index) {
                    const detail::window_guard guard(*this);
                    if (index >= 0 && std::size_t(index) < _navigation_ids.size())
                        on_native_selection(_navigation_ids[index]);
                    if (guard.alive()) refresh();
                    return true;
                });
                _list_navigation->create(); _list_navigation->show();
            } else {
                _navigation = std::make_unique<tree_view>();
                _navigation->set_lines_visible(false).set_border_visible(
                    detail::settings_sidebar_navigation_padding() > 0);
                _navigation->set_parent(this);
                _navigation->on_selection_change.connect([this](tree_item_id id) {
                    on_native_selection(static_cast<int>(id)); return true;
                });
                _navigation->create(); _navigation->show();
            }
        }
        _items_dirty = true; refresh();
    }
    void settings_sidebar::destroy_native() {
        destroy_children();
        if (auto *resources = detail::peer_state<detail::settings_sidebar_resources>(*this))
            resources->close();
        _search.reset(); _navigation.reset(); _list_navigation.reset(); panel::destroy_native();
    }
    void settings_sidebar::on_bounds_changed() { refresh(); }
    void settings_sidebar::refresh() {
        if (!get_created() || _refreshing) return;
        const detail::window_guard guard(*this);
        _refreshing = true;
        try {
            const auto area = detail::settings_navigation_bounds(*this);
            if (!detail::settings_sidebar_uses_native_search()) {
                if (_search_visible) {
                    if (!_search) {
                        _search = std::make_unique<text_edit>();
                        _search->set_parent(this);
                        _search->on_change.connect([this](const std::string &text) {
                            on_native_search(text); return true;
                        });
                    }
                    _search->set_text(_search_text);
                    const int padding =
                        detail::settings_sidebar_navigation_padding();
                    _search->set_bounds(
                        padding > 0
                            ? detail::input_bounds(
                                  *this,
                                  rect(area.x1(),
                                       get_client_bounds().y1() +
                                           padding,
                                       area.w(),
                                       1))
                            : rect(area.x1(), 6, area.w(), 28));
                    if (!_search->get_created()) _search->create();
                    _search->show();
                } else if (_search) _search->destroy();
            }
            if (_list_navigation) {
                if (_items_dirty) {
                    std::vector<std::string> labels;
                    _navigation_ids.clear();
                    std::string section;
                    for (const auto &item : detail::settings_visible_items(*this)) {
                        if (!item.section.empty() && item.section != section) {
                            labels.push_back(item.section); _navigation_ids.push_back(0);
                        }
                        section = item.section;
                        labels.push_back(item.enabled ? item.title : item.title + " (unavailable)");
                        _navigation_ids.push_back(item.id);
                    }
                    _list_navigation->set_items(std::move(labels)); _items_dirty = false;
                }
                _list_navigation->set_bounds(rect(area.x1(), area.y1(),
                    area.w(), area.h()));
                const auto item = std::find(_navigation_ids.begin(), _navigation_ids.end(), _selected);
                _list_navigation->set_selected_index(_selected && item != _navigation_ids.end()
                    ? int(item - _navigation_ids.begin()) : -1);
            } else if (_navigation) {
                if (_items_dirty) {
                    std::vector<tree_view_item> rows;
                    std::string section;
                    std::optional<std::size_t> group;
                    tree_item_id header_id = tree_item_id(1) << 63;
                    for (const auto &item : detail::settings_visible_items(*this)) {
                        if (!item.section.empty() && item.section != section) {
                            rows.emplace_back(item.section, nullptr, header_id++,
                                std::vector<tree_view_item>{}, true, false);
                            group = rows.size() - 1;
                        }
                        if (item.section.empty()) group.reset();
                        section = item.section;
                        auto &destination = group ? rows[*group].children : rows;
                        destination.emplace_back(item.title, item.image, item.id,
                            std::vector<tree_view_item>{}, false, item.enabled);
                    }
                    _navigation->set_items(std::move(rows));
                    _items_dirty = false;
                }
                _navigation->set_bounds(rect(area.x1(), area.y1(),
                    area.w(), area.h()));
                _navigation->set_selected_item(_navigation->contains_item(_selected) ? _selected : 0);
            }
            detail::refresh_settings_sidebar_peer(*this);
            const auto items = _items;
            for (const auto &item : items) {
                if (item.id == _selected && item.enabled) {
                    item.content->set_bounds(get_content_bounds());
                    if (!item.content->get_created()) item.content->create();
                    if (!guard.alive()) return;
                    item.content->show();
                } else item.content->destroy();
                if (!guard.alive()) return;
            }
        } catch (...) {
            if (guard.object_alive()) _refreshing = false;
            throw;
        }
        _refreshing = false;
    }
}

namespace native::detail
{
    std::vector<settings_page> settings_visible_items(const settings_sidebar &owner) {
        std::vector<settings_page> result;
        for (const auto &item : owner.get_items()) {
            const auto &query = owner.get_search_text();
            if (!owner.get_search_visible() || query.empty() ||
                table_text_matches(item.title, query, table_search_match::substring, table_search_case::insensitive) ||
                table_text_matches(item.section, query, table_search_match::substring, table_search_case::insensitive))
                result.push_back(item);
        }
        return result;
    }
    rect settings_navigation_bounds(const settings_sidebar &owner) {
        const auto area = owner.get_client_bounds();
        const int search_height =
            settings_sidebar_navigation_padding() == 0
                ? 40
                : int(input_bounds(owner, rect(0, 0, 1, 1)).h()) +
                      settings_sidebar_navigation_padding();
        const int inset = owner.get_search_visible()
                              ? std::min(search_height, int(area.h()))
                              : 0;
        const int width = std::min(owner.get_sidebar_width(), int(area.w()));
        const int padding = std::min({settings_sidebar_navigation_padding(),
            width / 2, (int(area.h()) - inset) / 2});
        return rect(area.x1() + padding, area.y1() + inset + padding,
            width - 2 * padding, int(area.h()) - inset - 2 * padding);
    }
}

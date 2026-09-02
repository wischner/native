//
// Implements the portable docking tree, geometry, persistence, host
// interaction, and modeless floating-pane lifecycle.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/dock.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <native/app_wnd.h>
#include <native/accordion.h>
#include <native/font.h>
#include <native/graphics.h>
#include <native/modeless_wnd.h>
#include <native/theme.h>
#include <native/wnd.h>

namespace
{
    constexpr std::uint32_t dock_layout_version = 2;
    constexpr std::uint32_t minimum_dock_layout_version = 1;
    constexpr float minimum_split_ratio = 0.05f;
    constexpr float maximum_split_ratio = 0.95f;
    constexpr int ratio_scale = 1000000;

    native::coord to_coord(int value) {
        const int low =
            static_cast<int>(std::numeric_limits<native::coord>::min());
        const int high =
            static_cast<int>(std::numeric_limits<native::coord>::max());
        return static_cast<native::coord>(
            std::max(low, std::min(high, value)));
    }

    native::dim to_dim(int value) {
        if (value <= 0)
            return 0;
        const int high =
            static_cast<int>(std::numeric_limits<native::dim>::max());
        return static_cast<native::dim>(std::min(high, value));
    }

    native::rect make_rect(int x, int y, int width, int height) {
        return native::rect(to_coord(x),
                            to_coord(y),
                            to_dim(width),
                            to_dim(height));
    }

    int rect_right(const native::rect &bounds) {
        return static_cast<int>(bounds.p.x) +
               static_cast<int>(bounds.d.w);
    }

    int rect_bottom(const native::rect &bounds) {
        return static_cast<int>(bounds.p.y) +
               static_cast<int>(bounds.d.h);
    }

    int caption_button_extent(int header_height) {
        return std::max(8, std::min(14, header_height - 4));
    }

    void draw_rotated_text(native::gpx &graphics,
                           const std::string &text,
                           const native::rect &bounds,
                           bool clockwise,
                           int padding) {
        const int source_width = std::max(
            1, static_cast<int>(bounds.d.h) - padding * 2);
        const int source_height = std::max(
            1, static_cast<int>(bounds.d.w) - 4);
        native::img source(to_dim(source_width),
                           to_dim(source_height));
        native::gpx &source_graphics = source.get_gpx();
        const native::rgba ink = graphics.get_ink();
        // Alpha-zero complementary paper stays transparent while letting
        // native image-font adapters detect black and white glyph pixels.
        const native::rgba transparent_paper(
            static_cast<std::uint8_t>(255U - ink.r),
            static_cast<std::uint8_t>(255U - ink.g),
            static_cast<std::uint8_t>(255U - ink.b),
            0);
        source_graphics
            .clear(transparent_paper)
            .set_font(graphics.get_font())
            .set_ink(ink)
            .draw_text(
                text,
                native::rect(0, 0,
                             to_dim(source_width),
                             to_dim(source_height)),
                native::text_layout{
                    native::text_align::center,
                    native::text_valign::center,
                    native::text_overflow::ellipsis,
                    true});

        native::img rotated(to_dim(source_height),
                            to_dim(source_width));
        native::rgba *destination = rotated.pixels();
        const native::rgba *origin = source.pixels();
        for (int y = 0; y < source_height; ++y) {
            for (int x = 0; x < source_width; ++x) {
                const int target_x = clockwise
                                         ? source_height - 1 - y
                                         : y;
                const int target_y = clockwise
                                         ? x
                                         : source_width - 1 - x;
                destination[target_y * source_height + target_x] =
                    origin[y * source_width + x];
            }
        }
        graphics.draw_img(
            rotated,
            native::point(
                to_coord(bounds.p.x +
                         (static_cast<int>(bounds.d.w) -
                          static_cast<int>(rotated.w())) /
                             2),
                to_coord(bounds.p.y +
                         (static_cast<int>(bounds.d.h) -
                          static_cast<int>(rotated.h())) /
                             2)));
    }

    bool contains_pane(const native::dock_layout_node &node,
                       native::dock_pane_id pane) {
        if (node.kind == native::dock_node_kind::tabs) {
            return std::find(node.panes.begin(),
                             node.panes.end(),
                             pane) != node.panes.end();
        }
        for (const auto &child : node.children) {
            if (contains_pane(child, pane))
                return true;
        }
        return false;
    }

    native::dock_layout_node *find_pane_node(
        native::dock_layout_node &node,
        native::dock_pane_id pane) {
        if (node.kind == native::dock_node_kind::tabs) {
            return contains_pane(node, pane) ? &node : nullptr;
        }
        for (auto &child : node.children) {
            if (native::dock_layout_node *found =
                    find_pane_node(child, pane)) {
                return found;
            }
        }
        return nullptr;
    }

    const native::dock_layout_node *find_pane_node(
        const native::dock_layout_node &node,
        native::dock_pane_id pane) {
        if (node.kind == native::dock_node_kind::tabs) {
            return contains_pane(node, pane) ? &node : nullptr;
        }
        for (const auto &child : node.children) {
            if (const native::dock_layout_node *found =
                    find_pane_node(child, pane)) {
                return found;
            }
        }
        return nullptr;
    }

    native::dock_layout_node *find_node(native::dock_layout_node &node,
                                        native::dock_node_id id) {
        if (node.id == id)
            return &node;
        for (auto &child : node.children) {
            if (native::dock_layout_node *found =
                    find_node(child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    const native::dock_layout_node *find_node(
        const native::dock_layout_node &node,
        native::dock_node_id id) {
        if (node.id == id)
            return &node;
        for (const auto &child : node.children) {
            if (const native::dock_layout_node *found =
                    find_node(child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    bool erase_pane(native::dock_layout_node &node,
                    native::dock_pane_id pane) {
        if (node.kind == native::dock_node_kind::tabs) {
            const auto old_size = node.panes.size();
            node.panes.erase(
                std::remove(node.panes.begin(),
                            node.panes.end(),
                            pane),
                node.panes.end());
            if (node.active_pane == pane) {
                node.active_pane = node.panes.empty()
                                       ? 0
                                       : node.panes.front();
            }
            return old_size != node.panes.size();
        }

        bool removed = false;
        for (auto child = node.children.begin();
             child != node.children.end();) {
            if (erase_pane(*child, pane))
                removed = true;

            const bool empty_tabs =
                child->kind == native::dock_node_kind::tabs &&
                child->panes.empty();
            const bool empty_split =
                child->kind == native::dock_node_kind::split &&
                child->children.empty();
            if (empty_tabs || empty_split)
                child = node.children.erase(child);
            else
                ++child;
        }

        if (node.children.size() == 1) {
            native::dock_layout_node surviving =
                std::move(node.children.front());
            node = std::move(surviving);
        }
        return removed;
    }

    native::dock_layout_node *first_tab_node(
        native::dock_layout_node &node) {
        if (node.kind == native::dock_node_kind::tabs)
            return &node;
        for (auto &child : node.children) {
            if (native::dock_layout_node *found =
                    first_tab_node(child)) {
                return found;
            }
        }
        return nullptr;
    }

    void collect_node_ids(const native::dock_layout_node &node,
                          native::dock_node_id &maximum) {
        maximum = std::max(maximum, node.id);
        for (const auto &child : node.children)
            collect_node_ids(child, maximum);
    }

    void serialize_node(std::ostringstream &stream,
                        const native::dock_layout_node &node) {
        if (node.kind == native::dock_node_kind::tabs) {
            stream << "T(" << node.id << ',' << node.active_pane << ','
                   << node.panes.size();
            for (native::dock_pane_id pane : node.panes)
                stream << ',' << pane;
            stream << ')';
            return;
        }

        if (node.children.size() != 2)
            throw std::invalid_argument(
                "A persisted dock split requires two children.");
        const int ratio = static_cast<int>(std::lround(
            std::clamp(node.split_ratio,
                       minimum_split_ratio,
                       maximum_split_ratio) *
            ratio_scale));
        stream << "S(" << node.id << ','
               << (node.orientation ==
                           native::dock_orientation::horizontal
                       ? 'H'
                       : 'V')
               << ',' << ratio << ',';
        serialize_node(stream, node.children[0]);
        stream << ',';
        serialize_node(stream, node.children[1]);
        stream << ')';
    }

    char serialize_edge(native::dock_position edge) {
        switch (edge) {
        case native::dock_position::left:
            return 'L';
        case native::dock_position::right:
            return 'R';
        case native::dock_position::top:
            return 'T';
        case native::dock_position::bottom:
            return 'B';
        case native::dock_position::center:
            break;
        }
        throw std::invalid_argument(
            "An auto-hide pane requires a host edge.");
    }

    class dock_parser final
    {
    public:
        explicit dock_parser(std::string_view text)
            : _text(text) {}

        native::dock_layout_state parse() {
            literal("NDOCK");
            const std::uint64_t version = unsigned_number();
            if (version < minimum_dock_layout_version ||
                version > dock_layout_version) {
                fail("unsupported layout version");
            }
            literal(";R");
            native::dock_layout_state state;
            state.version = static_cast<std::uint32_t>(version);
            skip_space();
            if (peek() == 'E') {
                ++_position;
            } else {
                state.root = node();
            }

            literal(";F");
            const std::size_t float_count = count();
            state.floating.reserve(float_count);
            for (std::size_t index = 0; index < float_count; ++index) {
                character(',');
                const std::uint64_t pane = unsigned_number();
                character(',');
                const std::int64_t x = signed_number();
                character(',');
                const std::int64_t y = signed_number();
                character(',');
                const std::uint64_t width = unsigned_number();
                character(',');
                const std::uint64_t height = unsigned_number();
                if (pane == 0 ||
                    x < std::numeric_limits<native::coord>::min() ||
                    x > std::numeric_limits<native::coord>::max() ||
                    y < std::numeric_limits<native::coord>::min() ||
                    y > std::numeric_limits<native::coord>::max() ||
                    width == 0 || height == 0 ||
                    width > std::numeric_limits<native::dim>::max() ||
                    height > std::numeric_limits<native::dim>::max()) {
                    fail("invalid floating bounds");
                }
                state.floating.push_back(
                    {pane,
                     native::rect(static_cast<native::coord>(x),
                                  static_cast<native::coord>(y),
                                  static_cast<native::dim>(width),
                                  static_cast<native::dim>(height))});
            }

            literal(";H");
            const std::size_t hidden_count = count();
            state.hidden.reserve(hidden_count);
            for (std::size_t index = 0; index < hidden_count; ++index) {
                character(',');
                const std::uint64_t pane = unsigned_number();
                if (pane == 0)
                    fail("zero hidden pane ID");
                state.hidden.push_back(pane);
            }

            if (version >= 2) {
                literal(";A");
                const std::size_t auto_hide_count = count();
                state.auto_hidden.reserve(auto_hide_count);
                for (std::size_t index = 0;
                     index < auto_hide_count;
                     ++index) {
                    character(',');
                    const std::uint64_t pane = unsigned_number();
                    character(',');
                    const char edge = peek();
                    ++_position;
                    if (pane == 0)
                        fail("zero auto-hide pane ID");
                    native::dock_position position;
                    switch (edge) {
                    case 'L':
                        position = native::dock_position::left;
                        break;
                    case 'R':
                        position = native::dock_position::right;
                        break;
                    case 'T':
                        position = native::dock_position::top;
                        break;
                    case 'B':
                        position = native::dock_position::bottom;
                        break;
                    default:
                        fail("invalid auto-hide edge");
                    }
                    state.auto_hidden.push_back({pane, position});
                }
            }

            skip_space();
            if (_position != _text.size())
                fail("trailing characters");
            return state;
        }

    private:
        std::string_view _text;
        std::size_t _position = 0;

        [[noreturn]] void fail(const char *reason) const {
            throw std::invalid_argument(
                std::string("Invalid Native Dock layout at byte ") +
                std::to_string(_position) + ": " + reason + '.');
        }

        void skip_space() {
            while (_position < _text.size() &&
                   (_text[_position] == ' ' ||
                    _text[_position] == '\t' ||
                    _text[_position] == '\n' ||
                    _text[_position] == '\r')) {
                ++_position;
            }
        }

        char peek() {
            skip_space();
            if (_position >= _text.size())
                fail("unexpected end");
            return _text[_position];
        }

        void character(char expected) {
            if (peek() != expected)
                fail("unexpected character");
            ++_position;
        }

        void literal(std::string_view expected) {
            skip_space();
            if (_text.substr(_position, expected.size()) != expected)
                fail("unexpected token");
            _position += expected.size();
        }

        std::uint64_t unsigned_number() {
            skip_space();
            const char *begin = _text.data() + _position;
            const char *end = _text.data() + _text.size();
            std::uint64_t value = 0;
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc() || result.ptr == begin)
                fail("invalid unsigned integer");
            _position = static_cast<std::size_t>(
                result.ptr - _text.data());
            return value;
        }

        std::int64_t signed_number() {
            skip_space();
            const char *begin = _text.data() + _position;
            const char *end = _text.data() + _text.size();
            std::int64_t value = 0;
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc() || result.ptr == begin)
                fail("invalid signed integer");
            _position = static_cast<std::size_t>(
                result.ptr - _text.data());
            return value;
        }

        std::size_t count() {
            const std::uint64_t value = unsigned_number();
            if (value > 1000000U)
                fail("unreasonable item count");
            return static_cast<std::size_t>(value);
        }

        native::dock_layout_node node() {
            const char type = peek();
            ++_position;
            character('(');

            native::dock_layout_node result;
            result.id = unsigned_number();
            if (result.id == 0)
                fail("zero node ID");
            character(',');

            if (type == 'T') {
                result.kind = native::dock_node_kind::tabs;
                result.active_pane = unsigned_number();
                character(',');
                const std::size_t pane_count = count();
                result.panes.reserve(pane_count);
                for (std::size_t index = 0; index < pane_count;
                     ++index) {
                    character(',');
                    const std::uint64_t pane = unsigned_number();
                    if (pane == 0)
                        fail("zero pane ID");
                    result.panes.push_back(pane);
                }
                character(')');
                return result;
            }

            if (type != 'S')
                fail("unknown node kind");
            result.kind = native::dock_node_kind::split;
            const char orientation = peek();
            if (orientation != 'H' && orientation != 'V')
                fail("invalid split orientation");
            ++_position;
            result.orientation =
                orientation == 'H'
                    ? native::dock_orientation::horizontal
                    : native::dock_orientation::vertical;
            character(',');
            const std::uint64_t ratio = unsigned_number();
            if (ratio > static_cast<std::uint64_t>(ratio_scale))
                fail("invalid split ratio");
            result.split_ratio =
                static_cast<float>(ratio) / ratio_scale;
            character(',');
            result.children.push_back(node());
            character(',');
            result.children.push_back(node());
            character(')');
            return result;
        }
    };
} // namespace

namespace native
{
    dock_pane::dock_pane() = default;

    dock_pane::dock_pane(dock_pane_id pane_id,
                         std::string pane_title,
                         wnd &pane_content)
        : id(pane_id)
        , title(std::move(pane_title))
        , content(&pane_content) {}

    std::string serialize_dock_layout(
        const dock_layout_state &state) {
        if (state.version < minimum_dock_layout_version ||
            state.version > dock_layout_version)
            throw std::invalid_argument(
                "Unsupported Native Dock layout version.");
        if (state.version == 1 && !state.auto_hidden.empty())
            throw std::invalid_argument(
                "Native Dock v1 cannot store auto-hidden panes.");

        std::ostringstream stream;
        stream << "NDOCK" << state.version << ";R";
        if (state.root)
            serialize_node(stream, *state.root);
        else
            stream << 'E';
        stream << ";F" << state.floating.size();
        for (const auto &floating : state.floating) {
            stream << ',' << floating.pane << ','
                   << floating.bounds.p.x << ','
                   << floating.bounds.p.y << ','
                   << floating.bounds.d.w << ','
                   << floating.bounds.d.h;
        }
        stream << ";H" << state.hidden.size();
        for (dock_pane_id pane : state.hidden)
            stream << ',' << pane;
        if (state.version >= 2) {
            stream << ";A" << state.auto_hidden.size();
            for (const dock_auto_hide_pane &pane : state.auto_hidden) {
                stream << ',' << pane.pane << ','
                       << serialize_edge(pane.edge);
            }
        }
        return stream.str();
    }

    dock_layout_state deserialize_dock_layout(
        std::string_view text) {
        return dock_parser(text).parse();
    }

    class dock_layout_manager::implementation final
    {
    public:
        std::vector<dock_pane> panes;
        std::vector<wnd *> children;
        std::optional<dock_layout_node> root;
        std::vector<dock_floating_pane> floating;
        std::vector<dock_pane_id> hidden;
        std::vector<dock_auto_hide_pane> auto_hidden;
        std::vector<dock_layout_region> geometry;
        std::vector<dock_auto_hide_region> auto_hide_geometry;
        dock_node_id next_node = 1;
        int tab_height = 24;
        int splitter_extent = 5;
        int tab_padding = 6;

        const dock_pane *pane(dock_pane_id id) const {
            const auto found = std::find_if(
                panes.begin(), panes.end(),
                [id](const dock_pane &item) { return item.id == id; });
            return found == panes.end() ? nullptr : &*found;
        }

        dock_pane *pane(dock_pane_id id) {
            return const_cast<dock_pane *>(
                static_cast<const implementation *>(this)->pane(id));
        }

        void require_pane(dock_pane_id id) const {
            if (!pane(id))
                throw std::invalid_argument(
                    "Dock pane ID is not registered.");
        }

        dock_layout_node make_tabs(dock_pane_id id) {
            dock_layout_node node;
            node.id = allocate_node();
            node.kind = dock_node_kind::tabs;
            node.panes.push_back(id);
            node.active_pane = id;
            return node;
        }

        dock_node_id allocate_node() {
            if (next_node == 0)
                throw std::overflow_error("Dock node IDs exhausted.");
            return next_node++;
        }

        void remove_from_locations(dock_pane_id id) {
            if (root && erase_pane(*root, id)) {
                const bool empty =
                    root->kind == dock_node_kind::tabs &&
                    root->panes.empty();
                if (empty)
                    root.reset();
            }
            floating.erase(
                std::remove_if(floating.begin(),
                               floating.end(),
                               [id](const dock_floating_pane &item) {
                                   return item.pane == id;
                               }),
                floating.end());
            hidden.erase(std::remove(hidden.begin(), hidden.end(), id),
                         hidden.end());
            auto_hidden.erase(
                std::remove_if(
                    auto_hidden.begin(),
                    auto_hidden.end(),
                    [id](const dock_auto_hide_pane &item) {
                        return item.pane == id;
                    }),
                auto_hidden.end());
        }

        dock_layout_node *target_tabs(dock_pane_id relative_to) {
            if (!root)
                return nullptr;
            if (relative_to != 0)
                return find_pane_node(*root, relative_to);
            return first_tab_node(*root);
        }

        void dock(dock_pane_id id,
                  dock_position position,
                  dock_pane_id relative_to) {
            require_pane(id);
            if (relative_to == id)
                relative_to = 0;
            if (relative_to != 0) {
                require_pane(relative_to);
                if (!root || !contains_pane(*root, relative_to))
                    throw std::invalid_argument(
                        "A relative dock pane must already be docked.");
            }

            remove_from_locations(id);
            if (!root) {
                root = make_tabs(id);
                return;
            }

            dock_layout_node *target =
                relative_to == 0 && position != dock_position::center
                    ? &*root
                    : target_tabs(relative_to);
            if (!target)
                throw std::logic_error(
                    "Dock layout has no tab target.");

            if (position == dock_position::center) {
                target->panes.push_back(id);
                target->active_pane = id;
                return;
            }

            dock_layout_node previous = std::move(*target);
            dock_layout_node added = make_tabs(id);
            dock_layout_node split;
            split.id = allocate_node();
            split.kind = dock_node_kind::split;
            split.orientation =
                position == dock_position::left ||
                        position == dock_position::right
                    ? dock_orientation::horizontal
                    : dock_orientation::vertical;
            split.split_ratio = 0.5f;

            const bool before = position == dock_position::left ||
                                position == dock_position::top;
            if (before) {
                split.children.push_back(std::move(added));
                split.children.push_back(std::move(previous));
            } else {
                split.children.push_back(std::move(previous));
                split.children.push_back(std::move(added));
            }
            *target = std::move(split);
        }

        size minimum(const dock_layout_node &node) const {
            if (node.kind == dock_node_kind::tabs) {
                int width = 1;
                int height = tab_height + 1;
                for (dock_pane_id id : node.panes) {
                    const dock_pane *item = pane(id);
                    if (!item)
                        continue;
                    width = std::max(
                        width, static_cast<int>(item->minimum_size.w));
                    height = std::max(
                        height,
                        tab_height +
                            static_cast<int>(item->minimum_size.h));
                }
                return size(to_dim(width), to_dim(height));
            }

            if (node.children.size() != 2)
                return size(1, 1);
            const size first = minimum(node.children[0]);
            const size second = minimum(node.children[1]);
            if (node.orientation == dock_orientation::horizontal) {
                return size(
                    to_dim(static_cast<int>(first.w) +
                           static_cast<int>(second.w) +
                           splitter_extent),
                    std::max(first.h, second.h));
            }
            return size(
                std::max(first.w, second.w),
                to_dim(static_cast<int>(first.h) +
                       static_cast<int>(second.h) +
                       splitter_extent));
        }

        void layout_node(dock_layout_node &node, const rect &bounds) {
            const std::size_t geometry_index = geometry.size();
            geometry.push_back(
                {node.id, node.kind, bounds, {}, {}, {}, {}});

            if (node.kind == dock_node_kind::tabs) {
                const int header = std::min(
                    tab_height, static_cast<int>(bounds.d.h));
                geometry[geometry_index].tab_strip = make_rect(
                    bounds.p.x, bounds.p.y, bounds.d.w, header);
                geometry[geometry_index].content = make_rect(
                    bounds.p.x,
                    static_cast<int>(bounds.p.y) + header,
                    bounds.d.w,
                    static_cast<int>(bounds.d.h) - header);

                const int count = static_cast<int>(node.panes.size());
                int x = bounds.p.x;
                for (int index = 0; index < count; ++index) {
                    const int remaining =
                        rect_right(bounds) - x;
                    const int tabs_left = count - index;
                    const int width = tabs_left > 0
                                          ? std::max(
                                                0, remaining / tabs_left)
                                          : 0;
                    rect tab = make_rect(x, bounds.p.y, width, header);
                    rect pin;
                    rect close;
                    const dock_pane *item = pane(
                        node.panes[static_cast<std::size_t>(index)]);
                    const int button_size = caption_button_extent(header);
                    int button_x = x + width - tab_padding;
                    if (item && item->closable && header >= 10 &&
                        width >= 20) {
                        button_x -= button_size;
                        close = make_rect(
                            button_x,
                            bounds.p.y + (header - button_size) / 2,
                            button_size,
                            button_size);
                    }
                    if (item && item->pinnable && header >= 10 &&
                        button_x - x >= button_size + tab_padding) {
                        button_x -= button_size + 2;
                        pin = make_rect(
                            button_x,
                            bounds.p.y + (header - button_size) / 2,
                            button_size,
                            button_size);
                    }
                    geometry[geometry_index].tabs.push_back(
                        {node.panes[static_cast<std::size_t>(index)],
                         tab,
                         pin,
                         close});
                    x += width;
                }

                // Every pane in a tab stack occupies the same content
                // rectangle.  Keeping inactive panes in sync matters when
                // an auto-hide strip offsets the dock surface: activating a
                // different tab must not briefly expose its stale geometry.
                for (dock_pane_id id : node.panes) {
                    if (const dock_pane *item = pane(id);
                        item && item->content) {
                        item->content->set_bounds(
                            geometry[geometry_index].content);
                    }
                }
                return;
            }

            if (node.children.size() != 2)
                return;
            const bool horizontal =
                node.orientation == dock_orientation::horizontal;
            const int total = horizontal
                                  ? static_cast<int>(bounds.d.w)
                                  : static_cast<int>(bounds.d.h);
            const int separator = std::min(splitter_extent, total);
            const int available = std::max(0, total - separator);
            const size first_minimum = minimum(node.children[0]);
            const size second_minimum = minimum(node.children[1]);
            const int first_min = horizontal
                                      ? first_minimum.w
                                      : first_minimum.h;
            const int second_min = horizontal
                                       ? second_minimum.w
                                       : second_minimum.h;

            int first = static_cast<int>(
                std::lround(available * std::clamp(
                    node.split_ratio,
                    minimum_split_ratio,
                    maximum_split_ratio)));
            if (first_min + second_min <= available) {
                first = std::clamp(first,
                                   first_min,
                                   available - second_min);
            } else if (first_min + second_min > 0) {
                first = static_cast<int>(std::lround(
                    available *
                    (static_cast<double>(first_min) /
                     (first_min + second_min))));
            }
            first = std::clamp(first, 0, available);
            const int second = available - first;
            if (available > 0)
                node.split_ratio = static_cast<float>(first) / available;

            rect first_bounds;
            rect second_bounds;
            rect splitter;
            if (horizontal) {
                first_bounds = make_rect(bounds.p.x,
                                         bounds.p.y,
                                         first,
                                         bounds.d.h);
                splitter = make_rect(
                    static_cast<int>(bounds.p.x) + first,
                    bounds.p.y,
                    separator,
                    bounds.d.h);
                second_bounds = make_rect(
                    static_cast<int>(bounds.p.x) + first + separator,
                    bounds.p.y,
                    second,
                    bounds.d.h);
            } else {
                first_bounds = make_rect(bounds.p.x,
                                         bounds.p.y,
                                         bounds.d.w,
                                         first);
                splitter = make_rect(
                    bounds.p.x,
                    static_cast<int>(bounds.p.y) + first,
                    bounds.d.w,
                    separator);
                second_bounds = make_rect(
                    bounds.p.x,
                    static_cast<int>(bounds.p.y) + first + separator,
                    bounds.d.w,
                    second);
            }
            geometry[geometry_index].splitter = splitter;
            layout_node(node.children[0], first_bounds);
            layout_node(node.children[1], second_bounds);
        }

        rect layout_auto_hide(const rect &bounds) {
            auto_hide_geometry.clear();
            bool has_left = false;
            bool has_right = false;
            bool has_top = false;
            bool has_bottom = false;
            for (const dock_auto_hide_pane &item : auto_hidden) {
                has_left = has_left ||
                           item.edge == dock_position::left;
                has_right = has_right ||
                            item.edge == dock_position::right;
                has_top = has_top || item.edge == dock_position::top;
                has_bottom = has_bottom ||
                             item.edge == dock_position::bottom;
            }

            const int left = has_left ? tab_height : 0;
            const int right = has_right ? tab_height : 0;
            const int top = has_top ? tab_height : 0;
            const int bottom = has_bottom ? tab_height : 0;
            const rect content = make_rect(
                bounds.p.x + left,
                bounds.p.y + top,
                std::max(0,
                         static_cast<int>(bounds.d.w) - left - right),
                std::max(0,
                         static_cast<int>(bounds.d.h) - top - bottom));

            int left_y = content.p.y;
            int right_y = content.p.y;
            int top_x = content.p.x;
            int bottom_x = content.p.x;
            for (const dock_auto_hide_pane &item : auto_hidden) {
                const dock_pane *registered = pane(item.pane);
                const int title_extent = registered
                    ? std::clamp<int>(
                          static_cast<int>(registered->title.size()) * 8 +
                              tab_padding * 2,
                          tab_height * 2,
                          160)
                    : tab_height * 2;
                rect tab;
                switch (item.edge) {
                case dock_position::left:
                    tab = make_rect(bounds.p.x,
                                    left_y,
                                    tab_height,
                                    std::min(title_extent,
                                             rect_bottom(content) - left_y));
                    left_y += tab.d.h;
                    break;
                case dock_position::right:
                    tab = make_rect(rect_right(bounds) - tab_height,
                                    right_y,
                                    tab_height,
                                    std::min(title_extent,
                                             rect_bottom(content) - right_y));
                    right_y += tab.d.h;
                    break;
                case dock_position::top:
                    tab = make_rect(top_x,
                                    bounds.p.y,
                                    std::min(title_extent,
                                             rect_right(content) - top_x),
                                    tab_height);
                    top_x += tab.d.w;
                    break;
                case dock_position::bottom:
                    tab = make_rect(bottom_x,
                                    rect_bottom(bounds) - tab_height,
                                    std::min(title_extent,
                                             rect_right(content) - bottom_x),
                                    tab_height);
                    bottom_x += tab.d.w;
                    break;
                case dock_position::center:
                    continue;
                }
                auto_hide_geometry.push_back(
                    {item.pane, item.edge, tab});
            }
            return content;
        }

        std::optional<dock_layout_node> filter_node(
            const dock_layout_node &source,
            std::unordered_set<dock_pane_id> &placed,
            std::unordered_set<dock_node_id> &nodes) const {
            if (source.id == 0 || !nodes.insert(source.id).second)
                throw std::invalid_argument(
                    "Dock layout node IDs must be unique and non-zero.");

            if (source.kind == dock_node_kind::tabs) {
                if (!source.children.empty())
                    throw std::invalid_argument(
                        "A dock tab node cannot contain child nodes.");
                dock_layout_node result = source;
                result.children.clear();
                result.panes.clear();
                for (dock_pane_id id : source.panes) {
                    if (!pane(id))
                        continue;
                    if (!placed.insert(id).second)
                        throw std::invalid_argument(
                            "A dock pane occurs more than once.");
                    result.panes.push_back(id);
                }
                if (result.panes.empty())
                    return std::nullopt;
                if (std::find(result.panes.begin(),
                              result.panes.end(),
                              source.active_pane) ==
                    result.panes.end()) {
                    result.active_pane = result.panes.front();
                }
                return result;
            }

            if (source.kind != dock_node_kind::split)
                throw std::invalid_argument(
                    "A dock layout contains an unknown node kind.");
            if (!source.panes.empty() || source.children.size() != 2)
                throw std::invalid_argument(
                    "A dock split requires exactly two child nodes.");
            if (!std::isfinite(source.split_ratio))
                throw std::invalid_argument(
                    "A dock split ratio must be finite.");

            auto first = filter_node(source.children[0], placed, nodes);
            auto second = filter_node(source.children[1], placed, nodes);
            if (!first)
                return second;
            if (!second)
                return first;

            dock_layout_node result = source;
            result.panes.clear();
            result.active_pane = 0;
            result.split_ratio = std::clamp(source.split_ratio,
                                            minimum_split_ratio,
                                            maximum_split_ratio);
            result.children.clear();
            result.children.push_back(std::move(*first));
            result.children.push_back(std::move(*second));
            return result;
        }

        void restore(const dock_layout_state &state) {
            if (state.version < minimum_dock_layout_version ||
                state.version > dock_layout_version)
                throw std::invalid_argument(
                    "Unsupported Native Dock layout version.");

            std::unordered_set<dock_pane_id> placed;
            std::unordered_set<dock_node_id> nodes;
            std::optional<dock_layout_node> filtered;
            if (state.root)
                filtered = filter_node(*state.root, placed, nodes);

            std::vector<dock_floating_pane> new_floating;
            for (const dock_floating_pane &item : state.floating) {
                const dock_pane *registered = pane(item.pane);
                if (!registered)
                    continue;
                if (!registered->floatable)
                    continue;
                if (!item.bounds.d.w || !item.bounds.d.h)
                    throw std::invalid_argument(
                        "Floating dock bounds must be non-empty.");
                if (!placed.insert(item.pane).second)
                    throw std::invalid_argument(
                        "A dock pane occurs more than once.");
                new_floating.push_back(item);
            }

            std::vector<dock_pane_id> new_hidden;
            for (dock_pane_id id : state.hidden) {
                if (!pane(id))
                    continue;
                if (!placed.insert(id).second)
                    throw std::invalid_argument(
                        "A dock pane occurs more than once.");
                new_hidden.push_back(id);
            }

            std::vector<dock_auto_hide_pane> new_auto_hidden;
            for (const dock_auto_hide_pane &item :
                 state.auto_hidden) {
                const dock_pane *registered = pane(item.pane);
                if (!registered)
                    continue;
                if (!registered->pinnable)
                    continue;
                if (item.edge == dock_position::center)
                    throw std::invalid_argument(
                        "An auto-hide pane requires a host edge.");
                if (!placed.insert(item.pane).second)
                    throw std::invalid_argument(
                        "A dock pane occurs more than once.");
                new_auto_hidden.push_back(item);
            }

            dock_node_id maximum = 0;
            if (filtered)
                collect_node_ids(*filtered, maximum);
            if (maximum == std::numeric_limits<dock_node_id>::max())
                throw std::overflow_error("Dock node IDs exhausted.");

            root = std::move(filtered);
            floating = std::move(new_floating);
            hidden = std::move(new_hidden);
            auto_hidden = std::move(new_auto_hidden);
            next_node = maximum + 1;
            if (next_node == 0)
                next_node = 1;

            for (const dock_pane &item : panes) {
                if (placed.find(item.id) == placed.end())
                    dock(item.id, dock_position::center, 0);
            }
        }
    };

    dock_layout_manager::dock_layout_manager()
        : _impl(std::make_unique<implementation>()) {}

    dock_layout_manager::~dock_layout_manager() = default;

    dock_layout_manager &dock_layout_manager::add_pane(
        const dock_pane &pane,
        dock_position position,
        dock_pane_id relative_to) {
        if (pane.id == 0 || !pane.content)
            throw std::invalid_argument(
                "A dock pane requires a non-zero ID and content.");
        if (_impl->pane(pane.id))
            throw std::invalid_argument(
                "Dock pane IDs must be unique.");
        const auto same_content = std::find_if(
            _impl->panes.begin(),
            _impl->panes.end(),
            [&](const dock_pane &item) {
                return item.content == pane.content;
            });
        if (same_content != _impl->panes.end())
            throw std::invalid_argument(
                "A content window can belong to only one dock pane.");

        dock_pane normalized = pane;
        normalized.minimum_size = size(
            std::max<dim>(1, normalized.minimum_size.w),
            std::max<dim>(1, normalized.minimum_size.h));
        _impl->panes.push_back(std::move(normalized));
        try {
            _impl->dock(pane.id, position, relative_to);
        } catch (...) {
            _impl->panes.pop_back();
            throw;
        }
        return *this;
    }

    dock_layout_manager &dock_layout_manager::remove_pane(
        dock_pane_id pane) {
        _impl->require_pane(pane);
        wnd *content = _impl->pane(pane)->content;
        _impl->remove_from_locations(pane);
        _impl->panes.erase(
            std::remove_if(_impl->panes.begin(),
                           _impl->panes.end(),
                           [pane](const dock_pane &item) {
                               return item.id == pane;
                           }),
            _impl->panes.end());
        _impl->children.erase(
            std::remove(_impl->children.begin(),
                        _impl->children.end(),
                        content),
            _impl->children.end());
        return *this;
    }

    const dock_pane *dock_layout_manager::get_pane(
        dock_pane_id pane) const {
        return _impl->pane(pane);
    }

    const std::vector<dock_pane> &
    dock_layout_manager::get_panes() const {
        return _impl->panes;
    }

    dock_pane_location dock_layout_manager::get_pane_location(
        dock_pane_id pane) const {
        _impl->require_pane(pane);
        if (_impl->root && contains_pane(*_impl->root, pane))
            return dock_pane_location::docked;
        const auto floating = std::find_if(
            _impl->floating.begin(),
            _impl->floating.end(),
            [pane](const dock_floating_pane &item) {
                return item.pane == pane;
            });
        if (floating != _impl->floating.end())
            return dock_pane_location::floating;
        const auto auto_hidden = std::find_if(
            _impl->auto_hidden.begin(),
            _impl->auto_hidden.end(),
            [pane](const dock_auto_hide_pane &item) {
                return item.pane == pane;
            });
        return auto_hidden != _impl->auto_hidden.end()
                   ? dock_pane_location::auto_hidden
                   : dock_pane_location::hidden;
    }

    dock_layout_manager &dock_layout_manager::dock(
        dock_pane_id pane,
        dock_position position,
        dock_pane_id relative_to) {
        _impl->dock(pane, position, relative_to);
        return *this;
    }

    dock_layout_manager &dock_layout_manager::float_pane(
        dock_pane_id pane,
        const rect &bounds) {
        _impl->require_pane(pane);
        const dock_pane *item = _impl->pane(pane);
        if (!item->floatable)
            throw std::invalid_argument(
                "This dock pane cannot float.");
        if (!bounds.d.w || !bounds.d.h)
            throw std::invalid_argument(
                "Floating dock bounds must be non-empty.");
        _impl->remove_from_locations(pane);
        _impl->floating.push_back({pane, bounds});
        return *this;
    }

    dock_layout_manager &dock_layout_manager::auto_hide_pane(
        dock_pane_id pane,
        dock_position edge) {
        _impl->require_pane(pane);
        const dock_pane *item = _impl->pane(pane);
        if (!item->pinnable)
            throw std::invalid_argument(
                "This dock pane cannot be auto-hidden.");
        if (edge == dock_position::center)
            throw std::invalid_argument(
                "An auto-hide pane requires a host edge.");
        _impl->remove_from_locations(pane);
        _impl->auto_hidden.push_back({pane, edge});
        return *this;
    }

    dock_layout_manager &dock_layout_manager::pin_pane(
        dock_pane_id pane,
        dock_position position,
        dock_pane_id relative_to) {
        _impl->require_pane(pane);
        const auto found = std::find_if(
            _impl->auto_hidden.begin(),
            _impl->auto_hidden.end(),
            [pane](const dock_auto_hide_pane &item) {
                return item.pane == pane;
            });
        if (found == _impl->auto_hidden.end())
            throw std::invalid_argument(
                "Dock pane is not auto-hidden.");
        _impl->dock(pane, position, relative_to);
        return *this;
    }

    dock_layout_manager &dock_layout_manager::hide_pane(
        dock_pane_id pane) {
        _impl->require_pane(pane);
        _impl->remove_from_locations(pane);
        _impl->hidden.push_back(pane);
        return *this;
    }

    dock_layout_manager &dock_layout_manager::show_pane(
        dock_pane_id pane,
        dock_position position,
        dock_pane_id relative_to) {
        _impl->require_pane(pane);
        _impl->dock(pane, position, relative_to);
        return *this;
    }

    dock_layout_manager &dock_layout_manager::activate_pane(
        dock_pane_id pane) {
        _impl->require_pane(pane);
        if (!_impl->root)
            throw std::invalid_argument("Dock pane is not docked.");
        dock_layout_node *node = find_pane_node(*_impl->root, pane);
        if (!node)
            throw std::invalid_argument("Dock pane is not docked.");
        node->active_pane = pane;
        return *this;
    }

    dock_layout_manager &dock_layout_manager::move_tab(
        dock_pane_id pane,
        dock_pane_id before) {
        _impl->require_pane(pane);
        if (!_impl->root)
            throw std::invalid_argument("Dock pane is not docked.");
        dock_layout_node *node = find_pane_node(*_impl->root, pane);
        if (!node)
            throw std::invalid_argument("Dock pane is not docked.");
        if (before != 0 && !contains_pane(*node, before))
            throw std::invalid_argument(
                "The tab insertion target is not a sibling.");

        node->panes.erase(std::remove(node->panes.begin(),
                                      node->panes.end(),
                                      pane),
                          node->panes.end());
        const auto insertion = before == 0
                                   ? node->panes.end()
                                   : std::find(node->panes.begin(),
                                               node->panes.end(),
                                               before);
        node->panes.insert(insertion, pane);
        node->active_pane = pane;
        return *this;
    }

    dock_layout_manager &dock_layout_manager::set_split_ratio(
        dock_node_id node,
        float ratio) {
        if (!_impl->root)
            throw std::invalid_argument("Dock split does not exist.");
        dock_layout_node *split = find_node(*_impl->root, node);
        if (!split || split->kind != dock_node_kind::split)
            throw std::invalid_argument("Dock split does not exist.");
        if (!std::isfinite(ratio))
            throw std::invalid_argument(
                "A dock split ratio must be finite.");
        split->split_ratio = std::clamp(ratio,
                                        minimum_split_ratio,
                                        maximum_split_ratio);
        return *this;
    }

    dock_layout_state dock_layout_manager::get_state() const {
        dock_layout_state state;
        state.version = dock_layout_version;
        state.root = _impl->root;
        state.floating = _impl->floating;
        state.hidden = _impl->hidden;
        state.auto_hidden = _impl->auto_hidden;
        return state;
    }

    dock_layout_manager &dock_layout_manager::set_state(
        const dock_layout_state &state) {
        _impl->restore(state);
        return *this;
    }

    const std::vector<dock_layout_region> &
    dock_layout_manager::get_regions() const {
        return _impl->geometry;
    }

    const std::vector<dock_auto_hide_region> &
    dock_layout_manager::get_auto_hide_regions() const {
        return _impl->auto_hide_geometry;
    }

    dock_layout_manager &dock_layout_manager::set_metrics(
        int tab_height,
        int splitter_extent,
        int tab_padding) {
        _impl->tab_height = std::max(12, tab_height);
        _impl->splitter_extent = std::max(1, splitter_extent);
        _impl->tab_padding = std::max(2, tab_padding);
        return *this;
    }

    void dock_layout_manager::relayout(wnd *parent,
                                       const rect &bounds) {
        if (!parent)
            return;
        _impl->geometry.clear();
        const rect content = _impl->layout_auto_hide(bounds);
        if (_impl->root)
            _impl->layout_node(*_impl->root, content);
    }

    void dock_layout_manager::add_child(wnd *child) {
        if (!child)
            return;
        if (std::find(_impl->children.begin(),
                      _impl->children.end(),
                      child) == _impl->children.end()) {
            _impl->children.push_back(child);
        }
    }

    void dock_layout_manager::remove_child(wnd *child) {
        _impl->children.erase(
            std::remove(_impl->children.begin(),
                        _impl->children.end(),
                        child),
            _impl->children.end());
    }

    const std::vector<wnd *> &dock_layout_manager::children() const {
        return _impl->children;
    }
} // namespace native

namespace
{
    // Owned top-level shell used for one floating pane. Native close is
    // reported after the portable base has reconciled child resources.
    class dock_floating_window final : public native::modeless_wnd
    {
    public:
        dock_floating_window(native::app_wnd &owner,
                             const std::string &title,
                             const native::rect &bounds,
                             std::function<void()> closed)
            : native::modeless_wnd(owner, title, bounds)
            , _closed(std::move(closed)) {}

        void on_native_destroy() override {
            const bool notify = get_created() && !_suppress_close;
            native::modeless_wnd::on_native_destroy();
            if (notify && _closed)
                _closed();
        }

        void transition_destroy() {
            _suppress_close = true;
            destroy();
            _suppress_close = false;
        }

        native::point pointer_screen_position() const {
            return get_mouse_screen_position();
        }

        bool get_native_title_visible() const override {
            return false;
        }

    private:
        bool _suppress_close = false;
        std::function<void()> _closed;
    };

    struct integer_point
    {
        int x = 0;
        int y = 0;
    };

    integer_point screen_origin(const native::wnd &window) {
        integer_point result;
        const native::wnd *current = &window;
        while (current) {
            const native::point position = current->get_position();
            result.x += position.x;
            result.y += position.y;
            current = current->get_parent();
        }
        return result;
    }

    bool same_rect(const native::rect &a, const native::rect &b) {
        return a.p.x == b.p.x && a.p.y == b.p.y &&
               a.d.w == b.d.w && a.d.h == b.d.h;
    }

    bool empty_rect(const native::rect &bounds) {
        return bounds.d.w == 0 || bounds.d.h == 0;
    }

    native::rect relative_to(const native::rect &bounds,
                             const native::rect &parent) {
        return make_rect(
            static_cast<int>(bounds.p.x) - parent.p.x,
            static_cast<int>(bounds.p.y) - parent.p.y,
            bounds.d.w,
            bounds.d.h);
    }

    struct dock_guide_target
    {
        native::dock_position position =
            native::dock_position::center;
        native::rect bounds;
    };

    class dock_guide_surface final : public native::accordion
    {
    public:
        using paint_function = std::function<void(
            native::gpx &,
            native::theme &,
            native::dock_position,
            const native::rect &,
            const native::theme::state &)>;

        dock_guide_surface(native::dock_position position,
                           const native::rect &bounds,
                           paint_function paint)
            : native::accordion(bounds)
            , _position(position)
            , _paint(std::move(paint)) {}

        void set_drop_state(const native::theme::state &state) {
            if (_drop_state.hot == state.hot &&
                _drop_state.pressed == state.pressed &&
                _drop_state.selected == state.selected &&
                _drop_state.disabled == state.disabled &&
                _drop_state.focused == state.focused &&
                _drop_state.active == state.active) {
                return;
            }
            _drop_state = state;
            if (get_created())
                invalidate();
        }

    protected:
        void draw_background(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &native_state) override {
            native::theme::state state = _drop_state;
            state.disabled = state.disabled || native_state.disabled;
            state.focused = state.focused || native_state.focused;
            state.hot = state.hot || native_state.hot;
            state.pressed = state.pressed || native_state.pressed;
            state.selected = state.selected || native_state.selected;
            _paint(graphics, appearance, _position, bounds, state);
        }

    private:
        native::dock_position _position;
        native::theme::state _drop_state;
        paint_function _paint;
    };

    class dock_destination_surface final : public native::accordion
    {
    public:
        using paint_function = std::function<void(
            native::gpx &,
            native::theme &,
            native::dock_pane_id,
            native::dock_position,
            const native::rect &,
            const native::theme::state &)>;

        dock_destination_surface(native::dock_pane_id pane,
                                 native::dock_position position,
                                 const native::rect &bounds,
                                 paint_function paint)
            : native::accordion(bounds)
            , _pane(pane)
            , _position(position)
            , _paint(std::move(paint)) {}

        void set_target(native::dock_pane_id pane,
                        native::dock_position position) {
            if (_pane == pane && _position == position)
                return;
            _pane = pane;
            _position = position;
            if (get_created())
                invalidate();
        }

    protected:
        void draw_background(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &native_state) override {
            native::theme::state state = native_state;
            state.hot = true;
            state.selected = true;
            state.focused = true;
            state.active = true;
            _paint(graphics,
                   appearance,
                   _pane,
                   _position,
                   bounds,
                   state);
        }

    private:
        native::dock_pane_id _pane;
        native::dock_position _position;
        paint_function _paint;
    };

    // A real child surface for a temporarily revealed auto-hide pane. The
    // host's own pixels sit below native table, tree, and editor children, so
    // painting the reveal directly into the host cannot form an overlay.
    class dock_auto_hide_surface final : public native::accordion
    {
    public:
        using paint_function = std::function<void(
            native::gpx &,
            native::theme &,
            native::dock_pane_id,
            const native::rect &)>;

        dock_auto_hide_surface(native::dock_pane_id pane,
                               const native::rect &bounds,
                               paint_function paint)
            : native::accordion(bounds)
            , _pane(pane)
            , _paint(std::move(paint)) {}

        void set_pane(native::dock_pane_id pane) {
            if (_pane == pane)
                return;
            _pane = pane;
            if (get_created())
                invalidate();
        }

    protected:
        void draw_background(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &) override {
            _paint(graphics, appearance, _pane, bounds);
        }

    private:
        native::dock_pane_id _pane;
        paint_function _paint;
    };

    native::rect destination_label_bounds(
        const native::rect &preview,
        const native::rect &surface,
        int preferred_height) {
        const native::rect clipped = preview.intersect(surface);
        if (empty_rect(clipped))
            return {};
        const int padding = 6;
        const int available_width =
            static_cast<int>(clipped.d.w) - padding * 2;
        const int available_height =
            static_cast<int>(clipped.d.h) - padding * 2;
        if (available_width < 32 || available_height < 16)
            return {};
        return make_rect(
            clipped.p.x + padding,
            clipped.p.y + padding,
            std::min(220, available_width),
            std::min(std::max(20, preferred_height),
                     available_height));
    }

    std::array<dock_guide_target, 5> dock_guides(
        const native::rect &bounds,
        int preferred_size,
        int preferred_gap) {
        const int available = std::min<int>(bounds.d.w, bounds.d.h);
        const int gap = std::max(
            0, std::min(preferred_gap, std::max(0, available / 8)));
        const int maximum_size = std::max(
            1, (available - gap * 2) / 3);
        const int target_size = std::max(
            1, std::min(preferred_size, maximum_size));
        const int center_x = bounds.p.x +
                             (static_cast<int>(bounds.d.w) -
                              target_size) /
                                 2;
        const int center_y = bounds.p.y +
                             (static_cast<int>(bounds.d.h) -
                              target_size) /
                                 2;
        const int step = target_size + gap;
        return {{
            {native::dock_position::center,
             make_rect(center_x,
                       center_y,
                       target_size,
                       target_size)},
            {native::dock_position::left,
             make_rect(center_x - step,
                       center_y,
                       target_size,
                       target_size)},
            {native::dock_position::right,
             make_rect(center_x + step,
                       center_y,
                       target_size,
                       target_size)},
            {native::dock_position::top,
             make_rect(center_x,
                       center_y - step,
                       target_size,
                       target_size)},
            {native::dock_position::bottom,
             make_rect(center_x,
                       center_y + step,
                       target_size,
                       target_size)}
        }};
    }

    const dock_guide_target *dock_guide_at(
        const std::array<dock_guide_target, 5> &guides,
        native::point position) {
        const auto found = std::find_if(
            guides.begin(),
            guides.end(),
            [position](const dock_guide_target &guide) {
                return guide.bounds.contains(position);
            });
        return found == guides.end() ? nullptr : &*found;
    }

    void collect_active_panes(const native::dock_layout_node &node,
                              std::unordered_set<
                                  native::dock_pane_id> &active) {
        if (node.kind == native::dock_node_kind::tabs) {
            if (node.active_pane != 0)
                active.insert(node.active_pane);
            return;
        }
        for (const auto &child : node.children)
            collect_active_panes(child, active);
    }

    const native::dock_layout_region *region_at(
        const std::vector<native::dock_layout_region> &regions,
        native::point position) {
        for (auto region = regions.rbegin();
             region != regions.rend(); ++region) {
            if (region->kind == native::dock_node_kind::tabs &&
                region->bounds.contains(position)) {
                return &*region;
            }
        }
        return nullptr;
    }

    const native::dock_layout_region *central_region(
        const std::vector<native::dock_layout_region> &regions,
        const native::rect &host_bounds) {
        const native::point center(
            static_cast<native::coord>(
                host_bounds.p.x + host_bounds.d.w / 2),
            static_cast<native::coord>(
                host_bounds.p.y + host_bounds.d.h / 2));
        if (const native::dock_layout_region *direct =
                region_at(regions, center)) {
            return direct;
        }

        const native::dock_layout_region *closest = nullptr;
        long long closest_distance =
            std::numeric_limits<long long>::max();
        for (const auto &region : regions) {
            if (region.kind != native::dock_node_kind::tabs)
                continue;
            const long long x =
                region.content.p.x + region.content.d.w / 2;
            const long long y =
                region.content.p.y + region.content.d.h / 2;
            const long long dx = x - center.x;
            const long long dy = y - center.y;
            const long long distance = dx * dx + dy * dy;
            if (distance < closest_distance) {
                closest = &region;
                closest_distance = distance;
            }
        }
        return closest;
    }

    const native::dock_layout_region *splitter_at(
        const std::vector<native::dock_layout_region> &regions,
        native::point position) {
        for (const auto &region : regions) {
            if (region.kind == native::dock_node_kind::split &&
                region.splitter.contains(position)) {
                return &region;
            }
        }
        return nullptr;
    }

    const native::dock_tab_region *tab_at(
        const std::vector<native::dock_layout_region> &regions,
        native::point position,
        const native::dock_layout_region **owner = nullptr) {
        for (const auto &region : regions) {
            if (region.kind != native::dock_node_kind::tabs ||
                !region.tab_strip.contains(position)) {
                continue;
            }
            for (const auto &tab : region.tabs) {
                if (tab.bounds.contains(position)) {
                    if (owner)
                        *owner = &region;
                    return &tab;
                }
            }
        }
        return nullptr;
    }

    const native::dock_tab_region *close_at(
        const std::vector<native::dock_layout_region> &regions,
        native::point position) {
        for (const auto &region : regions) {
            for (const auto &tab : region.tabs) {
                if (!empty_rect(tab.close_bounds) &&
                    tab.close_bounds.contains(position)) {
                    return &tab;
                }
            }
        }
        return nullptr;
    }

    const native::dock_tab_region *pin_at(
        const std::vector<native::dock_layout_region> &regions,
        native::point position) {
        for (const auto &region : regions) {
            for (const auto &tab : region.tabs) {
                if (!empty_rect(tab.pin_bounds) &&
                    tab.pin_bounds.contains(position)) {
                    return &tab;
                }
            }
        }
        return nullptr;
    }

    const native::dock_auto_hide_region *auto_hide_at(
        const std::vector<native::dock_auto_hide_region> &regions,
        native::point position) {
        const auto found = std::find_if(
            regions.begin(),
            regions.end(),
            [position](const native::dock_auto_hide_region &region) {
                return region.bounds.contains(position);
            });
        return found == regions.end() ? nullptr : &*found;
    }

    native::rect floating_tab_strip(const native::wnd &window,
                                    int tab_height) {
        const native::size dimensions = window.get_dimensions();
        return make_rect(0,
                         0,
                         dimensions.w,
                         std::min(tab_height,
                                  static_cast<int>(dimensions.h)));
    }

    native::rect floating_content(const native::wnd &window,
                                  int tab_height) {
        const native::size dimensions = window.get_dimensions();
        const int header = std::min(
            tab_height, static_cast<int>(dimensions.h));
        return make_rect(0,
                         header,
                         dimensions.w,
                         static_cast<int>(dimensions.h) - header);
    }

    native::rect floating_close(const native::wnd &window,
                                int tab_height,
                                int padding,
                                bool closable) {
        if (!closable)
            return {};
        const native::rect strip =
            floating_tab_strip(window, tab_height);
        if (strip.d.w < 20 || strip.d.h < 10)
            return {};
        const int extent = caption_button_extent(strip.d.h);
        return make_rect(rect_right(strip) - padding - extent,
                         strip.p.y +
                             (static_cast<int>(strip.d.h) - extent) / 2,
                         extent,
                         extent);
    }

} // namespace

namespace native
{
    class dock_host::implementation final
    {
    public:
        implementation(dock_host &public_host,
                       app_wnd &owner_window,
                       wnd &surface_window)
            : host(public_host)
            , owner(owner_window)
            , surface(surface_window) {
            if (surface.get_layout())
                throw std::invalid_argument(
                    "A dock host requires a surface without a layout.");
            auto installed = std::make_unique<dock_layout_manager>();
            layout = installed.get();
            surface.set_layout(std::move(installed));

            create_connection = surface.on_wnd_create.connect(
                [this]() { return on_surface_create(); });
            paint_connection = surface.on_wnd_paint.connect(
                [this](wnd_paint_event event) {
                    return on_surface_paint(event);
                });
            click_connection = surface.on_mouse_click.connect(
                [this](mouse_event event) {
                    return on_surface_click(event);
                });
            move_connection = surface.on_mouse_move.connect(
                [this](point position) {
                    return on_surface_move(position);
                });
            resize_connection = surface.on_wnd_resize.connect(
                [this](size) {
                    relayout();
                    return false;
                });
        }

        ~implementation() {
            disposing = true;
            destroy_drop_destination_surface();
            destroy_drop_guide_surfaces();
            surface.on_wnd_create.disconnect(create_connection);
            surface.on_wnd_paint.disconnect(paint_connection);
            surface.on_mouse_click.disconnect(click_connection);
            surface.on_mouse_move.disconnect(move_connection);
            surface.on_wnd_resize.disconnect(resize_connection);
            for (const auto &entry : pane_paint_connections) {
                if (entry.second.content) {
                    entry.second.content->on_wnd_paint.disconnect(
                        entry.second.connection);
                    entry.second.content->on_mouse_move.disconnect(
                        entry.second.move_connection);
                    entry.second.content->on_mouse_click.disconnect(
                        entry.second.click_connection);
                }
            }
            pane_paint_connections.clear();

            if (valid_layout()) {
                const std::vector<dock_pane> panes = layout->get_panes();
                for (auto &entry : floating) {
                    if (entry.second.window)
                        entry.second.window->transition_destroy();
                }
                floating.clear();
                for (const dock_pane &pane : panes) {
                    if (!pane.content)
                        continue;
                    if (pane.content->get_created())
                        pane.content->destroy();
                    if (pane.content->get_parent())
                        pane.content->set_parent(nullptr);
                }
                destroy_auto_hide_surface(true);
                surface.set_layout(nullptr);
            } else {
                for (auto &entry : floating) {
                    if (entry.second.window)
                        entry.second.window->transition_destroy();
                }
                floating.clear();
                destroy_auto_hide_surface(true);
            }
        }

        dock_host &host;
        app_wnd &owner;
        wnd &surface;
        dock_layout_manager *layout = nullptr;

        struct floating_record
        {
            std::unique_ptr<dock_floating_window> window;
        };
        std::unordered_map<dock_pane_id, floating_record> floating;

        struct pane_paint_connection
        {
            wnd *content = nullptr;
            int connection = 0;
            int move_connection = 0;
            int click_connection = 0;
        };
        std::unordered_map<dock_pane_id, pane_paint_connection>
            pane_paint_connections;
        std::array<std::unique_ptr<dock_guide_surface>, 5>
            drop_guide_surfaces;
        std::unique_ptr<dock_destination_surface>
            drop_destination_surface;
        std::unique_ptr<dock_auto_hide_surface>
            auto_hide_surface;

        int create_connection = 0;
        int paint_connection = 0;
        int click_connection = 0;
        int move_connection = 0;
        int resize_connection = 0;
        bool disposing = false;
        bool reconciling = false;
        bool metrics_configured = false;
        int tab_height = 24;
        int splitter_extent = 5;
        int tab_padding = 6;
        int guide_size = 32;
        int guide_gap = 2;
        dock_pane_id hot_pane = 0;
        dock_node_id hot_splitter = 0;
        dock_pane_id pressed_close = 0;
        dock_pane_id pressed_pin = 0;
        dock_pane_id revealed_auto_hide = 0;
        dock_position pressed_pin_edge = dock_position::left;
        bool pressed_pin_is_auto_hidden = false;
        mutable dock_auto_hide_pane auto_hide_cache;

        struct drag_state
        {
            bool pressed = false;
            bool dragging = false;
            bool split_changed = false;
            dock_pane_id pane = 0;
            dock_node_id splitter = 0;
            point press;
            dock_floating_window *floating_window = nullptr;
            integer_point initial_screen;
            integer_point initial_window;
        } drag;

        struct drop_state
        {
            bool valid = false;
            bool guides_visible = false;
            dock_pane_id relative_to = 0;
            dock_pane_id before = 0;
            dock_position position = dock_position::center;
            rect preview;
            std::array<dock_guide_target, 5> guides;
            std::optional<dock_position> hot_guide;
        } drop;

        bool valid_layout() const {
            return surface.get_layout() == layout;
        }

        void require_layout() const {
            if (!valid_layout())
                throw std::logic_error(
                    "The dock host's installed layout was replaced.");
        }

        bool on_surface_create() {
            if (!disposing && valid_layout())
                reconcile();
            return false;
        }

        void configure_metrics(theme &appearance) {
            if (metrics_configured)
                return;
            const theme::metrics defaults = appearance.defaults();
            tab_height = std::max(16, defaults.header_height);
            splitter_extent = std::max(
                3, defaults.dock_splitter_extent);
            tab_padding = std::max(3, defaults.header_padding_x);
            guide_size = std::max(16, defaults.dock_guide_size);
            guide_gap = std::max(0, defaults.dock_guide_gap);
            layout->set_metrics(
                tab_height, splitter_extent, tab_padding);
            metrics_configured = true;
            // This is normally reached from the first paint. Recalculate
            // geometry for that paint without recursively invalidating the
            // native surface while XView is already servicing its repaint.
            relayout(false);
            layout_floating_content();
        }

        const dock_layout_node *state_node(dock_node_id id,
                                           dock_layout_state &state) const {
            if (!state.root)
                return nullptr;
            return find_node(*state.root, id);
        }

        dock_pane_id active_in_region(
            const dock_layout_region &region) const {
            dock_layout_state state = layout->get_state();
            const dock_layout_node *node = state_node(region.node, state);
            if (!node || node->kind != dock_node_kind::tabs)
                return region.tabs.empty() ? 0 : region.tabs.front().pane;
            return node->active_pane;
        }

        const dock_auto_hide_pane *auto_hide_pane_state(
            dock_pane_id id) const {
            const dock_layout_state state = layout->get_state();
            const auto found = std::find_if(
                state.auto_hidden.begin(),
                state.auto_hidden.end(),
                [id](const dock_auto_hide_pane &item) {
                    return item.pane == id;
                });
            if (found == state.auto_hidden.end())
                return nullptr;
            auto_hide_cache = *found;
            return &auto_hide_cache;
        }

        rect auto_hide_overlay(dock_pane_id id) const {
            const dock_auto_hide_pane *state =
                auto_hide_pane_state(id);
            const dock_pane *pane = layout->get_pane(id);
            if (!state || !pane)
                return {};
            const size dimensions = surface.get_dimensions();
            const int width = dimensions.w;
            const int height = dimensions.h;
            const int maximum_width = std::max(1, width - tab_height);
            const int maximum_height = std::max(1, height - tab_height);
            const int desired_width = std::clamp<int>(
                std::max<int>(pane->minimum_size.w, width / 3),
                std::min(120, maximum_width),
                maximum_width);
            const int desired_height = std::clamp<int>(
                std::max<int>(pane->minimum_size.h + tab_height,
                              height / 3),
                std::min(100, maximum_height),
                maximum_height);
            switch (state->edge) {
            case dock_position::left:
                return make_rect(tab_height,
                                 0,
                                 desired_width,
                                 height);
            case dock_position::right:
                return make_rect(width - tab_height - desired_width,
                                 0,
                                 desired_width,
                                 height);
            case dock_position::top:
                return make_rect(0,
                                 tab_height,
                                 width,
                                 desired_height);
            case dock_position::bottom:
                return make_rect(0,
                                 height - tab_height - desired_height,
                                 width,
                                 desired_height);
            case dock_position::center:
                return {};
            }
            return {};
        }

        rect auto_hide_caption(dock_pane_id id) const {
            const rect overlay = auto_hide_overlay(id);
            return make_rect(overlay.p.x,
                             overlay.p.y,
                             overlay.d.w,
                             std::min<int>(tab_height, overlay.d.h));
        }

        rect auto_hide_content(dock_pane_id id) const {
            const rect overlay = auto_hide_overlay(id);
            const int header = std::min<int>(tab_height, overlay.d.h);
            return make_rect(overlay.p.x,
                             overlay.p.y + header,
                             overlay.d.w,
                             static_cast<int>(overlay.d.h) - header);
        }

        rect auto_hide_close(dock_pane_id id) const {
            const dock_pane *pane = layout->get_pane(id);
            const rect caption = auto_hide_caption(id);
            if (!pane || !pane->closable || caption.d.w < 20 ||
                caption.d.h < 10) {
                return {};
            }
            const int extent = caption_button_extent(caption.d.h);
            return make_rect(rect_right(caption) - tab_padding - extent,
                             caption.p.y +
                                 (caption.d.h - extent) / 2,
                             extent,
                             extent);
        }

        rect auto_hide_pin(dock_pane_id id) const {
            const dock_pane *pane = layout->get_pane(id);
            const rect caption = auto_hide_caption(id);
            if (!pane || !pane->pinnable || caption.d.w < 20 ||
                caption.d.h < 10) {
                return {};
            }
            const int extent = caption_button_extent(caption.d.h);
            const rect close = auto_hide_close(id);
            const int right = empty_rect(close)
                ? rect_right(caption) - tab_padding
                : close.p.x - 2;
            return make_rect(right - extent,
                             caption.p.y +
                                 (caption.d.h - extent) / 2,
                             extent,
                             extent);
        }

        void paint_auto_hide_surface(
            gpx &graphics,
            theme &appearance,
            dock_pane_id id,
            const rect &bounds) {
            const dock_pane *pane = layout->get_pane(id);
            const rect overlay = auto_hide_overlay(id);
            if (!pane || empty_rect(overlay))
                return;
            const rect caption = relative_to(
                auto_hide_caption(id), overlay);
            const rect pin = relative_to(auto_hide_pin(id), overlay);
            const rect close = relative_to(
                auto_hide_close(id), overlay);
            host.draw_surface(graphics,
                              appearance,
                              bounds,
                              surface_kind::content,
                              theme::state{});
            host.draw_auto_hide_caption(
                graphics,
                appearance,
                *pane,
                caption,
                pin,
                close,
                pressed_pin == id,
                pressed_close == id);
        }

        bool on_auto_hide_surface_click(mouse_event event) {
            if (revealed_auto_hide == 0)
                return false;
            const rect overlay = auto_hide_overlay(
                revealed_auto_hide);
            event.position = point(
                to_coord(static_cast<int>(event.position.x) +
                         overlay.p.x),
                to_coord(static_cast<int>(event.position.y) +
                         overlay.p.y));
            return on_surface_click(event);
        }

        void destroy_auto_hide_surface(bool release = false) {
            if (!auto_hide_surface)
                return;
            if (auto_hide_surface->get_created())
                auto_hide_surface->destroy();
            if (auto_hide_surface->get_parent())
                auto_hide_surface->set_parent(nullptr);
            if (release)
                auto_hide_surface.reset();
        }

        void sync_auto_hide_surface() {
            if (revealed_auto_hide == 0 || !surface.get_created())
                return;
            const rect overlay = auto_hide_overlay(
                revealed_auto_hide);
            if (empty_rect(overlay))
                return;

            bool needs_show = false;
            if (!auto_hide_surface) {
                auto control = std::make_unique<
                    dock_auto_hide_surface>(
                    revealed_auto_hide,
                    overlay,
                    [this](gpx &graphics,
                           theme &appearance,
                           dock_pane_id pane,
                           const rect &bounds) {
                        paint_auto_hide_surface(
                            graphics, appearance, pane, bounds);
                    });
                control->on_mouse_click.connect(
                    [this](mouse_event event) {
                        return on_auto_hide_surface_click(event);
                    });
                auto_hide_surface = std::move(control);
            }
            auto_hide_surface->set_pane(revealed_auto_hide);
            if (auto_hide_surface->get_parent() != &surface) {
                auto_hide_surface->set_parent(&surface);
                needs_show = true;
            }
            if (!same_rect(auto_hide_surface->get_bounds(), overlay)) {
                auto_hide_surface->set_bounds(overlay);
                needs_show = true;
            }
            if (!auto_hide_surface->get_created()) {
                try {
                    auto_hide_surface->create();
                } catch (...) {
                    if (auto_hide_surface->get_created())
                        auto_hide_surface->destroy();
                    if (auto_hide_surface->get_parent())
                        auto_hide_surface->set_parent(nullptr);
                    throw;
                }
                needs_show = true;
            }
            if (needs_show)
                auto_hide_surface->show();
        }

        dock_position nearest_edge(const rect &bounds) const {
            const size dimensions = surface.get_dimensions();
            const int center_x = bounds.p.x + bounds.d.w / 2;
            const int center_y = bounds.p.y + bounds.d.h / 2;
            const int left = center_x;
            const int right = dimensions.w - center_x;
            const int top = center_y;
            const int bottom = dimensions.h - center_y;
            const int nearest = std::min({left, right, top, bottom});
            if (nearest == left)
                return dock_position::left;
            if (nearest == right)
                return dock_position::right;
            if (nearest == top)
                return dock_position::top;
            return dock_position::bottom;
        }

        void paint_tab(gpx &graphics,
                       theme &appearance,
                       const dock_tab_region &tab,
                       bool selected,
                       bool close_pressed,
                       bool pin_pressed = false) {
            const dock_pane *pane = layout->get_pane(tab.pane);
            if (!pane)
                return;
            host.draw_tab(graphics,
                          appearance,
                          *pane,
                          tab,
                          selected,
                          hot_pane == tab.pane,
                          close_pressed,
                          pin_pressed);
        }

        rect overlay_bounds(const rect &surface_bounds,
                            const rect &viewport) const {
            const rect clipped = surface_bounds.intersect(viewport);
            return make_rect(
                static_cast<int>(clipped.p.x) - viewport.p.x,
                static_cast<int>(clipped.p.y) - viewport.p.y,
                clipped.d.w,
                clipped.d.h);
        }

        void paint_drop_overlay(gpx &graphics,
                                theme &appearance,
                                const rect &viewport) {
            const bool raised_guides = std::any_of(
                drop_guide_surfaces.begin(),
                drop_guide_surfaces.end(),
                [](const auto &control) {
                    return control && control->get_created();
                });
            if (!raised_guides && drop.valid &&
                !empty_rect(drop.preview)) {
                const rect preview = overlay_bounds(
                    drop.preview, viewport);
                if (!empty_rect(preview)) {
                    theme::state state;
                    state.hot = true;
                    state.selected = true;
                    state.focused = true;
                    host.draw_drop_preview(
                        graphics, appearance, preview, state);
                }
            }
            // Created guide surfaces own the visible compass. Painting the
            // same guides into the host or pane underneath them produces
            // stacking artifacts and needless exposure churn.
            if (!drop.guides_visible || raised_guides)
                return;
            for (const dock_guide_target &guide : drop.guides) {
                const rect bounds = overlay_bounds(
                    guide.bounds, viewport);
                if (empty_rect(bounds))
                    continue;
                theme::state state;
                state.hot = drop.hot_guide &&
                            *drop.hot_guide == guide.position;
                state.selected = drop.valid &&
                                 drop.position == guide.position;
                host.draw_drop_guide(graphics,
                                     appearance,
                                     guide.position,
                                     bounds,
                                     state);
            }
        }

        bool on_pane_paint(dock_pane_id id,
                           wnd_paint_event event) {
            if (disposing || !valid_layout() ||
                !drop.guides_visible || drop.relative_to != id) {
                return false;
            }
            const dock_pane *pane = layout->get_pane(id);
            if (!pane || !pane->content ||
                !pane->content->get_created()) {
                return false;
            }
            std::unique_ptr<theme> appearance = theme::create(event.g);
            paint_drop_overlay(
                event.g, *appearance, pane->content->get_bounds());
            return false;
        }

        point pane_position_on_surface(const dock_pane &pane,
                                       point position) const {
            if (!pane.content)
                return position;
            const integer_point pane_origin = screen_origin(*pane.content);
            const integer_point surface_origin = screen_origin(surface);
            return point(
                to_coord(pane_origin.x - surface_origin.x + position.x),
                to_coord(pane_origin.y - surface_origin.y + position.y));
        }

        bool on_pane_move(dock_pane_id id, point position) {
            if (disposing || reconciling || !valid_layout())
                return false;
            const dock_pane *pane = layout->get_pane(id);
            if (!pane)
                return false;
            const point surface_position =
                pane_position_on_surface(*pane, position);
            if (revealed_auto_hide != 0 &&
                id != revealed_auto_hide &&
                !auto_hide_overlay(revealed_auto_hide)
                     .contains(surface_position)) {
                collapse_auto_hidden(true);
            }
            return false;
        }

        bool on_pane_click(dock_pane_id id, mouse_event event) {
            if (disposing || reconciling || !valid_layout())
                return false;
            const dock_pane *pane = layout->get_pane(id);
            if (!pane)
                return false;
            event.position = pane_position_on_surface(
                *pane, event.position);
            if (revealed_auto_hide != 0 &&
                id != revealed_auto_hide &&
                !auto_hide_overlay(revealed_auto_hide)
                     .contains(event.position)) {
                collapse_auto_hidden(true);
            }
            return false;
        }

        bool on_surface_paint(wnd_paint_event event) {
            if (disposing || !valid_layout())
                return false;
            std::unique_ptr<theme> appearance = theme::create(event.g);
            configure_metrics(*appearance);

            for (const dock_layout_region &region :
                 layout->get_regions()) {
                if (region.kind == dock_node_kind::split) {
                    theme::state splitter_state;
                    splitter_state.hot =
                        hot_splitter == region.node;
                    splitter_state.pressed =
                        drag.splitter == region.node;
                    host.draw_splitter(
                        event.g,
                        *appearance,
                        region.splitter,
                        region.splitter.d.w >= region.splitter.d.h
                            ? dock_orientation::horizontal
                            : dock_orientation::vertical,
                        splitter_state);
                    continue;
                }

                host.draw_surface(event.g,
                                  *appearance,
                                  region.content,
                                  surface_kind::content,
                                  theme::state{});
                host.draw_surface(event.g,
                                  *appearance,
                                  region.tab_strip,
                                  surface_kind::header,
                                  theme::state{});
                const dock_pane_id active = active_in_region(region);
                for (const dock_tab_region &tab : region.tabs) {
                    paint_tab(event.g,
                              *appearance,
                              tab,
                              tab.pane == active,
                              tab.pane == pressed_close,
                              tab.pane == pressed_pin);
                }
                appearance->draw_separator(
                    make_rect(region.tab_strip.p.x,
                              rect_bottom(region.tab_strip) - 1,
                              region.tab_strip.d.w,
                              1),
                    separator_orientation::horizontal);
            }


            for (const dock_auto_hide_region &region :
                 layout->get_auto_hide_regions()) {
                const dock_pane *pane = layout->get_pane(region.pane);
                if (!pane)
                    continue;
                theme::state state;
                state.hot = hot_pane == region.pane;
                state.selected = revealed_auto_hide == region.pane;
                state.pressed = state.selected;
                host.draw_auto_hide_tab(
                    event.g, *appearance, *pane, region, state);
            }

            const bool raised_auto_hide =
                auto_hide_surface && auto_hide_surface->get_created();
            if (revealed_auto_hide != 0 && !raised_auto_hide) {
                const dock_pane *pane =
                    layout->get_pane(revealed_auto_hide);
                const rect overlay =
                    auto_hide_overlay(revealed_auto_hide);
                const rect caption =
                    auto_hide_caption(revealed_auto_hide);
                const rect pin = auto_hide_pin(revealed_auto_hide);
                const rect close = auto_hide_close(revealed_auto_hide);
                if (pane && !empty_rect(overlay)) {
                    host.draw_surface(event.g,
                                      *appearance,
                                      overlay,
                                      surface_kind::content,
                                      theme::state{});
                    host.draw_auto_hide_caption(
                        event.g,
                        *appearance,
                        *pane,
                        caption,
                        pin,
                        close,
                        pressed_pin == revealed_auto_hide,
                        pressed_close == revealed_auto_hide);
                }
            }

            paint_drop_overlay(
                event.g,
                *appearance,
                rect(point(0, 0), surface.get_dimensions()));
            return true;
        }

        bool on_floating_paint(dock_pane_id id,
                               wnd_paint_event event) {
            auto found = floating.find(id);
            const dock_pane *pane = layout->get_pane(id);
            if (found == floating.end() || !found->second.window ||
                !pane) {
                return false;
            }
            dock_floating_window &window = *found->second.window;
            std::unique_ptr<theme> appearance = theme::create(event.g);
            const rect strip = floating_tab_strip(window, tab_height);
            const rect content = floating_content(window, tab_height);
            const rect close = floating_close(
                window, tab_height, tab_padding, pane->closable);
            host.draw_surface(event.g,
                              *appearance,
                              content,
                              surface_kind::content,
                              theme::state{});
            host.draw_surface(event.g,
                              *appearance,
                              strip,
                              surface_kind::header,
                              theme::state{});
            dock_tab_region tab{id, strip, {}, close};
            paint_tab(event.g,
                      *appearance,
                      tab,
                      true,
                      pressed_close == id);
            appearance->draw_separator(
                make_rect(strip.p.x,
                          rect_bottom(strip) - 1,
                          strip.d.w,
                          1),
                separator_orientation::horizontal);
            return true;
        }

        void create_floating_window(dock_pane_id id,
                                    const rect &bounds) {
            if (floating.find(id) != floating.end())
                return;
            const dock_pane *pane = layout->get_pane(id);
            if (!pane)
                return;

            floating_record record;
            record.window = std::make_unique<dock_floating_window>(
                owner,
                pane->title,
                bounds,
                [this, id]() { on_native_floating_close(id); });
            dock_floating_window *window = record.window.get();
            window->on_wnd_paint.connect(
                [this, id](wnd_paint_event event) {
                    return on_floating_paint(id, event);
                });
            window->on_mouse_click.connect(
                [this, id](mouse_event event) {
                    return on_floating_click(id, event);
                });
            window->on_mouse_move.connect(
                [this, id](point position) {
                    return on_floating_move(id, position);
                });
            window->on_wnd_resize.connect(
                [this, id](size) {
                    on_floating_geometry(id);
                    return false;
                });
            window->on_wnd_move.connect(
                [this, id](point) {
                    on_floating_geometry(id);
                    return false;
                });
            floating.emplace(id, std::move(record));
        }

        void on_native_floating_close(dock_pane_id id) {
            if (disposing || reconciling || !valid_layout())
                return;
            const dock_pane *pane = layout->get_pane(id);
            if (!pane)
                return;

            if (pane->closable) {
                layout->hide_pane(id);
                reconcile();
                emit(dock_action::closed, id);
            } else {
                layout->dock(id, dock_position::center, 0);
                reconcile();
                emit(dock_action::docked,
                     id,
                     0,
                     dock_position::center);
            }
        }

        void on_floating_geometry(dock_pane_id id) {
            if (disposing || reconciling || !valid_layout())
                return;
            auto found = floating.find(id);
            if (found == floating.end() || !found->second.window ||
                !found->second.window->get_created()) {
                return;
            }
            layout->float_pane(id, found->second.window->get_bounds());
            layout_floating_content(id);
        }

        void transition_parent(wnd &content, wnd &new_parent) {
            if (content.get_parent() == &new_parent)
                return;
            if (content.get_created())
                content.destroy();
            content.set_parent(&new_parent);
        }

        void reconcile() {
            require_layout();
            if (reconciling)
                return;
            reconciling = true;
            try {
                const dock_layout_state state = layout->get_state();
                std::unordered_set<dock_pane_id> active;
                if (state.root)
                    collect_active_panes(*state.root, active);
                const bool reveal_valid = std::any_of(
                    state.auto_hidden.begin(),
                    state.auto_hidden.end(),
                    [this](const dock_auto_hide_pane &item) {
                        return item.pane == revealed_auto_hide;
                    });
                if (!reveal_valid)
                    revealed_auto_hide = 0;
                if (revealed_auto_hide != 0)
                    active.insert(revealed_auto_hide);
                std::unordered_map<dock_pane_id, rect> floats;
                for (const dock_floating_pane &item : state.floating)
                    floats[item.pane] = item.bounds;

                // First close shells which no longer represent a float.
                for (auto &entry : floating) {
                    if (floats.find(entry.first) == floats.end() &&
                        entry.second.window &&
                        entry.second.window->get_created()) {
                        entry.second.window->transition_destroy();
                    }
                }

                if (revealed_auto_hide != 0)
                    sync_auto_hide_surface();

                // Establish the desired parent before creating controls.
                for (const dock_pane &pane : layout->get_panes()) {
                    if (!pane.content)
                        continue;
                    auto requested_float = floats.find(pane.id);
                    if (requested_float != floats.end()) {
                        create_floating_window(pane.id,
                                               requested_float->second);
                        dock_floating_window &window =
                            *floating[pane.id].window;
                        window.set_title(pane.title);
                        if (!same_rect(window.get_bounds(),
                                       requested_float->second)) {
                            window.set_bounds(requested_float->second);
                        }
                        if (owner.get_created() && !window.get_created())
                            window.create();
                        if (window.get_created())
                            transition_parent(*pane.content, window);
                    } else if (pane.id == revealed_auto_hide &&
                               auto_hide_surface &&
                               auto_hide_surface->get_created()) {
                        transition_parent(
                            *pane.content, *auto_hide_surface);
                    } else {
                        transition_parent(*pane.content, surface);
                    }
                }

                if (revealed_auto_hide == 0)
                    destroy_auto_hide_surface();

                relayout();
                layout_floating_content();

                for (const dock_pane &pane : layout->get_panes()) {
                    if (!pane.content)
                        continue;
                    const bool should_create =
                        active.find(pane.id) != active.end() ||
                        floats.find(pane.id) != floats.end();
                    wnd *parent = pane.content->get_parent();
                    if (should_create && parent && parent->get_created()) {
                        if (!pane.content->get_created())
                            pane.content->create();
                        pane.content->show();
                    } else if (pane.content->get_created()) {
                        pane.content->destroy();
                    }
                }

                for (const auto &item : floats) {
                    auto found = floating.find(item.first);
                    if (found != floating.end() &&
                        found->second.window &&
                        found->second.window->get_created()) {
                        found->second.window->show();
                        layout->float_pane(
                            item.first,
                            found->second.window->get_bounds());
                    }
                }
                surface.invalidate();
            } catch (...) {
                reconciling = false;
                throw;
            }
            reconciling = false;
        }

        void relayout(bool invalidate_surface = true) {
            if (!valid_layout())
                return;
            const size dimensions = surface.get_dimensions();
            layout->relayout(
                &surface, rect(point(0, 0), dimensions));
            if (revealed_auto_hide != 0) {
                const dock_pane *revealed =
                    layout->get_pane(revealed_auto_hide);
                const rect overlay =
                    auto_hide_overlay(revealed_auto_hide);
                if (auto_hide_surface &&
                    auto_hide_surface->get_parent() == &surface &&
                    !same_rect(auto_hide_surface->get_bounds(), overlay)) {
                    auto_hide_surface->set_bounds(overlay);
                }
                if (revealed && revealed->content) {
                    const rect content =
                        auto_hide_content(revealed_auto_hide);
                    revealed->content->set_bounds(
                        revealed->content->get_parent() ==
                                auto_hide_surface.get()
                            ? relative_to(content, overlay)
                            : content);
                }
            }
            if (invalidate_surface && surface.get_created())
                surface.invalidate();
        }

        void layout_floating_content(dock_pane_id only = 0) {
            for (auto &entry : floating) {
                if (only != 0 && entry.first != only)
                    continue;
                const dock_pane *pane = layout->get_pane(entry.first);
                if (!pane || !pane->content ||
                    !entry.second.window ||
                    pane->content->get_parent() !=
                        entry.second.window.get()) {
                    continue;
                }
                pane->content->set_bounds(floating_content(
                    *entry.second.window, tab_height));
                if (entry.second.window->get_created())
                    entry.second.window->invalidate();
            }
        }

        void emit(dock_action action,
                  dock_pane_id pane = 0,
                  dock_node_id node = 0,
                  dock_position position = dock_position::center) {
            dock_event event;
            event.action = action;
            event.pane = pane;
            event.node = node;
            event.position = position;
            host.on_native_change(event);
        }

        void reset_drag() {
            const drop_state previous = drop;
            const bool raised_guides = std::any_of(
                drop_guide_surfaces.begin(),
                drop_guide_surfaces.end(),
                [](const auto &control) {
                    return control && control->get_created();
                });
            drag = {};
            drop = {};
            destroy_drop_destination_surface();
            destroy_drop_guide_surfaces();
            pressed_close = 0;
            pressed_pin = 0;
            pressed_pin_edge = dock_position::left;
            pressed_pin_is_auto_hidden = false;
            if (!raised_guides) {
                invalidate_drop_content(previous);
                if (surface.get_created())
                    surface.invalidate();
            }
        }

        void update_hot(point position) {
            const dock_tab_region *tab =
                tab_at(layout->get_regions(), position);
            const dock_layout_region *splitter =
                splitter_at(layout->get_regions(), position);
            const dock_auto_hide_region *collapsed = auto_hide_at(
                layout->get_auto_hide_regions(), position);
            const dock_pane_id next = tab
                ? tab->pane
                : (collapsed ? collapsed->pane : 0);
            const dock_node_id next_splitter = splitter
                                                   ? splitter->node
                                                   : 0;
            if (next == hot_pane &&
                next_splitter == hot_splitter)
                return;
            hot_pane = next;
            hot_splitter = next_splitter;
            if (surface.get_created())
                surface.invalidate();
        }

        void begin_tab_drag(dock_pane_id pane,
                            point position,
                            dock_floating_window *window = nullptr) {
            drag = {};
            drag.pressed = true;
            drag.pane = pane;
            drag.press = position;
            drag.floating_window = window;
            if (window) {
                const integer_point origin = screen_origin(*window);
                drag.initial_window = origin;
                const point pointer =
                    window->pointer_screen_position();
                drag.initial_screen = {pointer.x, pointer.y};
            }
        }

        void begin_split_drag(dock_node_id node, point position) {
            drag = {};
            drag.pressed = true;
            drag.splitter = node;
            drag.press = position;
            hot_splitter = node;
            if (surface.get_created())
                surface.invalidate();
        }

        bool moved_far(point position) const {
            return std::abs(static_cast<int>(position.x) - drag.press.x) >=
                       4 ||
                   std::abs(static_cast<int>(position.y) - drag.press.y) >=
                       4;
        }

        rect preview_for(const dock_layout_region &region,
                         dock_position position) const {
            const int width = region.bounds.d.w;
            const int height = region.bounds.d.h;
            switch (position) {
            case dock_position::left:
                return make_rect(region.bounds.p.x,
                                 region.bounds.p.y,
                                 width / 2,
                                 height);
            case dock_position::right:
                return make_rect(region.bounds.p.x + width / 2,
                                 region.bounds.p.y,
                                 width - width / 2,
                                 height);
            case dock_position::top:
                return make_rect(region.bounds.p.x,
                                 region.bounds.p.y,
                                 width,
                                 height / 2);
            case dock_position::bottom:
                return make_rect(region.bounds.p.x,
                                 region.bounds.p.y + height / 2,
                                 width,
                                 height - height / 2);
            case dock_position::center:
                return region.content;
            }
            return region.content;
        }

        void calculate_drop(point position) {
            drop = {};
            const rect surface_bounds(
                point(0, 0), surface.get_dimensions());
            if (!surface_bounds.contains(position))
                return;
            const dock_layout_region *pointer_region =
                region_at(layout->get_regions(), position);
            if (!pointer_region)
                return;

            const auto guides = dock_guides(
                surface_bounds, guide_size, guide_gap);
            const dock_guide_target *guide =
                dock_guide_at(guides, position);
            const dock_layout_region *region = guide
                ? central_region(layout->get_regions(), surface_bounds)
                : pointer_region;
            if (!region)
                return;

            drop.valid = true;
            drop.guides_visible = true;
            drop.relative_to = active_in_region(*region);
            drop.position = dock_position::center;
            drop.preview = region->content;
            // The docking compass belongs to the host, not to whichever
            // pane happens to be below the pointer.  Keeping it fixed at
            // the host center makes the targets predictable throughout a
            // drag and matches native IDE docking behavior.
            drop.guides = guides;

            const dock_layout_region *tab_owner = nullptr;
            if (const dock_tab_region *tab =
                    tab_at(layout->get_regions(),
                           position,
                           &tab_owner)) {
                drop.relative_to = tab->pane;
                drop.before =
                    position.x < tab->bounds.p.x +
                                         static_cast<int>(tab->bounds.d.w) /
                                             2
                        ? tab->pane
                        : 0;
                drop.preview = tab_owner ? tab_owner->tab_strip
                                         : tab->bounds;
                return;
            }

            if (guide) {
                drop.position = guide->position;
                drop.hot_guide = guide->position;
                drop.preview = preview_for(*region, guide->position);
                return;
            }

            const int width = region->bounds.d.w;
            const int height = region->bounds.d.h;
            const int edge_x = std::min(72, std::max(20, width / 4));
            const int edge_y = std::min(72, std::max(20, height / 4));
            const int local_x = position.x - region->bounds.p.x;
            const int local_y = position.y - region->bounds.p.y;
            if (local_x < edge_x) {
                drop.position = dock_position::left;
            } else if (local_x >= width - edge_x) {
                drop.position = dock_position::right;
            } else if (local_y < edge_y) {
                drop.position = dock_position::top;
            } else if (local_y >= height - edge_y) {
                drop.position = dock_position::bottom;
            }
            drop.preview = preview_for(*region, drop.position);
        }

        void invalidate_drop_content(
            const drop_state &current) const {
            if (!current.guides_visible || current.relative_to == 0 ||
                !valid_layout()) {
                return;
            }
            const dock_pane *pane = layout->get_pane(
                current.relative_to);
            if (pane && pane->content &&
                pane->content->get_created()) {
                pane->content->invalidate();
            }
        }

        void update_drop(point position) {
            const drop_state previous = drop;
            calculate_drop(position);
            const bool changed =
                previous.valid != drop.valid ||
                previous.guides_visible != drop.guides_visible ||
                previous.relative_to != drop.relative_to ||
                previous.before != drop.before ||
                previous.position != drop.position ||
                previous.hot_guide != drop.hot_guide ||
                !same_rect(previous.preview, drop.preview);
            if (!changed)
                return;
            const bool raised_guides = std::any_of(
                drop_guide_surfaces.begin(),
                drop_guide_surfaces.end(),
                [](const auto &control) {
                    return control && control->get_created();
                });
            const bool native_guides =
                surface.get_created() && drop.guides_visible;
            if (!raised_guides && !native_guides) {
                invalidate_drop_content(previous);
                if (!previous.guides_visible ||
                    drop.relative_to != previous.relative_to) {
                    invalidate_drop_content(drop);
                }
                if (surface.get_created())
                    surface.invalidate();
            }
            // Native child surfaces form the complete drop overlay.  The
            // compact label names both the operation and target pane without
            // covering native controls; guide children remain above it.
            sync_drop_destination_surface();
            sync_drop_guide_surfaces();
        }

        void resize_split(point position) {
            const auto &regions = layout->get_regions();
            const auto found = std::find_if(
                regions.begin(),
                regions.end(),
                [&](const dock_layout_region &region) {
                    return region.node == drag.splitter;
                });
            if (found == regions.end())
                return;
            dock_layout_state state = layout->get_state();
            const dock_layout_node *node = state_node(found->node, state);
            if (!node || node->kind != dock_node_kind::split)
                return;
            const bool horizontal =
                node->orientation == dock_orientation::horizontal;
            const int total = horizontal ? found->bounds.d.w
                                         : found->bounds.d.h;
            const int available = std::max(1, total - splitter_extent);
            const int offset = horizontal
                                   ? position.x - found->bounds.p.x
                                   : position.y - found->bounds.p.y;
            layout->set_split_ratio(
                found->node,
                static_cast<float>(offset) / available);
            drag.split_changed = true;
            relayout();
        }

        void reveal_auto_hidden(dock_pane_id id,
                                bool notify_user) {
            if (layout->get_pane_location(id) !=
                dock_pane_location::auto_hidden) {
                return;
            }
            if (revealed_auto_hide == id)
                return;
            if (revealed_auto_hide != 0 && notify_user)
                emit(dock_action::auto_hide_collapsed,
                     revealed_auto_hide);
            revealed_auto_hide = id;
            reconcile();
            if (notify_user)
                emit(dock_action::auto_hide_revealed, id);
        }

        void collapse_auto_hidden(bool notify_user) {
            if (revealed_auto_hide == 0)
                return;
            const dock_pane_id id = revealed_auto_hide;
            revealed_auto_hide = 0;
            reconcile();
            if (notify_user)
                emit(dock_action::auto_hide_collapsed, id);
        }

        void user_auto_hide(dock_pane_id id, dock_position edge) {
            const dock_pane *pane = layout->get_pane(id);
            if (!pane || !pane->pinnable)
                return;
            revealed_auto_hide = 0;
            layout->auto_hide_pane(id, edge);
            reconcile();
            emit(dock_action::auto_hidden, id, 0, edge);
        }

        void user_pin(dock_pane_id id) {
            if (layout->get_pane_location(id) !=
                dock_pane_location::auto_hidden) {
                return;
            }
            revealed_auto_hide = 0;
            layout->pin_pane(id, dock_position::center, 0);
            reconcile();
            emit(dock_action::pinned, id);
        }

        bool on_surface_click(mouse_event event) {
            if (disposing || !valid_layout() ||
                event.button != mouse_button::left) {
                return false;
            }
            if (event.action == mouse_action::press) {
                if (revealed_auto_hide != 0) {
                    const dock_pane_id revealed =
                        revealed_auto_hide;
                    const rect overlay = auto_hide_overlay(revealed);
                    if (auto_hide_close(revealed).contains(
                            event.position)) {
                        pressed_close = revealed;
                        begin_tab_drag(revealed, event.position);
                        surface.invalidate();
                        if (auto_hide_surface &&
                            auto_hide_surface->get_created()) {
                            auto_hide_surface->invalidate();
                        }
                        return true;
                    }
                    if (auto_hide_pin(revealed).contains(
                            event.position)) {
                        pressed_pin = revealed;
                        pressed_pin_is_auto_hidden = true;
                        begin_tab_drag(revealed, event.position);
                        surface.invalidate();
                        if (auto_hide_surface &&
                            auto_hide_surface->get_created()) {
                            auto_hide_surface->invalidate();
                        }
                        return true;
                    }
                    if (!overlay.contains(event.position))
                        collapse_auto_hidden(true);
                }
                if (const dock_auto_hide_region *collapsed =
                        auto_hide_at(layout->get_auto_hide_regions(),
                                     event.position)) {
                    const dock_pane_id id = collapsed->pane;
                    reveal_auto_hidden(id, true);
                    return true;
                }
                const dock_layout_region *pin_owner = nullptr;
                if (const dock_tab_region *tab =
                        tab_at(layout->get_regions(),
                               event.position,
                               &pin_owner);
                    tab && !empty_rect(tab->pin_bounds) &&
                    tab->pin_bounds.contains(event.position)) {
                    pressed_pin = tab->pane;
                    pressed_pin_edge = pin_owner
                        ? nearest_edge(pin_owner->bounds)
                        : dock_position::left;
                    begin_tab_drag(tab->pane, event.position);
                    surface.invalidate();
                    return true;
                }
                if (const dock_tab_region *close =
                        close_at(layout->get_regions(),
                                 event.position)) {
                    pressed_close = close->pane;
                    begin_tab_drag(close->pane, event.position);
                    surface.invalidate();
                    return true;
                }
                if (const dock_layout_region *split =
                        splitter_at(layout->get_regions(),
                                    event.position)) {
                    begin_split_drag(split->node, event.position);
                    return true;
                }
                if (const dock_tab_region *tab =
                        tab_at(layout->get_regions(),
                               event.position)) {
                    // Activating a pane reconciles and relays out the host,
                    // which replaces the cached region vector containing
                    // `tab`. Keep the stable ID before that mutation.
                    const dock_pane_id id = tab->pane;
                    layout->activate_pane(id);
                    reconcile();
                    emit(dock_action::activated, id);
                    begin_tab_drag(id, event.position);
                    return true;
                }
                return false;
            }

            if (!drag.pressed)
                return false;
            if (drag.splitter != 0) {
                const dock_node_id node = drag.splitter;
                const bool changed = drag.split_changed;
                reset_drag();
                if (changed)
                    emit(dock_action::split_resized, 0, node);
                return true;
            }
            if (pressed_pin != 0) {
                const dock_pane_id id = pressed_pin;
                bool accepted = false;
                if (pressed_pin_is_auto_hidden) {
                    accepted = auto_hide_pin(id).contains(
                        event.position);
                } else {
                    const dock_tab_region *pin =
                        pin_at(layout->get_regions(), event.position);
                    accepted = pin && pin->pane == id;
                }
                const bool was_auto_hidden =
                    pressed_pin_is_auto_hidden;
                const dock_position edge = pressed_pin_edge;
                reset_drag();
                if (accepted) {
                    if (was_auto_hidden)
                        user_pin(id);
                    else
                        user_auto_hide(id, edge);
                }
                return true;
            }
            if (pressed_close != 0) {
                const dock_pane_id id = pressed_close;
                const dock_tab_region *close =
                    close_at(layout->get_regions(), event.position);
                const bool accepted =
                    (close && close->pane == id) ||
                    (revealed_auto_hide == id &&
                     auto_hide_close(id).contains(event.position));
                reset_drag();
                if (accepted)
                    user_close(id);
                return true;
            }

            const dock_pane_id id = drag.pane;
            const bool was_dragging = drag.dragging;
            const drop_state accepted = drop;
            reset_drag();
            if (!was_dragging)
                return true;
            if (accepted.valid) {
                accept_drop(id, accepted);
                return true;
            }

            const dock_pane *pane = layout->get_pane(id);
            if (pane && pane->floatable) {
                const integer_point origin = screen_origin(surface);
                const integer_point screen{
                    origin.x + event.position.x,
                    origin.y + event.position.y};
                const size current = pane->content
                                         ? pane->content->get_dimensions()
                                         : size(320, 220);
                const int width = std::max<int>(320, current.w);
                const int height =
                    std::max<int>(220, current.h + tab_height);
                const rect bounds = make_rect(screen.x - width / 2,
                                              screen.y - tab_height / 2,
                                              width,
                                              height);
                layout->float_pane(id, bounds);
                reconcile();
                emit(dock_action::floated, id);
            }
            return true;
        }

        bool on_surface_move(point position) {
            if (disposing || !valid_layout())
                return false;
            update_hot(position);
            if (!drag.pressed) {
                if (revealed_auto_hide != 0) {
                    const dock_auto_hide_region *collapsed =
                        auto_hide_at(layout->get_auto_hide_regions(),
                                     position);
                    if (!auto_hide_overlay(revealed_auto_hide)
                             .contains(position) &&
                        (!collapsed ||
                         collapsed->pane != revealed_auto_hide)) {
                        collapse_auto_hidden(true);
                        return true;
                    }
                }
                return false;
            }
            if (drag.splitter != 0) {
                resize_split(position);
                return true;
            }
            if (pressed_close != 0 || pressed_pin != 0)
                return true;
            if (!drag.dragging && moved_far(position))
                drag.dragging = true;
            if (drag.dragging) {
                update_drop(position);
            }
            return true;
        }

        void accept_drop(dock_pane_id id,
                         const drop_state &accepted) {
            dock_pane_id relative = accepted.relative_to;
            if (relative == id)
                relative = 0;
            if (accepted.position == dock_position::center &&
                accepted.before != 0 &&
                layout->get_pane_location(id) ==
                    dock_pane_location::docked) {
                dock_layout_state state = layout->get_state();
                const dock_layout_node *source =
                    state.root ? find_pane_node(*state.root, id)
                               : nullptr;
                if (source && contains_pane(*source,
                                            accepted.before)) {
                    layout->move_tab(id, accepted.before);
                    reconcile();
                    emit(dock_action::tab_reordered, id);
                    return;
                }
            }
            layout->dock(id, accepted.position, relative);
            reconcile();
            emit(dock_action::docked,
                 id,
                 0,
                 accepted.position);
        }

        void user_close(dock_pane_id id) {
            const dock_pane *pane = layout->get_pane(id);
            if (!pane || !pane->closable)
                return;
            layout->hide_pane(id);
            reconcile();
            emit(dock_action::closed, id);
        }

        bool on_floating_click(dock_pane_id id, mouse_event event) {
            if (disposing || !valid_layout() ||
                event.button != mouse_button::left)
                return false;
            auto found = floating.find(id);
            const dock_pane *pane = layout->get_pane(id);
            if (found == floating.end() || !found->second.window ||
                !pane)
                return false;
            dock_floating_window *window = found->second.window.get();
            const rect strip = floating_tab_strip(*window, tab_height);
            const rect close = floating_close(
                *window, tab_height, tab_padding, pane->closable);

            if (event.action == mouse_action::press) {
                if (!strip.contains(event.position))
                    return false;
                if (!empty_rect(close) && close.contains(event.position))
                    pressed_close = id;
                begin_tab_drag(id, event.position, window);
                window->invalidate();
                return true;
            }

            if (!drag.pressed || drag.pane != id ||
                drag.floating_window != window) {
                return false;
            }
            if (pressed_close == id) {
                const bool accepted = close.contains(event.position);
                reset_drag();
                window->invalidate();
                if (accepted)
                    user_close(id);
                return true;
            }

            const bool was_dragging = drag.dragging;
            const drop_state accepted = drop;
            reset_drag();
            window->invalidate();
            if (was_dragging && accepted.valid) {
                accept_drop(id, accepted);
            } else if (was_dragging) {
                layout->float_pane(id, window->get_bounds());
                emit(dock_action::floated, id);
            }
            return true;
        }

        bool on_floating_move(dock_pane_id id, point position) {
            if (disposing || !valid_layout() || !drag.pressed ||
                drag.pane != id || !drag.floating_window)
                return false;
            if (pressed_close == id)
                return true;
            if (!drag.dragging && moved_far(position))
                drag.dragging = true;
            if (!drag.dragging)
                return true;

            dock_floating_window &window = *drag.floating_window;
            const point native_pointer =
                window.pointer_screen_position();
            const integer_point pointer{
                native_pointer.x, native_pointer.y};
            window.set_position(point(
                to_coord(drag.initial_window.x +
                         pointer.x - drag.initial_screen.x),
                to_coord(drag.initial_window.y +
                         pointer.y - drag.initial_screen.y)));
            layout->float_pane(id, window.get_bounds());

            const integer_point surface_origin = screen_origin(surface);
            update_drop(point(
                to_coord(pointer.x - surface_origin.x),
                to_coord(pointer.y - surface_origin.y)));
            return true;
        }

        void destroy_drop_guide_surfaces() {
            for (auto &control : drop_guide_surfaces) {
                if (!control)
                    continue;
                if (control->get_created())
                    control->destroy();
                if (control->get_parent())
                    control->set_parent(nullptr);
                control.reset();
            }
        }

        void destroy_drop_destination_surface() {
            if (!drop_destination_surface)
                return;
            if (drop_destination_surface->get_created())
                drop_destination_surface->destroy();
            if (drop_destination_surface->get_parent())
                drop_destination_surface->set_parent(nullptr);
            drop_destination_surface.reset();
        }

        void sync_drop_destination_surface() {
            if (!drop.valid || empty_rect(drop.preview) ||
                drop.relative_to == 0 || !surface.get_created()) {
                destroy_drop_destination_surface();
                return;
            }

            const size dimensions = surface.get_dimensions();
            const rect surface_bounds = make_rect(
                0, 0, dimensions.w, dimensions.h);
            const rect bounds = destination_label_bounds(
                drop.preview, surface_bounds, tab_height);
            if (empty_rect(bounds)) {
                destroy_drop_destination_surface();
                return;
            }

            bool needs_show = false;
            if (!drop_destination_surface) {
                auto control = std::make_unique<
                    dock_destination_surface>(
                    drop.relative_to,
                    drop.position,
                    bounds,
                    [this](gpx &graphics,
                           theme &appearance,
                           dock_pane_id pane_id,
                           dock_position position,
                           const rect &paint_bounds,
                           const theme::state &state) {
                        const dock_pane *pane =
                            layout->get_pane(pane_id);
                        if (pane) {
                            host.draw_drop_destination(
                                graphics,
                                appearance,
                                *pane,
                                position,
                                paint_bounds,
                                state);
                        }
                    });
                control->set_parent(&surface);
                try {
                    control->create();
                } catch (...) {
                    if (control->get_created())
                        control->destroy();
                    control->set_parent(nullptr);
                    throw;
                }
                drop_destination_surface = std::move(control);
                needs_show = true;
            } else if (!same_rect(
                           drop_destination_surface->get_bounds(),
                           bounds)) {
                drop_destination_surface->set_bounds(bounds);
                needs_show = true;
            }
            drop_destination_surface->set_target(
                drop.relative_to, drop.position);
            if (needs_show)
                drop_destination_surface->show();
        }

        void sync_drop_guide_surfaces() {
            // Unit tests can drive the portable event surface without a
            // native peer.  The surface painter remains their fallback;
            // real windows receive child guide controls so the compass is
            // above native tree, table, and text widgets as well.
            if (!drop.guides_visible || !surface.get_created()) {
                destroy_drop_guide_surfaces();
                return;
            }

            for (std::size_t index = 0;
                 index < drop.guides.size(); ++index) {
                const dock_guide_target &guide = drop.guides[index];
                theme::state state;
                state.hot = drop.hot_guide &&
                            *drop.hot_guide == guide.position;
                state.selected = drop.valid &&
                                 drop.position == guide.position;

                bool needs_show = false;
                if (!drop_guide_surfaces[index]) {
                    auto control = std::make_unique<dock_guide_surface>(
                        guide.position,
                        guide.bounds,
                        [this](gpx &graphics,
                               theme &appearance,
                               dock_position position,
                               const rect &bounds,
                               const theme::state &paint_state) {
                            host.draw_drop_guide(graphics,
                                                 appearance,
                                                 position,
                                                 bounds,
                                                 paint_state);
                        });
                    control->set_drop_state(state);
                    control->set_parent(&surface);
                    try {
                        control->create();
                    } catch (...) {
                        if (control->get_created())
                            control->destroy();
                        control->set_parent(nullptr);
                        throw;
                    }
                    drop_guide_surfaces[index] = std::move(control);
                    needs_show = true;
                } else if (!same_rect(
                               drop_guide_surfaces[index]->get_bounds(),
                               guide.bounds)) {
                    drop_guide_surfaces[index]->set_bounds(guide.bounds);
                    needs_show = true;
                }
                drop_guide_surfaces[index]->set_drop_state(state);
                if (needs_show) {
                    // Map new or repositioned guides without cycling the
                    // other four siblings when only hover state changes.
                    drop_guide_surfaces[index]->show();
                }
            }
        }

        void attach_pane_paint(const dock_pane &pane) {
            if (!pane.content ||
                pane_paint_connections.find(pane.id) !=
                    pane_paint_connections.end()) {
                return;
            }
            pane_paint_connection entry;
            entry.content = pane.content;
            entry.connection = pane.content->on_wnd_paint.connect(
                [this, id = pane.id](wnd_paint_event event) {
                    return on_pane_paint(id, event);
                });
            entry.move_connection = pane.content->on_mouse_move.connect(
                [this, id = pane.id](point position) {
                    return on_pane_move(id, position);
                });
            entry.click_connection = pane.content->on_mouse_click.connect(
                [this, id = pane.id](mouse_event event) {
                    return on_pane_click(id, event);
                });
            pane_paint_connections.emplace(pane.id, entry);
        }

        void detach_pane_paint(dock_pane_id id) {
            const auto found = pane_paint_connections.find(id);
            if (found == pane_paint_connections.end())
                return;
            if (found->second.content) {
                found->second.content->on_wnd_paint.disconnect(
                    found->second.connection);
                found->second.content->on_mouse_move.disconnect(
                    found->second.move_connection);
                found->second.content->on_mouse_click.disconnect(
                    found->second.click_connection);
            }
            pane_paint_connections.erase(found);
        }

        void detach_pane(dock_pane_id id) {
            const dock_pane *pane = layout->get_pane(id);
            if (!pane)
                throw std::invalid_argument(
                    "Dock pane ID is not registered.");
            wnd *content = pane->content;
            detach_pane_paint(id);
            auto shell = floating.find(id);
            if (shell != floating.end() && shell->second.window)
                shell->second.window->transition_destroy();
            floating.erase(id);
            if (content) {
                if (content->get_created())
                    content->destroy();
                if (content->get_parent())
                    content->set_parent(nullptr);
            }
        }
    };

    dock_host::dock_host(app_wnd &surface)
        : dock_host(surface, surface) {}

    dock_host::dock_host(app_wnd &owner, wnd &surface)
        : _impl(std::make_unique<implementation>(
              *this, owner, surface)) {}

    dock_host::~dock_host() = default;

    void dock_host::on_native_change(const dock_event &event) {
        on_change.emit(event);
    }

    void dock_host::draw_tab(
        gpx &graphics,
        theme &appearance,
        const dock_pane &pane,
        const dock_tab_region &tab,
        bool selected,
        bool hot,
        bool close_pressed,
        bool pin_pressed) {
        theme::state state;
        state.selected = selected;
        state.hot = hot;
        state.pressed = selected;
        appearance.draw_surface(
            tab.bounds, surface_kind::header, state);

        const theme::palette colors = appearance.native_palette();
        const rect first_button = !empty_rect(tab.pin_bounds)
                                      ? tab.pin_bounds
                                      : tab.close_bounds;
        const int tab_padding = _impl->tab_padding;
        const int right_padding = empty_rect(first_button)
            ? tab_padding
            : static_cast<int>(tab.bounds.d.w) -
                  (first_button.p.x - tab.bounds.p.x);
        const rect text_bounds = make_rect(
            tab.bounds.p.x + tab_padding,
            tab.bounds.p.y,
            std::max(0,
                     static_cast<int>(tab.bounds.d.w) -
                         tab_padding - right_padding),
            tab.bounds.d.h);
        text_layout text;
        const dock_pane *registered =
            _impl->layout->get_pane(pane.id);
        text.horizontal =
            registered &&
                    _impl->layout->get_pane_location(pane.id) ==
                        dock_pane_location::floating
                ? text_align::center
                : text_align::start;
        text.vertical = text_valign::center;
        text.overflow = text_overflow::ellipsis;
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(selected ? colors.button_pressed_text
                              : colors.button_text)
            .draw_text(pane.title, text_bounds, text);

        theme::state close_state;
        close_state.hot = hot;
        close_state.pressed = close_pressed;
        if (!empty_rect(tab.close_bounds)) {
            appearance.draw_caption_button(
                tab.close_bounds,
                caption_button_kind::close,
                close_state);
        }
        theme::state pin_state;
        pin_state.hot = hot;
        pin_state.pressed = pin_pressed;
        if (!empty_rect(tab.pin_bounds)) {
            appearance.draw_caption_button(
                tab.pin_bounds,
                caption_button_kind::pin,
                pin_state);
        }
    }

    void dock_host::draw_surface(
        gpx &,
        theme &appearance,
        const rect &bounds,
        surface_kind kind,
        const theme::state &state) {
        appearance.draw_surface(bounds, kind, state);
    }

    void dock_host::draw_splitter(
        gpx &,
        theme &appearance,
        const rect &bounds,
        dock_orientation orientation,
        const theme::state &state) {
        appearance.draw_surface(
            bounds, surface_kind::header, state);
        appearance.draw_separator(
            bounds,
            orientation == dock_orientation::horizontal
                ? separator_orientation::horizontal
                : separator_orientation::vertical);
        if (state.hot || state.pressed) {
            // Adjacent native separator strokes form a compact resize grip
            // without introducing platform-specific colors.
            if (orientation == dock_orientation::horizontal &&
                bounds.d.h >= 5) {
                appearance.draw_separator(
                    make_rect(bounds.p.x,
                              bounds.p.y - 1,
                              bounds.d.w,
                              bounds.d.h),
                    separator_orientation::horizontal);
                appearance.draw_separator(
                    make_rect(bounds.p.x,
                              bounds.p.y + 1,
                              bounds.d.w,
                              bounds.d.h),
                    separator_orientation::horizontal);
            } else if (orientation == dock_orientation::vertical &&
                       bounds.d.w >= 5) {
                appearance.draw_separator(
                    make_rect(bounds.p.x - 1,
                              bounds.p.y,
                              bounds.d.w,
                              bounds.d.h),
                    separator_orientation::vertical);
                appearance.draw_separator(
                    make_rect(bounds.p.x + 1,
                              bounds.p.y,
                              bounds.d.w,
                              bounds.d.h),
                    separator_orientation::vertical);
            }
        }
    }

    void dock_host::draw_auto_hide_tab(
        gpx &graphics,
        theme &appearance,
        const dock_pane &pane,
        const dock_auto_hide_region &region,
        const theme::state &state) {
        appearance.draw_surface(
            region.bounds, surface_kind::header, state);
        const theme::palette colors = appearance.native_palette();
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.selected ? colors.button_pressed_text
                                    : colors.button_text);
        if (region.edge == dock_position::left ||
            region.edge == dock_position::right) {
            // Letter tops face inward: clockwise on the left strip and
            // counter-clockwise on the right strip.
            draw_rotated_text(
                graphics,
                pane.title,
                region.bounds,
                region.edge == dock_position::left,
                _impl->tab_padding);
            return;
        }
        graphics.draw_text(
            pane.title,
            region.bounds,
            text_layout{text_align::center,
                        text_valign::center,
                        text_overflow::ellipsis,
                        true});
    }

    void dock_host::draw_auto_hide_caption(
        gpx &graphics,
        theme &appearance,
        const dock_pane &pane,
        const rect &caption,
        const rect &pin,
        const rect &close,
        bool pin_pressed,
        bool close_pressed) {
        appearance.draw_surface(
            caption, surface_kind::header, theme::state{});
        const rect first_button = !empty_rect(pin) ? pin : close;
        rect text_bounds = caption;
        text_bounds.p.x += _impl->tab_padding;
        text_bounds.d.w = to_dim(std::max(
            0,
            (empty_rect(first_button)
                 ? rect_right(caption) - _impl->tab_padding
                 : first_button.p.x) -
                text_bounds.p.x));
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(appearance.native_palette().button_text)
            .draw_text(
                pane.title,
                text_bounds,
                text_layout{text_align::start,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});
        theme::state pin_state;
        pin_state.pressed = pin_pressed;
        if (!empty_rect(pin)) {
            appearance.draw_caption_button(
                pin, caption_button_kind::unpin, pin_state);
        }
        theme::state close_state;
        close_state.pressed = close_pressed;
        if (!empty_rect(close)) {
            appearance.draw_caption_button(
                close, caption_button_kind::close, close_state);
        }
        appearance.draw_separator(
            make_rect(caption.p.x,
                      rect_bottom(caption) - 1,
                      caption.d.w,
                      1),
            separator_orientation::horizontal);
    }

    void dock_host::draw_drop_preview(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_selection(
            bounds, selection_shape::tile, state);
        appearance.draw_focus(bounds, state);
    }

    void dock_host::draw_drop_destination(
        gpx &graphics,
        theme &appearance,
        const dock_pane &pane,
        dock_position position,
        const rect &bounds,
        const theme::state &state) {
        std::string operation;
        switch (position) {
        case dock_position::left:
            operation = "LEFT OF: ";
            break;
        case dock_position::right:
            operation = "RIGHT OF: ";
            break;
        case dock_position::top:
            operation = "ABOVE: ";
            break;
        case dock_position::bottom:
            operation = "BELOW: ";
            break;
        case dock_position::center:
            operation = "TAB WITH: ";
            break;
        }

        theme::state surface_state = state;
        surface_state.pressed = true;
        appearance.draw_surface(
            bounds, surface_kind::popup, surface_state);
        appearance.draw_focus(bounds, state);
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(appearance.native_palette().button_text)
            .draw_text(
                operation + pane.title,
                bounds,
                text_layout{text_align::center,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});
    }

    void dock_host::draw_drop_guide(
        gpx &,
        theme &appearance,
        dock_position position,
        const rect &bounds,
        const theme::state &state) {
        dock_guide_kind kind = dock_guide_kind::center;
        switch (position) {
        case dock_position::left:
            kind = dock_guide_kind::left;
            break;
        case dock_position::right:
            kind = dock_guide_kind::right;
            break;
        case dock_position::top:
            kind = dock_guide_kind::top;
            break;
        case dock_position::bottom:
            kind = dock_guide_kind::bottom;
            break;
        case dock_position::center:
            break;
        }
        appearance.draw_dock_guide(bounds, kind, state);
    }

    wnd &dock_host::get_surface() const {
        return _impl->surface;
    }

    dock_layout_manager &dock_host::get_layout() const {
        _impl->require_layout();
        return *_impl->layout;
    }

    dock_host &dock_host::add_pane(const dock_pane &pane,
                                   dock_position position,
                                   dock_pane_id relative_to) {
        _impl->require_layout();
        if (dynamic_cast<app_wnd *>(pane.content))
            throw std::invalid_argument(
                "A dock pane content window must be a child control.");
        _impl->layout->add_pane(pane, position, relative_to);
        _impl->attach_pane_paint(pane);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::remove_pane(dock_pane_id pane) {
        _impl->require_layout();
        if (_impl->revealed_auto_hide == pane)
            _impl->revealed_auto_hide = 0;
        _impl->detach_pane(pane);
        _impl->layout->remove_pane(pane);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::dock(dock_pane_id pane,
                               dock_position position,
                               dock_pane_id relative_to) {
        _impl->require_layout();
        if (_impl->revealed_auto_hide == pane)
            _impl->revealed_auto_hide = 0;
        _impl->layout->dock(pane, position, relative_to);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::float_pane(dock_pane_id pane,
                                     const rect &bounds) {
        _impl->require_layout();
        if (_impl->revealed_auto_hide == pane)
            _impl->revealed_auto_hide = 0;
        _impl->layout->float_pane(pane, bounds);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::auto_hide_pane(dock_pane_id pane,
                                         dock_position edge) {
        _impl->require_layout();
        _impl->revealed_auto_hide = 0;
        _impl->layout->auto_hide_pane(pane, edge);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::pin_pane(dock_pane_id pane,
                                   dock_position position,
                                   dock_pane_id relative_to) {
        _impl->require_layout();
        _impl->revealed_auto_hide = 0;
        _impl->layout->pin_pane(pane, position, relative_to);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::reveal_auto_hide(dock_pane_id pane) {
        _impl->require_layout();
        if (_impl->layout->get_pane_location(pane) !=
            dock_pane_location::auto_hidden) {
            throw std::invalid_argument(
                "Dock pane is not auto-hidden.");
        }
        _impl->reveal_auto_hidden(pane, false);
        return *this;
    }

    dock_host &dock_host::collapse_auto_hide() {
        _impl->require_layout();
        _impl->collapse_auto_hidden(false);
        return *this;
    }

    dock_pane_id dock_host::get_revealed_auto_hide() const {
        _impl->require_layout();
        return _impl->revealed_auto_hide;
    }

    dock_host &dock_host::close_pane(dock_pane_id pane) {
        _impl->require_layout();
        const dock_pane *item = _impl->layout->get_pane(pane);
        if (!item)
            throw std::invalid_argument(
                "Dock pane ID is not registered.");
        if (item->closable) {
            if (_impl->revealed_auto_hide == pane)
                _impl->revealed_auto_hide = 0;
            _impl->layout->hide_pane(pane);
            _impl->reconcile();
        }
        return *this;
    }

    dock_host &dock_host::show_pane(dock_pane_id pane,
                                    dock_position position,
                                    dock_pane_id relative_to) {
        _impl->require_layout();
        if (_impl->revealed_auto_hide == pane)
            _impl->revealed_auto_hide = 0;
        _impl->layout->show_pane(pane, position, relative_to);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::activate_pane(dock_pane_id pane) {
        _impl->require_layout();
        _impl->layout->activate_pane(pane);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::move_tab(dock_pane_id pane,
                                   dock_pane_id before) {
        _impl->require_layout();
        _impl->layout->move_tab(pane, before);
        _impl->reconcile();
        return *this;
    }

    dock_host &dock_host::set_split_ratio(dock_node_id node,
                                          float ratio) {
        _impl->require_layout();
        _impl->layout->set_split_ratio(node, ratio);
        _impl->relayout();
        return *this;
    }

    dock_layout_state dock_host::get_layout_state() const {
        _impl->require_layout();
        return _impl->layout->get_state();
    }

    dock_host &dock_host::set_layout_state(
        const dock_layout_state &state) {
        _impl->require_layout();
        _impl->revealed_auto_hide = 0;
        _impl->layout->set_state(state);
        _impl->reconcile();
        return *this;
    }

    std::string dock_host::serialize_layout() const {
        return serialize_dock_layout(get_layout_state());
    }

    dock_host &dock_host::restore_layout(std::string_view text) {
        return set_layout_state(deserialize_dock_layout(text));
    }
} // namespace native

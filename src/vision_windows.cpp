//
// Implements the independent Vision modeless and modal child windows.
// Native controls remain real windows while theme samples are painted.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    // Build one alpha-bearing procedural thumbnail using only the
    // public portable graphics API.
    std::shared_ptr<const native::img> make_thumbnail(
        int index,
        int color_seed) {
        auto image = std::make_shared<native::img>(48, 48);
        native::gpx &graphics = image->get_gpx();
        graphics.clear(native::rgba(0, 0, 0, 0));
        const native::rgba color(
            static_cast<std::uint8_t>((color_seed + index * 47) % 210 + 32),
            static_cast<std::uint8_t>((color_seed * 3 + index * 71) % 210 + 32),
            static_cast<std::uint8_t>((color_seed * 5 + index * 29) % 210 + 32),
            static_cast<std::uint8_t>(index % 3 == 0 ? 190 : 255));
        graphics.set_ink(color);
        if (index % 3 == 0) {
            graphics.draw_ellipse(native::rect(5, 5, 38, 38), true);
        } else if (index % 3 == 1) {
            graphics.draw_rect(native::rect(6, 8, 36, 32), true);
        } else {
            graphics.draw_polygon(
                {{24, 3}, {45, 42}, {3, 42}}, true);
        }
        graphics.set_ink(native::rgba(35, 35, 35, 220))
            .set_pen(2)
            .draw_line({8, 40}, {40, 8});
        return image;
    }

    // Create enough stable-ID entries to force vertical scrolling.
    std::vector<native::icon_view_item> make_items(
        const std::string &prefix,
        int count,
        int color_seed) {
        std::vector<native::icon_view_item> items;
        items.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            items.push_back({prefix + " " + std::to_string(index + 1),
                             make_thumbnail(index, color_seed),
                             static_cast<std::uint64_t>(index + 1),
                             true});
        }
        return items;
    }

    // Create a classic file hierarchy with shared portable icons.
    std::vector<native::tree_view_item> make_tree_items() {
        auto folder = make_thumbnail(1, 41);
        auto document = make_thumbnail(2, 101);
        return {
            {"Project",
             folder,
             100,
             {{"include",
               folder,
               110,
               {{"native.h", document, 111},
                {"tree_view.h", document, 112}},
               true},
              {"lib",
               folder,
               120,
               {{"native",
                 folder,
                 121,
                 {{"tree_view.cpp", document, 122}},
                 true}},
               true},
              {"tests",
               folder,
               130,
               {{"native_window_api_tests.cpp", document, 131}},
               false}},
             true},
            {"README.md", document, 200},
            {"LICENSE", document, 201}};
    }

    // Generate a compact alpha-bearing image for table cells.
    std::shared_ptr<const native::img> make_table_icon(int index) {
        auto image = std::make_shared<native::img>(18, 18);
        native::gpx &graphics = image->get_gpx();
        graphics.clear(native::rgba(0, 0, 0, 0));
        graphics.set_ink(index % 2 == 0
                             ? native::rgba(52, 116, 205, 220)
                             : native::rgba(222, 132, 42, 220));
        if (index % 2 == 0)
            graphics.draw_rect(native::rect(2, 3, 14, 12), true);
        else
            graphics.draw_ellipse(native::rect(2, 2, 14, 14), true);
        return image;
    }

    // Return the four semantic columns shared by both table demos.
    std::vector<native::table_column> make_table_columns() {
        native::table_column name;
        name.id = 1;
        name.title = "Name";
        name.width = 230;
        name.sortable = true;
        native::table_column type;
        type.id = 2;
        type.title = "Type";
        type.width = 125;
        type.sortable = true;
        native::table_column modified;
        modified.id = 3;
        modified.title = "Modified";
        modified.width = 150;
        modified.sortable = true;
        native::table_column size;
        size.id = 4;
        size.title = "Size";
        size.width = 100;
        size.alignment = native::table_alignment::end;
        size.sortable = true;
        return {name, type, modified, size};
    }

    // Supplies one million rows without retaining per-row objects.
    class million_row_model final : public native::table_model
    {
    public:
        std::size_t row_count() const override {
            return 1000000;
        }

        native::table_row_id row_id(std::size_t row) const override {
            return static_cast<native::table_row_id>(row + 1);
        }

        native::table_cell cell(
            std::size_t row,
            native::table_column_id column) const override {
            const std::string number = std::to_string(row + 1);
            if (column == 1)
                return {"Virtual row " + number, nullptr};
            if (column == 2)
                return {row % 2 == 0 ? "Generated" : "On demand",
                        nullptr};
            if (column == 3)
                return {"Never materialized", nullptr};
            if (column == 4)
                return {number + " B", nullptr};
            return {};
        }

        std::optional<std::size_t> find(
            const native::table_search &query) const override {
            constexpr std::string_view prefix = "Virtual row ";
            if (query.text.rfind(prefix, 0) != 0)
                return native::table_model::find(query);
            std::size_t number = 0;
            const char *first = query.text.data() + prefix.size();
            const char *last = query.text.data() + query.text.size();
            const auto parsed = std::from_chars(first, last, number);
            if (parsed.ec != std::errc() || parsed.ptr != last ||
                number == 0 || number > row_count()) {
                return std::nullopt;
            }
            return number - 1;
        }
    };

    // Highlights a few C++ lexical forms for the editor demonstration.
    class demo_code_lexer final : public native::code_lexer
    {
    public:
        std::string language_id() const override { return "cpp"; }

        std::vector<native::style_run> lex(
            std::string_view text,
            std::size_t dirty_start,
            std::size_t dirty_end) override {
            (void)dirty_start;
            (void)dirty_end;
            std::vector<native::style_run> result;
            std::size_t offset = 0;
            while (offset < text.size()) {
                if (offset + 1 < text.size() && text[offset] == '/' &&
                    text[offset + 1] == '/') {
                    const std::size_t end = text.find('\n', offset);
                    result.push_back({
                        {offset,
                         end == std::string_view::npos
                             ? text.size()
                             : end},
                        3});
                    offset = end == std::string_view::npos
                                 ? text.size()
                                 : end;
                    continue;
                }
                if (text[offset] == '"') {
                    std::size_t end = offset + 1;
                    while (end < text.size()) {
                        if (text[end] == '\\' && end + 1 < text.size()) {
                            end += 2;
                        } else if (text[end] == '"') {
                            ++end;
                            break;
                        } else {
                            ++end;
                        }
                    }
                    result.push_back({{offset, end}, 2});
                    offset = end;
                    continue;
                }
                const unsigned char value =
                    static_cast<unsigned char>(text[offset]);
                if (!std::isalpha(value) && text[offset] != '_') {
                    ++offset;
                    continue;
                }
                const std::size_t begin = offset++;
                while (offset < text.size()) {
                    const unsigned char next =
                        static_cast<unsigned char>(text[offset]);
                    if (!std::isalnum(next) && text[offset] != '_')
                        break;
                    ++offset;
                }
                const std::string_view word =
                    text.substr(begin, offset - begin);
                if (word == "int" || word == "return" ||
                    word == "const" || word == "std") {
                    result.push_back({{begin, offset}, 1});
                }
            }
            return result;
        }
    };
} // namespace

namespace vision
{
    feature_inspector::feature_inspector(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Inspector",
                               150, 140, 390, 300)
        , _apply("Apply", 18, 70, 110, 30)
        , _enabled("Enabled", 18, 116, 150, 24)
        , _compact("Compact mode", 18, 150, 170, 24)
        , _sections({"Controls", "Images", "Fonts", "Clipboard"},
                    210, 70, 155, 112)
        , _status("Use these controls while the main window stays active.") {
        _enabled.set_checked(true);
        _compact.set_selected(true);
        _sections.set_selected_index(0);
        on_wnd_create.connect(this, &feature_inspector::on_create);
        on_wnd_paint.connect(this, &feature_inspector::on_paint);
        _apply.on_click.connect(this, &feature_inspector::on_apply);
        _enabled.on_change.connect(this, &feature_inspector::on_enabled);
        _compact.on_change.connect(this, &feature_inspector::on_compact);
        _sections.on_selection_change.connect(
            this, &feature_inspector::on_section);
    }

    bool feature_inspector::on_create() {
        for (native::wnd *control : {
                 static_cast<native::wnd *>(&_apply),
                 static_cast<native::wnd *>(&_enabled),
                 static_cast<native::wnd *>(&_compact),
                 static_cast<native::wnd *>(&_sections)}) {
            control->set_parent(this);
            control->create();
            control->show();
        }
        return true;
    }

    bool feature_inspector::on_paint(
        native::wnd_paint_event event) {
        event.g.set_font(native::font_t::stock(native::font_role::control))
            .set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text("This is an independent modeless window.",
                          native::point(18, 18));
        event.g.draw_text("The main window remains interactive.",
                          native::point(18, 42));
        event.g.draw_text(_status, native::point(18, 214));
        return true;
    }

    bool feature_inspector::on_apply() {
        _status = "Apply clicked; modeless input is responsive.";
        invalidate();
        return true;
    }

    bool feature_inspector::on_enabled(bool enabled) {
        _status = enabled ? "Inspector enabled." : "Inspector disabled.";
        invalidate();
        return true;
    }

    bool feature_inspector::on_compact(bool selected) {
        _status = selected ? "Compact mode selected."
                           : "Compact mode cleared.";
        invalidate();
        return true;
    }

    bool feature_inspector::on_section(int index) {
        if (index >= 0 &&
            index < static_cast<int>(_sections.get_items().size())) {
            _status = "Section: " +
                      _sections.get_items()[static_cast<std::size_t>(index)];
            invalidate();
        }
        return true;
    }

    feature_layout::feature_layout(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Layout Managers",
                               200, 170, 560, 420)
        , _toggle("Switch to absolute layout", 0, 0, 200, 28)
        , _sidebar("Sidebar", 0, 0, 120, 28)
        , _status("Status bar", 0, 0, 120, 28)
        , _cell_1("Cell 0,0", 0, 0, 90, 26)
        , _cell_2("Cell 0,1", 0, 0, 90, 26)
        , _cell_3("Cell 1,0", 0, 0, 90, 26)
        , _cell_4("Cell 1,1", 0, 0, 90, 26) {
        on_wnd_create.connect(this, &feature_layout::on_create);
        on_wnd_paint.connect(this, &feature_layout::on_paint);
        _toggle.on_click.connect(this, &feature_layout::on_toggle);
    }

    bool feature_layout::on_create() {
        // The controls carry no useful bounds yet. Parenting them
        // before a layout exists is deliberate: the layout installed
        // below adopts the children the window already has, and the
        // cells named here survive that adoption.
        for (native::button *control : {&_toggle,
                                        &_sidebar,
                                        &_status,
                                        &_cell_1,
                                        &_cell_2,
                                        &_cell_3,
                                        &_cell_4}) {
            control->set_parent(this);
            control->create();
            control->show();
        }

        apply_grid_layout();
        return true;
    }

    void feature_layout::apply_grid_layout() {
        auto grid = std::make_unique<native::grid_layout_manager>();

        // Three fixed rows and one weighted row. Only the weighted
        // row grows, so the toolbar and status bar keep their height
        // whatever the window does. The first row holds no control at
        // all; a track is just space, and this one is painted into.
        (*grid) << native::row(native::pixels(52))
                << native::row(native::pixels(40))
                << native::row(native::star())
                << native::row(native::pixels(40))
                << native::column(native::star(1.0f))
                << native::column(native::star(2.0f))
                << native::cell(_toggle, 1, 0, 1, 2, 6)
                << native::cell(_sidebar, 2, 0, 1, 1, 6)
                << native::cell(_status, 3, 0, 1, 2, 6);

        // A nested grid owns the content cell and subdivides it. Its
        // children belong to it alone; the grid above never places
        // them a second time.
        auto nested =
            std::make_unique<native::grid_layout_manager>(2, 2);
        nested->add(_cell_1, 0, 0, 1, 1, 6)
            .add(_cell_2, 0, 1, 1, 1, 6)
            .add(_cell_3, 1, 0, 1, 1, 6)
            .add(_cell_4, 1, 1, 1, 1, 6);
        (*grid) << native::child_grid(std::move(nested), 2, 1, 1, 1, 3);

        set_layout(std::move(grid));
        _toggle.set_text("Switch to absolute layout");
    }

    void feature_layout::apply_absolute_layout() {
        // Absolute layout registers children without moving them, so
        // the explicit bounds below are what the window keeps, at any
        // size.
        auto layout =
            std::make_unique<native::absolute_layout_manager>();
        (*layout) << _toggle << _sidebar << _status
                  << _cell_1 << _cell_2 << _cell_3 << _cell_4;
        set_layout(std::move(layout));

        _toggle.set_bounds(native::rect(16, 58, 200, 28));
        _sidebar.set_bounds(native::rect(16, 98, 150, 180));
        _cell_1.set_bounds(native::rect(180, 98, 130, 84));
        _cell_2.set_bounds(native::rect(320, 98, 130, 84));
        _cell_3.set_bounds(native::rect(180, 194, 130, 84));
        _cell_4.set_bounds(native::rect(320, 194, 130, 84));
        _status.set_bounds(native::rect(16, 292, 434, 28));
        _toggle.set_text("Switch to grid layout");
    }

    bool feature_layout::on_toggle() {
        _using_grid = !_using_grid;
        if (_using_grid)
            apply_grid_layout();
        else
            apply_absolute_layout();
        invalidate();
        return true;
    }

    bool feature_layout::on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            _using_grid
                ? "Grid layout: pixel rows stay fixed, the star row "
                  "grows."
                : "Absolute layout: every control keeps its explicit "
                  "bounds.",
            native::point(16, 14));
        event.g.draw_text(
            _using_grid
                ? "Columns share the width 1:2. Resize to watch it "
                  "reflow."
                : "Resize the window: nothing moves until a grid is "
                  "installed.",
            native::point(16, 32));
        return true;
    }

    feature_collections::feature_collections(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Collection Controls",
                               230, 130, 760, 500)
        , _shapes(make_items("Shape", 20, 17), 0, 0, 330, 300)
        , _colors(make_items("Color", 16, 79), 0, 0, 330, 300)
        , _backgrounds(
              make_items("Background", 14, 137), 0, 0, 330, 300)
        , _libraries(20, 52, 330, 390)
        , _tree(make_tree_items(), 370, 52, 370, 390)
        , _three_dimensional("3-D outline", 600, 12, 140, 28)
        , _status("Select, expand, or double-click an item.") {
        _libraries.set_mode(native::accordion_mode::single);
        _libraries.add_item("Shapes", _shapes);
        _libraries.add_item("Colors", _colors);
        _libraries.add_item("Backgrounds", _backgrounds);
        on_wnd_create.connect(this, &feature_collections::on_create);
        on_wnd_paint.connect(this, &feature_collections::on_paint);
        _libraries.on_expanded_change.connect(
            this, &feature_collections::on_expanded);
        for (native::icon_view *view : {
                 &_shapes, &_colors, &_backgrounds}) {
            view->on_selection_change.connect(
                this, &feature_collections::on_selected);
            view->on_item_activate.connect(
                this, &feature_collections::on_activated);
        }
        _tree.on_selection_change.connect(
            this, &feature_collections::on_tree_selected);
        _tree.on_expanded_change.connect(
            this, &feature_collections::on_tree_expanded);
        _tree.on_item_activate.connect(
            this, &feature_collections::on_tree_activated);
        _three_dimensional.on_change.connect(
            this, &feature_collections::on_tree_presentation);
    }

    bool feature_collections::on_create() {
        _libraries.set_parent(this);
        _libraries.create();
        _libraries.show();
        _tree.set_parent(this);
        _tree.create();
        _tree.show();
        _three_dimensional.set_parent(this);
        _three_dimensional.create();
        _three_dimensional.show();
        return true;
    }

    bool feature_collections::on_paint(
        native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "Libraries: accordion/icon view                 "
            "Classic tree view",
            native::point(20, 16));
        event.g.draw_text(_status, native::point(20, 458));
        return true;
    }

    bool feature_collections::on_expanded(int index) {
        static constexpr const char *names[] = {
            "Shapes", "Colors", "Backgrounds"};
        _status = index >= 0 && index < 3
                      ? std::string("Expanded ") + names[index] + "."
                      : "Collapsed the active library.";
        invalidate();
        return true;
    }

    bool feature_collections::on_selected(int index) {
        _status = "Selected icon index " + std::to_string(index) + ".";
        invalidate();
        return true;
    }

    bool feature_collections::on_activated(int index) {
        _status = "Activated icon index " + std::to_string(index) + ".";
        invalidate();
        return true;
    }

    bool feature_collections::on_tree_selected(
        native::tree_item_id id) {
        _status = "Selected tree item " + std::to_string(id) + ".";
        invalidate();
        return true;
    }

    bool feature_collections::on_tree_expanded(
        native::tree_item_id id,
        bool expanded) {
        _status = std::string(expanded ? "Expanded" : "Collapsed") +
                  " tree item " + std::to_string(id) + ".";
        invalidate();
        return true;
    }

    bool feature_collections::on_tree_activated(
        native::tree_item_id id) {
        _status = "Activated tree item " + std::to_string(id) + ".";
        invalidate();
        return true;
    }

    bool feature_collections::on_tree_presentation(
        bool three_dimensional) {
        _tree.set_presentation(
            three_dimensional
                ? native::tree_view_presentation::three_dimensional
                : native::tree_view_presentation::native);
        _status = three_dimensional
                      ? "Using the optional 3-D Motif outline."
                      : "Using the native CDE InfoLib outline.";
        invalidate();
        return true;
    }

    feature_tables::feature_tables(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Table View",
                               250, 100, 800, 650)
        , _million_model(std::make_unique<million_row_model>())
        , _table(20, 72, 760, 300)
        , _million_table(20, 430, 760, 150)
        , _alternating("Alternating rows", 20, 18, 140, 24)
        , _grid("Grid lines", 170, 18, 110, 24)
        , _multiple("Multiple selection", 290, 18, 160, 24)
        , _search("Item 78", native::text_edit_mode::single_line,
                  460, 16, 180, 28)
        , _find("Find/reveal", 650, 16, 120, 28)
        , _scroll("Scroll to row 900,001", 20, 594, 190, 28)
        , _status("Both tables request cells only as they are needed.") {
        for (int index = 0; index < 4; ++index)
            _images.push_back(make_table_icon(index));
        std::vector<native::table_store_row> rows;
        rows.reserve(120);
        for (std::uint64_t index = 1; index <= 120; ++index) {
            rows.push_back(
                {index,
                 {{1,
                   {"Item " + std::to_string(index),
                    _images[static_cast<std::size_t>(index % 4)]
                        .get()}},
                  {2,
                   {index <= 60 ? "Document" : "Image", nullptr}},
                  {3,
                   {index % 3 == 0 ? "Today" : "Yesterday",
                    nullptr}},
                  {4,
                   {std::to_string(index * 7) + " KB", nullptr}}}});
        }
        _store.set_rows(std::move(rows));
        _store.set_groups(
            {{10, "Documents (60)", 0, 60, true, true},
             {20, "Images (60)", 60, 60, true, true}});

        _alternating.set_checked(true);
        _multiple.set_checked(true);
        _table.set_columns(make_table_columns())
            .set_model(&_store)
            .set_data_mode(native::table_data_mode::materialized)
            .set_selection_mode(native::table_selection_mode::multiple)
            .set_alternating_rows(true)
            .set_icon_size(std::optional<native::size>(
                native::size(18, 18)));
        _million_table.set_columns(make_table_columns())
            .set_model(_million_model.get())
            .set_data_mode(native::table_data_mode::virtualized)
            .set_selection_mode(native::table_selection_mode::multiple)
            .set_alternating_rows(true);

        on_wnd_create.connect(this, &feature_tables::on_create);
        on_wnd_paint.connect(this, &feature_tables::on_paint);
        on_wnd_resize.connect(this, &feature_tables::on_resize);
        _alternating.on_change.connect(
            this, &feature_tables::on_alternating);
        _grid.on_change.connect(this, &feature_tables::on_grid);
        _multiple.on_change.connect(this, &feature_tables::on_multiple);
        _find.on_click.connect(this, &feature_tables::on_find);
        _scroll.on_click.connect(this, &feature_tables::on_scroll);
        _table.on_selection_change.connect(
            this, &feature_tables::on_selection);
        _million_table.on_selection_change.connect(
            this, &feature_tables::on_selection);
        _table.on_sort_request.connect(this, &feature_tables::on_sort);
        _million_table.on_sort_request.connect(
            this, &feature_tables::on_sort);
        _table.on_group_expand.connect(
            this, &feature_tables::on_group);
    }

    bool feature_tables::on_create() {
        layout_controls(get_dimensions());
        for (native::wnd *control : {
                 static_cast<native::wnd *>(&_alternating),
                 static_cast<native::wnd *>(&_grid),
                 static_cast<native::wnd *>(&_multiple),
                 static_cast<native::wnd *>(&_search),
                 static_cast<native::wnd *>(&_find),
                 static_cast<native::wnd *>(&_table),
                 static_cast<native::wnd *>(&_million_table),
                 static_cast<native::wnd *>(&_scroll)}) {
            control->set_parent(this);
            control->create();
            control->show();
        }
        return true;
    }

    bool feature_tables::on_resize(native::size dimensions) {
        layout_controls(dimensions);
        invalidate();
        return true;
    }

    void feature_tables::layout_controls(native::size dimensions) {
        constexpr int side_margin = 20;
        constexpr int table_top = 72;
        constexpr int between_tables = 58;
        constexpr int bottom_reserve = 70;
        constexpr int find_width = 120;
        constexpr int search_x = 460;

        const int client_width = static_cast<int>(dimensions.w);
        const int client_height = static_cast<int>(dimensions.h);
        const int table_width = std::max(
            80, client_width - side_margin * 2);
        const int table_pool = std::max(
            2,
            client_height - table_top - between_tables - bottom_reserve);
        const int first_height = std::max(1, table_pool * 2 / 3);
        const int second_height = std::max(1, table_pool - first_height);
        const int second_top = table_top + first_height + between_tables;
        const int bottom_top = second_top + second_height + 14;

        const int find_x = std::max(search_x + 90,
                                    client_width - side_margin - find_width);
        const int search_width = std::max(80, find_x - search_x - 10);
        _search.set_bounds(native::rect(
            search_x, 16, search_width, 28));
        _find.set_bounds(native::rect(
            find_x, 16, find_width, 28));
        _table.set_bounds(native::rect(
            side_margin, table_top, table_width, first_height));
        _million_table.set_bounds(native::rect(
            side_margin, second_top, table_width, second_height));
        _scroll.set_bounds(native::rect(
            side_margin, bottom_top, 190, 28));
    }

    bool feature_tables::on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "120 materialized rows, four columns, icons, and two groups",
            native::point(20, _table.get_bounds().p.y - 22));
        event.g.draw_text(
            "Virtual model: 1,000,000 generated rows",
            native::point(20, _million_table.get_bounds().p.y - 28));
        event.g.draw_text(
            _status,
            native::point(226, _scroll.get_bounds().p.y + 6));
        return true;
    }

    bool feature_tables::on_alternating(bool enabled) {
        _table.set_alternating_rows(enabled);
        _million_table.set_alternating_rows(enabled);
        _status = enabled ? "Alternating rows enabled."
                          : "Alternating rows disabled.";
        invalidate();
        return true;
    }

    bool feature_tables::on_grid(bool enabled) {
        const native::table_grid_lines lines = enabled
            ? native::table_grid_lines::both
            : native::table_grid_lines::none;
        _table.set_grid_lines(lines);
        _million_table.set_grid_lines(lines);
        _status = enabled ? "Horizontal and vertical grid lines enabled."
                          : "Grid lines disabled.";
        invalidate();
        return true;
    }

    bool feature_tables::on_multiple(bool enabled) {
        const native::table_selection_mode mode = enabled
            ? native::table_selection_mode::multiple
            : native::table_selection_mode::single;
        _table.set_selection_mode(mode);
        _million_table.set_selection_mode(mode);
        _status = enabled ? "Multiple selection enabled."
                          : "Single selection enabled.";
        invalidate();
        return true;
    }

    bool feature_tables::on_find() {
        native::table_search query;
        query.text = _search.get_text();
        query.match = native::table_search_match::substring;
        _status = _table.find_and_reveal(query)
            ? "Found and revealed: " + query.text
            : "No table row contains: " + query.text;
        invalidate();
        return true;
    }

    bool feature_tables::on_scroll() {
        native::table_search query;
        query.text = "Virtual row 900001";
        query.match = native::table_search_match::exact;
        query.columns = {1};
        _status = _million_table.find_and_reveal(query)
            ? "Virtual table jumped to stable row 900,001."
            : "Virtual row 900,001 was not found.";
        invalidate();
        return true;
    }

    bool feature_tables::on_selection(
        const std::vector<native::table_row_id> &rows) {
        _status = "User selected " + std::to_string(rows.size()) +
                  " stable row ID(s).";
        invalidate();
        return true;
    }

    bool feature_tables::on_sort(native::table_sort sort) {
        _status = "Sort requested for column " +
                  std::to_string(sort.column) +
                  (sort.direction == native::sort_direction::ascending
                       ? " ascending."
                       : " descending.");
        invalidate();
        return true;
    }

    bool feature_tables::on_group(native::table_group_id group,
                                  bool expanded) {
        _status = "Group " + std::to_string(group) +
                  (expanded ? " expanded." : " collapsed.");
        invalidate();
        return true;
    }

    feature_code_editor::feature_code_editor(native::app_wnd &owner)
        : native::modeless_wnd(owner,
                               "Vision Code Editor",
                               270,
                               110,
                               760,
                               550)
        , _lexer(std::make_unique<demo_code_lexer>())
        , _editor("#include <iostream>\n\n"
                  "int main() {\n"
                  "    const char *message = \"Hello, Native!\";\n"
                  "    // Click the gutter to toggle a breakpoint.\n"
                  "    return message[0] == 'H' ? 0 : 1;\n"
                  "}\n",
                  20,
                  54,
                  720,
                  400)
        , _show_completion("Show completion", 20, 468, 150, 28)
        , _status("Line numbers, styles, diagnostics, and marks are "
                  "portable overlays.") {
        native::code_theme colors;
        colors.styles.resize(4);
        colors.styles[0].foreground = native::rgba(20, 20, 20, 255);
        colors.styles[1].foreground = native::rgba(35, 70, 180, 255);
        colors.styles[2].foreground = native::rgba(35, 125, 65, 255);
        colors.styles[3].foreground = native::rgba(110, 110, 110, 255);
        _editor.set_language("cpp")
            .set_code_theme(std::move(colors))
            .set_lexer(_lexer.get());
        _editor.add_marker(
            {2, native::marker_kind::breakpoint});
        _editor.add_marker(
            {3, native::marker_kind::current_line});
        const std::size_t message = _editor.get_text().find("message");
        if (message != std::string::npos) {
            _editor.set_diagnostics({
                {{message, message + 7},
                 native::diagnostic_severity::info,
                 "Demonstration diagnostic"}});
        }
        on_wnd_create.connect(this, &feature_code_editor::on_create);
        on_wnd_paint.connect(this, &feature_code_editor::on_paint);
        _editor.on_gutter_click.connect(
            this, &feature_code_editor::on_gutter);
        _editor.on_text_change.connect(
            this, &feature_code_editor::on_text_change);
        _editor.on_complete.connect(
            this, &feature_code_editor::on_completion);
        _show_completion.on_click.connect(
            this, &feature_code_editor::on_show_completion);
    }

    bool feature_code_editor::on_create() {
        for (native::wnd *control : {
                 static_cast<native::wnd *>(&_editor),
                 static_cast<native::wnd *>(&_show_completion)}) {
            control->set_parent(this);
            control->create();
            control->show();
        }
        return true;
    }

    bool feature_code_editor::on_paint(
        native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "Edit UTF-8 source; arrows, selection, clipboard, and undo "
            "are active.",
            native::point(20, 18));
        event.g.draw_text(_status, native::point(186, 474));
        event.g.draw_text(
            "The optional session sidecar remains application-owned.",
            native::point(20, 512));
        return true;
    }

    bool feature_code_editor::on_gutter(int line) {
        bool found = false;
        for (const native::line_marker &marker : _editor.markers()) {
            if (marker.line == line &&
                marker.kind == native::marker_kind::breakpoint) {
                found = true;
                break;
            }
        }
        if (found) {
            _editor.remove_marker(line, native::marker_kind::breakpoint);
            _status = "Removed breakpoint from line " +
                      std::to_string(line + 1) + ".";
        } else {
            _editor.add_marker(
                {line, native::marker_kind::breakpoint});
            _status = "Added breakpoint to line " +
                      std::to_string(line + 1) + ".";
        }
        invalidate();
        return true;
    }

    bool feature_code_editor::on_text_change() {
        _status = "Document changed; " +
                  std::to_string(_editor.line_count()) +
                  " logical line(s).";
        invalidate();
        return true;
    }

    bool feature_code_editor::on_show_completion() {
        _editor.show_completion({
            {"std::cout", "std::cout", "standard output stream"},
            {"std::string", "std::string", "UTF-8 byte string"},
            {"return", "return", "return statement"}});
        _status = "Use Up/Down, Enter, or Escape in the editor.";
        invalidate();
        return true;
    }

    bool feature_code_editor::on_completion(
        native::completion_item item) {
        _editor.insert(_editor.get_caret_offset(), item.insert);
        _status = "Accepted completion: " + item.label + ".";
        invalidate();
        return true;
    }

    feature_splitter::feature_splitter(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Split View",
                               150, 80, 760, 500)
        , _left({"Project", "Sources", "Resources", "Tests"},
                0, 0, 240, 420)
        , _right({"Editor", "Properties", "Output", "Preview"},
                 0, 0, 440, 420)
        , _split(_left, _right,
                 native::split_orientation::horizontal,
                 native::rect(20, 20, 720, 460)) {
        _left.set_selected_index(0);
        _right.set_selected_index(0);
        _split.set_ratio(0.35f).set_minimums(140, 220);
        on_wnd_create.connect(this, &feature_splitter::on_create);
        on_wnd_resize.connect(this, &feature_splitter::on_resize);
        _split.on_ratio_change.connect(
            this, &feature_splitter::on_ratio_change);
    }

    bool feature_splitter::on_create() {
        _split.set_parent(this);
        _split.create();
        _split.show();
        return true;
    }

    bool feature_splitter::on_ratio_change(float ratio) {
        set_title("Vision Split View - " +
                  std::to_string(static_cast<int>(ratio*100.0f)) +
                  "% / " +
                  std::to_string(static_cast<int>((1.0f-ratio)*100.0f)) +
                  "%");
        return true;
    }

    bool feature_splitter::on_resize(native::size dimensions) {
        _split.set_bounds(native::rect(
            20,
            20,
            static_cast<native::dim>(std::max(
                0, static_cast<int>(dimensions.w) - 40)),
            static_cast<native::dim>(std::max(
                0, static_cast<int>(dimensions.h) - 40))));
        return true;
    }

    feature_input_chrome::feature_input_chrome(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Input and Window Chrome",
                               120, 60, 790, 730)
        , _selection_combo({"CDE", "OpenLook", "Window Maker", "Haiku"},
                           native::combo_box_style::drop_down_list,
                           60, 72, 230, 24)
        , _editable_combo({"10 mm", "25 mm", "50 mm", "100 mm"},
                          native::combo_box_style::editable,
                          60, 116, 230, 24)
        , _list_box({"One", "Two", "Three", "Four", "Five"},
                    60, 174, 230, 146)
        , _tab_general({"Native page chrome", "Borrowed page", "Silent API selection"},
                       0, 0, 220, 100)
        , _tab_advanced({"User change signal", "Disabled tab support",
                        "Selected page lifecycle"},
                        0, 0, 220, 100)
        , _tabs(400, 174, 300, 118)
        , _bottom_tab_general(
              {"Labels below content", "Straight edge joins above"},
              0, 0, 220, 72)
        , _bottom_tab_advanced(
              {"Rounded edge faces down", "Selection remains portable"},
              0, 0, 220, 72)
        , _bottom_tabs(400, 332, 300, 118)
        , _left_tab_general(
              {"Counter-clockwise labels", "Content follows the strip"},
              0, 0, 250, 100)
        , _left_tab_advanced(
              {"Runtime placement", "Borrowed page preserved"},
              0, 0, 250, 100)
        , _left_tabs(60, 516, 300, 142)
        , _right_tab_general(
              {"Clockwise labels", "Content precedes the strip"},
              0, 0, 250, 100)
        , _right_tab_advanced(
              {"Directional free edge", "Silent selection state"},
              0, 0, 250, 100)
        , _right_tabs(400, 516, 300, 142)
        , _choose_folder("Select folder...", 330, 74, 150, 30)
        , _show_message("Three-button message...", 330, 118, 190, 30)
        , _directory(*this, "Select a Workspace Folder")
        , _horizontal_ruler(*this, native::window_edge::top, 24)
        , _vertical_ruler(*this, native::window_edge::left, 24)
        , _status_bar(*this, 17) {
        _selection_combo.set_selected_index(0);
        _editable_combo.set_text("25 mm");
        _list_box.set_selected_index(1);
        _tabs.add_item("General", _tab_general);
        _tabs.add_item("Advanced", _tab_advanced);
        _bottom_tabs.set_tab_placement(native::tab_placement::bottom);
        _bottom_tabs.add_item("Bottom", _bottom_tab_general);
        _bottom_tabs.add_item("Details", _bottom_tab_advanced);
        _left_tabs.set_tab_placement(native::tab_placement::left);
        _left_tabs.add_item("Left", _left_tab_general);
        _left_tabs.add_item("Details", _left_tab_advanced);
        _right_tabs.set_tab_placement(native::tab_placement::right);
        _right_tabs.add_item("Right", _right_tab_general);
        _right_tabs.add_item("Details", _right_tab_advanced);
        _horizontal_ruler.set_minor_tick(10).set_major_tick(50)
            .set_track_mouse(true).set_edge_visible(true);
        _vertical_ruler.set_minor_tick(10).set_major_tick(50)
            .set_track_mouse(true).set_edge_visible(true);
        _status_bar.set_parts({
            {"Ready", 0}, {"minor 10 / major 50", 180}});

        on_wnd_create.connect(this, &feature_input_chrome::on_create);
        on_wnd_paint.connect(this, &feature_input_chrome::on_paint);
        _selection_combo.on_selection_change.connect(
            this, &feature_input_chrome::on_combo_selection);
        _editable_combo.on_text_change.connect(
            this, &feature_input_chrome::on_combo_text);
        _list_box.on_selection_change.connect(
            this, &feature_input_chrome::on_list_selection);
        _tabs.on_selection_change.connect(
            this, &feature_input_chrome::on_tab_selection);
        _bottom_tabs.on_selection_change.connect(
            this, &feature_input_chrome::on_bottom_tab_selection);
        _left_tabs.on_selection_change.connect(
            this, &feature_input_chrome::on_side_tab_selection);
        _right_tabs.on_selection_change.connect(
            this, &feature_input_chrome::on_side_tab_selection);
        _choose_folder.on_click.connect(
            this, &feature_input_chrome::on_choose_folder);
        _show_message.on_click.connect(
            this, &feature_input_chrome::on_show_message);
        _directory.on_modal_close.connect(
            this, &feature_input_chrome::on_folder_selected);
        _horizontal_ruler.on_tracking.connect(
            this, &feature_input_chrome::on_ruler_tracking);
        _vertical_ruler.on_tracking.connect(
            this, &feature_input_chrome::on_ruler_tracking);
    }

    bool feature_input_chrome::on_create() {
        for (native::wnd *control : {
                 static_cast<native::wnd *>(&_selection_combo),
                 static_cast<native::wnd *>(&_editable_combo),
                 static_cast<native::wnd *>(&_list_box),
                 static_cast<native::wnd *>(&_tabs),
                 static_cast<native::wnd *>(&_bottom_tabs),
                 static_cast<native::wnd *>(&_left_tabs),
                 static_cast<native::wnd *>(&_right_tabs),
                 static_cast<native::wnd *>(&_choose_folder),
                 static_cast<native::wnd *>(&_show_message)}) {
            control->set_parent(this);
            control->create();
            control->show();
        }
        return true;
    }

    bool feature_input_chrome::on_paint(native::wnd_paint_event event) {
        event.g.set_font(native::font_t::stock(native::font_role::control))
            .set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text("Selection-only combo box",
                          native::point(60, 52));
        event.g.draw_text("Editable combo box",
                          native::point(60, 98));
        event.g.draw_text("Native list_box",
                          native::point(60, 152));
        event.g.draw_text("Top native tab_view",
                          native::point(400, 152));
        event.g.draw_text("Bottom tab_view (downward-facing)",
                          native::point(400, 310));
        event.g.draw_text("Left tab_view (rotated labels)",
                          native::point(60, 494));
        event.g.draw_text("Right tab_view (rotated labels)",
                          native::point(400, 494));
        event.g.draw_text(
            "The rulers and status bar occupy non-client edge strips.",
            native::point(60, 674));
        event.g.draw_text(
            "Move the pointer over the client to update ruler tracking.",
            native::point(400, 674));
        return true;
    }

    bool feature_input_chrome::on_combo_selection(int index) {
        const auto &items = _selection_combo.get_items();
        if (index >= 0 && index < static_cast<int>(items.size()))
            _status_bar.set_parts({{"Combo: " + items[index], 0},
                                   {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_combo_text(std::string text) {
        _status_bar.set_parts({{"Typed: " + text, 0},
                               {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_list_selection(int index) {
        _status_bar.set_parts({
            {"List row " + std::to_string(index+1), 0},
            {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_tab_selection(int index) {
        _status_bar.set_parts({
            {"Tab: " + _tabs.get_item(
                static_cast<std::size_t>(index)).get_title(), 0},
            {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_bottom_tab_selection(int index) {
        _status_bar.set_parts({
            {"Bottom tab: " + _bottom_tabs.get_item(
                static_cast<std::size_t>(index)).get_title(), 0},
            {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_side_tab_selection(int) {
        _status_bar.set_parts({
            {"Side tabs use rotated labels", 0},
            {"left / right", 180}});
        return true;
    }

    bool feature_input_chrome::on_choose_folder() {
        _directory.create();
        _directory.show();
        return true;
    }

    bool feature_input_chrome::on_folder_selected(
        native::dialog_result result) {
        _status_bar.set_parts({
            {result == native::dialog_result::accepted
                 ? "Folder: " + _directory.get_path().string()
                 : "Folder selection cancelled",
             0},
            {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_show_message() {
        const native::message_box_result result = native::message_box::show(
            *this,
            "This is a standard three-button message dialog.",
            "Native Message Box",
            native::message_box_buttons::yes_no_cancel,
            native::message_box_icon::question);
        const char *name = result == native::message_box_result::yes ? "Yes"
            : result == native::message_box_result::no ? "No"
            : result == native::message_box_result::cancel ? "Cancel"
            : result == native::message_box_result::ok ? "OK" : "None";
        _status_bar.set_parts({{std::string("Message result: ")+name, 0},
                               {"minor 10 / major 50", 180}});
        return true;
    }

    bool feature_input_chrome::on_ruler_tracking(double value) {
        _status_bar.set_parts({
            {"Ruler: " + std::to_string(static_cast<int>(value)), 0},
            {"minor 10 / major 50", 180}});
        return true;
    }

    feature_dialog::feature_dialog(native::app_wnd &owner)
        : native::modal_wnd(owner, "Vision Modal Dialog",
                            190, 180, 380, 210)
        , _accept("Accept", 92, 148, 88, 30)
        , _cancel("Cancel", 196, 148, 88, 30) {
        on_wnd_create.connect(this, &feature_dialog::on_create);
        on_wnd_paint.connect(this, &feature_dialog::on_paint);
        _accept.on_click.connect(this, &feature_dialog::on_accept);
        _cancel.on_click.connect(this, &feature_dialog::on_cancel);
    }

    bool feature_dialog::on_create() {
        _accept.set_parent(this);
        _accept.create();
        _accept.show();
        _cancel.set_parent(this);
        _cancel.create();
        _cancel.show();
        return true;
    }

    bool feature_dialog::on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "This dialog owns focus and blocks its owner.",
                          native::point(24, 28));
        event.g.draw_text(
            "Closing it produces a portable dialog_result.",
                          native::point(24, 56));
        event.g.draw_text(
            "Paint and lifecycle dispatch continue normally.",
                          native::point(24, 84));
        return true;
    }

    bool feature_dialog::on_accept() {
        close(native::dialog_result::accepted);
        return true;
    }

    bool feature_dialog::on_cancel() {
        close(native::dialog_result::cancelled);
        return true;
    }
} // namespace vision

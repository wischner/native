//
// Exercises live collection-control and source-editor lifecycle through
// the selected backend. The posted check closes its own window, so
// toolkit sessions can run this executable unattended as a smoke test.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <native.h>

namespace
{
    int failure_count = 0;

    void expect(bool condition, const std::string &description) {
        if (condition)
            return;
        std::cerr << "FAILED: " << description << '\n';
        ++failure_count;
    }

    std::shared_ptr<const native::img> make_icon() {
        auto image = std::make_shared<native::img>(24, 24);
        image->get_gpx()
            .clear(native::rgba(0, 0, 0, 0))
            .set_ink(native::rgba(45, 110, 210, 180))
            .draw_ellipse(native::rect(2, 2, 20, 20), true);
        return image;
    }

    class runtime_virtual_model final : public native::table_model
    {
    public:
        std::size_t row_count() const override {
            return 1000000;
        }

        native::table_row_id row_id(std::size_t row) const override {
            ++id_requests;
            return static_cast<native::table_row_id>(row + 1);
        }

        native::table_cell cell(
            std::size_t row,
            native::table_column_id) const override {
            ++cell_requests;
            return {"Virtual " + std::to_string(row + 1), nullptr};
        }

        std::optional<std::size_t> find(
            const native::table_search &query) const override {
            ++find_requests;
            return query.text == "indexed"
                       ? std::optional<std::size_t>(900000)
                       : std::nullopt;
        }

        mutable std::size_t id_requests = 0;
        mutable std::size_t cell_requests = 0;
        mutable std::size_t find_requests = 0;
    };

    class collection_window final : public native::app_wnd
    {
    public:
        collection_window()
            : native::app_wnd("Collection runtime test",
                              native::rect(40, 40, 560, 680))
            , _image(make_icon())
            , _first(make_items("First"), 0, 0, 320, 180)
            , _second(make_items("Second"), 0, 0, 320, 180)
            , _command("Native body", 0, 0, 320, 80)
            , _sections(16, 16, 328, 250)
            , _list(make_list_items(), 360, 16, 176, 100)
            , _tree(make_tree_items(), 360, 16, 176, 250)
            , _table_store(make_table_rows())
            , _table(16, 280, 520, 210)
            , _code("int main() {\n    return 0;\n}\n",
                    16, 500, 520, 120)
            , _virtual_table(16, 630, 520, 34) {
            _first.set_selected_index(1);
            _sections.add_item("Shapes", _first);
            _sections.add_item("Colors", _second);
            _sections.add_item("Commands", _command);
            _sections.on_expanded_change.connect([this](int) {
                ++_expansion_events;
                return false;
            });
            _second.on_selection_change.connect([this](int) {
                ++_selection_events;
                return false;
            });
            _second.on_item_activate.connect([this](int) {
                ++_activation_events;
                return false;
            });
            _tree.on_selection_change.connect(
                [this](native::tree_item_id) {
                    ++_tree_selection_events;
                    return false;
                });
            _tree.on_expanded_change.connect(
                [this](native::tree_item_id, bool) {
                    ++_tree_expansion_events;
                    return false;
                });
            _tree.on_item_activate.connect(
                [this](native::tree_item_id) {
                    ++_tree_activation_events;
                    return false;
                });
            native::table_column name;
            name.id = 1;
            name.title = "Name";
            name.width = 180;
            name.sortable = true;
            native::table_column kind;
            kind.id = 2;
            kind.title = "Kind";
            kind.width = 120;
            native::table_column size;
            size.id = 3;
            size.title = "Size";
            size.width = 90;
            size.alignment = native::table_alignment::end;
            _table_store.set_groups(
                {{100, "Documents", 0, 3, true, true},
                 {200, "Images", 3, 3, true, true}});
            _table.set_columns({name, kind, size})
                .set_model(&_table_store)
                .set_data_mode(native::table_data_mode::materialized)
                .set_selection_mode(
                    native::table_selection_mode::multiple)
                .set_alternating_rows(true)
                .set_grid_lines(native::table_grid_lines::horizontal)
                .set_selected_rows({2});
            _virtual_table.set_columns({name})
                .set_model(&_virtual_model)
                .set_data_mode(native::table_data_mode::virtualized)
                .set_header_visible(false);
            _code.set_language("cpp").set_tab_width(4);
            _code.add_marker(
                {1, native::marker_kind::breakpoint});
            _table.on_selection_change.connect(
                [this](const std::vector<native::table_row_id> &) {
                    ++_table_selection_events;
                    return false;
                });
            _table.on_sort_request.connect(
                [this](native::table_sort) {
                    ++_sort_events;
                    return false;
                });
            _table.on_group_expand.connect(
                [this](native::table_group_id, bool) {
                    ++_group_events;
                    return false;
                });
            _code.on_text_change.connect([this] {
                ++_code_change_events;
                return false;
            });
            _code.on_gutter_click.connect([this](int line) {
                _gutter_line = line;
                return false;
            });
            _code.on_complete.connect(
                [this](native::completion_item item) {
                    if (item.insert == "return")
                        ++_completion_events;
                    return false;
                });
            on_wnd_create.connect(this, &collection_window::on_create);
        }

    private:
        std::shared_ptr<const native::img> _image;
        native::icon_view _first;
        native::icon_view _second;
        native::button _command;
        native::accordion _sections;
        native::list _list;
        native::tree_view _tree;
        native::table_store _table_store;
        runtime_virtual_model _virtual_model;
        native::table_view _table;
        native::code_edit _code;
        native::table_view _virtual_table;
        int _expansion_events = 0;
        int _selection_events = 0;
        int _activation_events = 0;
        int _tree_selection_events = 0;
        int _tree_expansion_events = 0;
        int _tree_activation_events = 0;
        int _table_selection_events = 0;
        int _sort_events = 0;
        int _group_events = 0;
        int _code_change_events = 0;
        int _completion_events = 0;
        int _gutter_line = -1;

        std::vector<native::icon_view_item> make_items(
            const std::string &prefix) {
            std::vector<native::icon_view_item> items;
            for (std::uint64_t id = 1; id <= 10; ++id) {
                items.push_back({prefix + " " + std::to_string(id),
                                 _image,
                                 id,
                                 true});
            }
            return items;
        }

        std::vector<native::table_store_row> make_table_rows() {
            std::vector<native::table_store_row> rows;
            for (std::uint64_t id = 1; id <= 6; ++id) {
                rows.push_back(
                    {id,
                     {{1,
                       {"Item " + std::to_string(id), _image.get()}},
                      {2,
                       {id <= 3 ? "Document" : "Image", nullptr}},
                      {3,
                       {std::to_string(id * 8) + " KB", nullptr}}}});
            }
            return rows;
        }

        std::vector<std::string> make_list_items() {
            std::vector<std::string> items;
            for (int index = 1; index <= 30; ++index)
                items.push_back("List item " + std::to_string(index));
            return items;
        }

        std::vector<native::tree_view_item> make_tree_items() {
            return {
                {"Root",
                 _image,
                 100,
                 {{"First", _image, 101},
                  {"Branch",
                   _image,
                   102,
                   {{"Leaf", _image, 103}},
                   false}},
                 true},
                {"Other", _image, 200}};
        }

        bool on_create() {
            _sections.set_parent(this);
            _sections.create();
            _sections.show();
            _list.set_parent(this);
            _list.create();
            _list.show();
            _tree.set_parent(this);
            _tree.create();
            _tree.show();
            _table.set_parent(this);
            _table.create();
            _table.show();
            _code.set_parent(this);
            _code.create();
            _code.show();
            _virtual_table.set_parent(this);
            _virtual_table.create();
            _virtual_table.show();
            native::app::post([this] { run_checks(); });
            return true;
        }

        void run_checks() {
            try {
                expect(_sections.get_created() && _first.get_created() &&
                           !_second.get_created(),
                       "accordion creates only its expanded body");
                expect(_tree.get_created() &&
                           _tree.get_visible_item_count() == 4 &&
                           _tree_selection_events == 0 &&
                           _tree_expansion_events == 0,
                       "tree applies its hierarchy without signals");
                expect(_list.get_created() &&
                           _list.get_items().size() == 30,
                       "overflowing native list creates and retains "
                       "its items");
                expect(_first.get_selected_index() == 1 &&
                           _selection_events == 0,
                       "pre-create icon state applies without a signal");
                expect(_table.get_created() &&
                           _table.get_selected_rows() ==
                               std::vector<native::table_row_id>{2} &&
                           _table_selection_events == 0,
                       "pre-create table model and selection apply "
                       "without a signal");
                expect(_virtual_table.get_created() &&
                           _virtual_table.get_display_row_count() ==
                               1000000,
                       "virtual backend creates without materializing "
                       "one million rows");
                expect(_code.get_created() &&
                           _code.line_count() == 4 &&
                           _code.markers().size() == 1,
                       "source editor applies pre-create document and "
                       "marker state");
                _code.go_to_offset(0);
                expect(_code.on_native_text_input("// ") &&
                           _code.get_text().rfind("// ", 0) == 0 &&
                           _code_change_events == 1,
                       "live source editor accepts backend text input");
                _code.show_completion(
                    {{"return", "return", "keyword"}});
                expect(_code.on_native_key(
                           native::code_edit_key::enter) &&
                           _completion_events == 1,
                       "live source editor accepts completion from "
                       "the keyboard");
                _code.on_mouse_click.emit(native::mouse_event(
                    native::mouse_button::left,
                    native::mouse_action::press,
                    native::point(2, 1)));
                expect(_gutter_line == 0,
                       "live source editor reports gutter clicks");
                const std::size_t ids_before =
                    _virtual_model.id_requests;
                native::table_search indexed;
                indexed.text = "indexed";
                indexed.match = native::table_search_match::exact;
                expect(_virtual_table.find_and_reveal(indexed) &&
                           _virtual_table.get_selected_rows() ==
                               std::vector<native::table_row_id>{900001} &&
                           _virtual_model.find_requests == 1 &&
                           _virtual_model.id_requests - ids_before < 1000,
                       "live indexed reveal avoids a million-row ID "
                       "scan");

                _table.on_native_selection({4, 5});
                _table.on_native_sort_request(1);
                _table.on_native_group_expand(200, false);
                expect(_table.get_selected_rows() ==
                           std::vector<native::table_row_id>{4, 5} &&
                           _table_selection_events == 1 &&
                           _sort_events == 1 && _group_events == 1 &&
                           !_table.get_group_expanded(200),
                       "table user selection, sorting, and grouping "
                       "each emit once");
                native::table_search query;
                query.text = "Item 5";
                query.match = native::table_search_match::substring;
                expect(_table.find_and_reveal(query) &&
                           _table.get_group_expanded(200) &&
                           _table.get_selected_rows() ==
                               std::vector<native::table_row_id>{5},
                       "live table search expands and reveals its row");

                _sections.on_native_toggle(1);
                expect(!_first.get_created() && _second.get_created() &&
                           _sections.get_expanded_index() == 1 &&
                           _expansion_events == 1,
                       "user expansion swaps bodies and emits once");

                _second.on_native_selection(2);
                _second.on_native_activate(2);
                expect(_second.get_selected_index() == 2 &&
                           _selection_events == 1 &&
                           _activation_events == 1,
                       "icon selection and activation each emit once");

                _tree.on_native_selection(102);
                _tree.on_native_expansion(102, true);
                _tree.on_native_activate(103);
                expect(_tree.get_selected_item() == 102 &&
                           _tree.get_visible_item_count() == 5 &&
                           _tree_selection_events == 1 &&
                           _tree_expansion_events == 1 &&
                           _tree_activation_events == 1,
                       "tree selection, disclosure, and activation "
                       "each emit once");
                // Exercise repeated reconstruction: Motif may retain icon
                // and disclosure pixmaps until its relayout work completes.
                for (int cycle = 0; cycle < 4; ++cycle) {
                    _tree.set_presentation(
                        native::tree_view_presentation::three_dimensional);
                    _tree.set_presentation(
                        native::tree_view_presentation::native);
                }
                expect(_tree.get_created() &&
                           _tree.get_selected_item() == 102 &&
                           _tree.get_expanded(102) &&
                           _tree_selection_events == 1 &&
                           _tree_expansion_events == 1,
                       "live tree presentation switching preserves "
                       "state without action signals");

                _sections.on_native_toggle(2);
                expect(!_second.get_created() &&
                           _command.get_created() &&
                           _sections.get_expanded_index() == 2 &&
                           _expansion_events == 2,
                       "accordion accepts an ordinary native body");
                _sections.on_native_toggle(1);
                expect(!_command.get_created() &&
                           _second.get_created() &&
                           _expansion_events == 3,
                       "native and custom bodies swap cleanly");

                _sections.set_dimensions({300, 220});
                const native::rect body =
                    _sections.get_content_bounds(1);
                expect(body.p.x == 1 && body.d.w == 298 &&
                           body.y2() <= 219,
                       "live accordion resize keeps body content inside "
                       "its outer border");
            } catch (const std::exception &error) {
                std::cerr << "FAILED: unexpected exception: "
                          << error.what() << '\n';
                ++failure_count;
            }
            destroy();
        }
    };
} // namespace

int program(int, char **) {
    collection_window window;
    const int result = native::app::run(window);
    return result == 0 && failure_count == 0 ? 0 : 1;
}

//
// Tests the backend-neutral table model, store, grouped visible-row
// mapping, searching, selection, scrolling, and virtual behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <native.h>

namespace
{
    int failures = 0;

    void expect(bool condition, const std::string &description) {
        if (condition)
            return;
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }

    native::table_store sample_store() {
        std::vector<native::table_store_row> rows;
        rows.push_back({10,
                        {{1, {"Primary Server", nullptr}},
                         {2, {"Online", nullptr}}}});
        rows.push_back({20,
                        {{1, {"Résumé", nullptr}},
                         {2, {"Offline", nullptr}}}});
        rows.push_back({30,
                        {{1, {"Archive", nullptr}},
                         {2, {"Online", nullptr}}}});
        return native::table_store(std::move(rows));
    }

    std::vector<native::table_column> sample_columns() {
        native::table_column name;
        name.id = 1;
        name.title = "Name";
        name.sortable = true;
        native::table_column state;
        state.id = 2;
        state.title = "State";
        return {name, state};
    }

    void test_store_and_notifications() {
        native::table_store store = sample_store();
        expect(store.row_count() == 3 && store.row_id(1) == 20,
               "table_store retains stable row IDs");
        expect(store.cell(0, 1).text == "Primary Server" &&
                   store.cell(0, 99).text.empty(),
               "table_store resolves sparse semantic cells");

        int notifications = 0;
        native::table_model_change last;
        store.on_change.connect(
            [&](const native::table_model_change &change) {
                ++notifications;
                last = change;
                return false;
            });
        store.add_row({40, {{1, {"Backup", nullptr}}}});
        expect(notifications == 1 &&
                   last.kind ==
                       native::table_model_change_kind::rows_inserted &&
                   last.first == 3 && last.count == 1,
               "table_store emits a precise insertion notification");
        store.set_row(3, {41, {{1, {"Changed", nullptr}}}});
        expect(notifications == 2 &&
                   last.kind ==
                       native::table_model_change_kind::rows_changed,
               "table_store emits a row-change notification");
        store.remove_row(3);
        expect(notifications == 3 &&
                   last.kind ==
                       native::table_model_change_kind::rows_removed,
               "table_store emits a row-removal notification");

        bool rejected = false;
        try {
            store.add_row({20, {{1, {"Duplicate", nullptr}}}});
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        expect(rejected, "table_store rejects duplicate stable IDs");
    }

    void test_search() {
        native::table_store store = sample_store();
        native::table_view view;
        view.set_columns(sample_columns()).set_model(&store);

        native::table_search query;
        query.text = "Primary Server";
        query.match = native::table_search_match::exact;
        expect(view.find(query) == 10,
               "exact table search finds the complete display text");
        query.text = "pri";
        query.match = native::table_search_match::prefix;
        expect(view.find(query) == 10,
               "prefix search is case insensitive by default");
        query.text = "mary ser";
        query.match = native::table_search_match::substring;
        expect(view.find(query) == 10,
               "substring search finds an interior string part");
        query.text = "RÉSUMÉ";
        query.match = native::table_search_match::exact;
        expect(view.find(query) == 20,
               "case-insensitive search folds common non-ASCII text");
        query.case_mode = native::table_search_case::sensitive;
        expect(!view.find(query), "case-sensitive search preserves case");
        query = {};
        query.text = "Online";
        query.columns = {2};
        query.start_row = 1;
        query.wrap = false;
        expect(view.find(query) == 30,
               "column-limited search honors its start row");
        query.start_row = 3;
        query.wrap = true;
        expect(view.find(query) == 10,
               "table search wraps only when requested");

        int type_events = 0;
        view.on_selection_change.connect(
            [&](const std::vector<native::table_row_id> &rows) {
                type_events += rows ==
                               std::vector<native::table_row_id>{30};
                return false;
            });
        view.on_native_type_text("Arc");
        expect(view.get_selected_rows() ==
                   std::vector<native::table_row_id>{30} &&
                   type_events == 1,
               "incremental type search selects and emits once");
        view.set_type_search_enabled(false);
        view.on_native_type_text("Pri");
        expect(view.get_selected_rows() ==
                   std::vector<native::table_row_id>{30},
               "disabled incremental type search preserves selection");
        view.set_type_search_enabled(true);
        view.on_native_type_text("Off");
        expect(view.get_selected_rows() ==
                   std::vector<native::table_row_id>{30},
               "incremental search uses only the primary column");
    }

    void test_groups_selection_and_reveal() {
        native::table_store store = sample_store();
        store.set_groups({{100, "Servers", 0, 2, true, false},
                          {200, "Archive", 2, 1, false, true}});
        native::table_view view(0, 0, 240, 80);
        view.set_columns(sample_columns()).set_model(&store);
        expect(view.get_display_row_count() == 3,
               "collapsed groups hide member display rows compactly");
        expect(view.get_display_row(0).group &&
                   view.get_display_row(1).group,
               "group headings are distinct mapped display rows");

        native::table_search query;
        query.text = "Primary";
        query.match = native::table_search_match::prefix;
        expect(view.find(query) == 10 &&
                   !view.get_group_expanded(100),
               "plain search includes collapsed logical rows");
        expect(view.find_and_reveal(query) &&
                   view.get_group_expanded(100) &&
                   view.get_selected_rows() ==
                       std::vector<native::table_row_id>{10},
               "find_and_reveal expands, selects, and reveals a row");
        expect(view.get_display_row_count() == 5,
               "expanding a group restores member display rows");

        int selection_events = 0;
        view.on_selection_change.connect(
            [&](const std::vector<native::table_row_id> &rows) {
                selection_events += rows ==
                                    std::vector<native::table_row_id>{30};
                return false;
            });
        view.set_selected_rows({20});
        expect(selection_events == 0,
               "programmatic table selection emits no user signal");
        view.on_native_selection({30});
        expect(selection_events == 1,
               "native table selection emits exactly one signal");
        view.set_group_expanded(100, false);
        expect(view.get_selected_rows() ==
                   std::vector<native::table_row_id>{30},
               "collapsing another group preserves stable selection");
    }

    class million_row_model final : public native::table_model
    {
    public:
        std::size_t row_count() const override { return 1000000; }

        native::table_row_id row_id(std::size_t row) const override {
            ++id_requests;
            return static_cast<native::table_row_id>(row + 1);
        }

        native::table_cell cell(
            std::size_t row,
            native::table_column_id column) const override {
            ++cell_requests;
            return {std::to_string(row) + ":" +
                        std::to_string(column),
                    nullptr};
        }

        std::optional<std::size_t>
        find(const native::table_search &query) const override {
            ++find_requests;
            return query.text == "indexed" ? std::optional(900000)
                                             : std::nullopt;
        }

        mutable std::size_t id_requests = 0;
        mutable std::size_t cell_requests = 0;
        mutable std::size_t find_requests = 0;
    };

    void test_virtual_scale() {
        million_row_model model;
        native::table_view view(0, 0, 320, 100);
        view.set_columns(sample_columns())
            .set_data_mode(native::table_data_mode::virtualized)
            .set_model(&model);
        expect(view.get_display_row_count() == 1000000 &&
                   model.id_requests == 0 &&
                   model.cell_requests == 0,
               "million-row mapping materializes no IDs or cells");
        const auto row = view.get_display_row(999999);
        expect(row.model_row == 999999 &&
                   model.id_requests == 0 &&
                   model.cell_requests == 0,
               "random virtual display mapping remains constant-space");
        native::table_search query;
        query.text = "indexed";
        expect(view.find(query) == 900001 &&
                   model.find_requests == 1 &&
                   model.id_requests == 1 &&
                   model.cell_requests == 0,
               "virtual search delegates to an optimized model index");
        expect(view.find_and_reveal(query) &&
                   view.get_selected_rows() ==
                       std::vector<native::table_row_id>{900001} &&
                   view.get_vertical_scroll_row() == 899998 &&
                   model.find_requests == 2 &&
                   model.id_requests == 2 &&
                   model.cell_requests == 0,
               "indexed find-and-reveal avoids a stable-ID scan");
        view.scroll_to_row(999999);
        expect(view.get_vertical_scroll_row() == 999998,
               "virtual scrolling exposes the million-row range");
    }

    void test_columns_and_sort_signals() {
        native::table_store store = sample_store();
        native::table_view view;
        view.set_columns(sample_columns()).set_model(&store);
        view.set_column_width(1, 4);
        expect(view.get_columns()[0].width == 24,
               "programmatic column width clamps to its minimum");
        view.move_column(2, 0);
        expect(view.get_columns()[0].id == 2,
               "programmatic column movement changes visual order");

        int sorts = 0;
        view.on_sort_request.connect([&](native::table_sort sort) {
            ++sorts;
            expect(sort.column == 1,
                   "sort signal retains semantic column identity");
            return false;
        });
        view.set_sort(native::table_sort{
            1, native::sort_direction::ascending});
        expect(sorts == 0,
               "programmatic sort indicator emits no user signal");
        view.on_native_sort_request(1);
        expect(sorts == 1 && view.get_sort()->direction ==
                                 native::sort_direction::descending,
               "native sort requests toggle direction and emit once");

        expect(!view.get_row_height(),
               "table rows use the native theme height by default");
        view.set_row_height(28);
        expect(view.get_row_height() == 28,
               "table row height is explicitly configurable");
        view.set_row_height(std::nullopt);
        expect(!view.get_row_height(),
               "clearing table row height restores the native default");
        bool rejected_height = false;
        try {
            view.set_row_height(0);
        } catch (const std::invalid_argument &) {
            rejected_height = true;
        }
        expect(rejected_height,
               "table row height rejects a zero-sized selection row");
    }
} // namespace

int main() {
    test_store_and_notifications();
    test_search();
    test_groups_selection_and_reveal();
    test_virtual_scale();
    test_columns_and_sort_signals();
    return failures == 0 ? 0 : 1;
}

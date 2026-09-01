//
// Tests backend-neutral window properties and hierarchy behavior.
// Native resources are not created, so the test requires no display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <native.h>

namespace
{
    static_assert(
        std::is_same_v<decltype(std::declval<const native::font_t &>()
                                    .get_metrics()),
                       native::font_metrics>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const native::gpx &>().measure_text(
                std::declval<const std::string &>())),
            native::text_metrics>);
    static_assert(std::is_base_of_v<native::app_wnd,
                                    native::owned_wnd>);
    static_assert(std::is_base_of_v<native::owned_wnd,
                                    native::modeless_wnd>);
    static_assert(std::is_base_of_v<native::owned_wnd,
                                    native::modal_wnd>);
    static_assert(std::is_base_of_v<native::modal_wnd,
                                    native::file_dialog>);
    static_assert(std::is_base_of_v<native::file_dialog,
                                    native::open_file_dialog>);
    static_assert(std::is_base_of_v<native::file_dialog,
                                    native::save_file_dialog>);
    static_assert(std::is_base_of_v<native::wnd, native::text_edit>);
    static_assert(
        std::is_base_of_v<native::text_edit, native::code_edit>);
    static_assert(!std::is_copy_constructible_v<native::clipboard>);
    static_assert(std::is_move_constructible_v<native::clipboard>);

    int failure_count = 0;

    class recording_gpx final : public native::gpx
    {
    public:
        using native::gpx::draw_img;

        native::gpx &set_clip(const native::rect &bounds) override {
            clip = bounds;
            return *this;
        }

        native::rect get_clip() const override {
            return clip;
        }

        native::gpx &clear(native::rgba) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_line(native::point, native::point) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_rect(native::rect, bool) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_native_text(
            const std::string &,
            native::point) override {
            painted = true;
            return *this;
        }

        native::gpx &draw_img(const native::img &,
                              native::point) override {
            painted = true;
            ++image_count;
            return *this;
        }

        native::rect clip = native::rect(0, 0, 40, 20);
        bool painted = false;
        int image_count = 0;
    };

    // Record a failed condition without stopping the remaining tests.
    void expect(bool condition, const std::string &description) {
        if (condition)
            return;

        std::cerr << "FAILED: " << description << '\n';
        ++failure_count;
    }

    // Verify property caching and the setter/getter naming contract.
    void test_cached_properties() {
        native::app_wnd window(
            "Initial", native::point(10, 20), native::size(300, 200));

        window.set_position({30, 40}).set_dimensions({640, 480});
        window.set_title("Updated");

        expect(window.get_position().x == 30 &&
                   window.get_position().y == 40,
               "window caches its position");
        expect(window.get_dimensions().w == 640 &&
                   window.get_dimensions().h == 480,
               "window caches its dimensions");
        expect(window.get_title() == "Updated",
               "window caches its title");
        expect(!window.get_created(),
               "construction does not create a window");

        window.on_native_move({50, 60});
        window.on_native_resize({800, 600});
        expect(window.get_position().x == 50 &&
                   window.get_position().y == 60,
               "native moves update the position cache");
        expect(window.get_dimensions().w == 800 &&
                   window.get_dimensions().h == 600,
               "native resizes update the dimensions cache");

        window.set_layout(
            std::make_unique<native::absolute_layout_manager>());
        expect(window.get_layout() != nullptr,
               "window owns its layout");
    }

    // Verify parent links remain safe in either lifetime order.
    void test_parent_lifetime() {
        auto child = std::make_unique<native::button>(
            "Action", native::rect(0, 0, 80, 24));

        {
            auto parent = std::make_unique<native::app_wnd>(
                "Parent", native::rect(0, 0, 320, 200));
            child->set_parent(parent.get());
            expect(child->get_parent() == parent.get(),
                   "child caches its parent");

            bool rejected_cycle = false;
            try {
                parent->set_parent(child.get());
            } catch (const std::invalid_argument &) {
                rejected_cycle = true;
            }
            expect(rejected_cycle, "parent hierarchy rejects cycles");
        }

        expect(child->get_parent() == nullptr,
               "destroying a parent detaches surviving children");
    }

    // Verify that independent ownership never enters child layout and
    // remains safe when C++ objects are destroyed in either order.
    void test_owned_window_lifetime() {
        auto owner = std::make_unique<native::app_wnd>(
            "Owner", native::rect(0, 0, 320, 200));
        auto modeless = std::make_unique<native::modeless_wnd>(
            *owner, "Palette", native::rect(360, 0, 180, 240));
        auto modal = std::make_unique<native::modal_wnd>(
            *owner, "Dialog", native::rect(80, 60, 200, 120));

        expect(modeless->get_owner() == owner.get() &&
                   modal->get_owner() == owner.get(),
               "independent windows cache their top-level owner");
        expect(modeless->get_parent() == nullptr &&
                   modal->get_parent() == nullptr,
               "independent windows are not layout children");
        expect(!modeless->get_modal() && modal->get_modal(),
               "owned window types expose distinct modal semantics");
        expect(owner->get_input_enabled() &&
                   modeless->get_input_enabled() &&
                   modal->get_input_enabled(),
               "unshown owned windows do not block input");

        bool rejected_result = false;
        try {
            modal->close(native::dialog_result::none);
        } catch (const std::invalid_argument &) {
            rejected_result = true;
        }
        expect(rejected_result,
               "modal close requires a final dialog result");

        owner.reset();
        expect(modeless->get_owner() == nullptr &&
                   modal->get_owner() == nullptr,
               "destroying an owner detaches surviving owned windows");
    }

    // Verify file-dialog properties remain portable before a native
    // system panel is created.
    void test_file_dialog_properties() {
        native::app_wnd owner(
            "Owner", native::rect(0, 0, 320, 200));
        native::open_file_dialog open(owner, "Choose Source");
        open.set_initial_path("/documents")
            .add_filter({"Images", {"*.png", "*.jpg"}})
            .add_filter({"Text", {"*.txt"}});
        open.set_allow_multiple(true);

        expect(open.get_owner() == &owner && open.get_modal(),
               "open dialog is an owner-modal object");
        expect(open.get_initial_path() == "/documents" &&
                   open.get_filters().size() == 2 &&
                   open.get_filters()[0].patterns.size() == 2,
               "open dialog caches its path and filter groups");
        expect(open.get_allow_multiple(),
               "open dialog caches multiple-selection state");
        expect(open.get_path().empty() && open.get_paths().empty() &&
                   open.get_result() == native::dialog_result::none,
               "unshown open dialog has no selection or result");

        open.clear_filters();
        expect(open.get_filters().empty(),
               "file dialog clears its filters");

        native::save_file_dialog save(owner, "Export");
        save.set_suggested_name("drawing")
            .set_default_extension("png")
            .set_confirm_overwrite(false);
        save.set_initial_path("/exports");
        save.set_filters({{"PNG image", {"*.png"}}});

        expect(save.get_suggested_name() == "drawing" &&
                   save.get_default_extension() == "png" &&
                   !save.get_confirm_overwrite(),
               "save dialog caches filename and overwrite options");
        expect(save.get_initial_path() == "/exports" &&
                   save.get_filters().size() == 1,
               "save dialog shares path and filter state");

        open.on_native_accept({"ignored.txt"});
        save.on_native_cancel();
        expect(open.get_paths().empty() &&
                   open.get_result() == native::dialog_result::none &&
                   save.get_result() == native::dialog_result::none,
               "inactive dialogs ignore stale native completion");
    }

    // Verify portable selection state before native resources are
    // created.
    void test_selection_controls() {
        native::app_wnd parent("Controls",
                               native::rect(0, 0, 320, 240));

        native::check enabled("Enabled");
        int check_events = 0;
        bool last_check_value = true;
        enabled.on_change.connect([&](bool checked) {
            ++check_events;
            last_check_value = checked;
            return false;
        });
        enabled.set_parent(&parent).set_bounds(
            native::rect(8, 8, 120, 24));
        enabled.set_checked(true);
        expect(enabled.get_checked(),
               "check caches programmatic state");
        expect(check_events == 0,
               "programmatic check changes do not emit");
        enabled.on_native_checked(false);
        expect(
            !enabled.get_checked() && check_events == 1 &&
                !last_check_value,
            "native check changes update state and emit their value");

        native::radio first("First");
        native::radio second("Second");
        first.set_parent(&parent);
        second.set_parent(&parent);
        first.set_selected(true);
        second.on_native_selected();
        expect(!first.get_selected() && second.get_selected(),
               "sibling radios are mutually exclusive");

        native::list choices({"One", "Two", "Three"});
        int selected = -1;
        choices.on_selection_change.connect([&](int index) {
            selected = index;
            return false;
        });
        choices.set_parent(&parent).set_bounds(
            native::rect(8, 64, 160, 80));
        choices.set_selected_index(1);
        expect(choices.get_selected_index() == 1 && selected == -1,
               "list caches programmatic selection without emitting");
        choices.on_native_selection(2);
        expect(choices.get_selected_index() == 2 && selected == 2,
               "native list selection updates state and emits");
        choices.remove_item(0);
        expect(choices.get_selected_index() == 1,
               "list selection follows an item removed before it");
    }

    // Verify accordion state, borrowed contents, geometry, and
    // user-only expansion notifications without native resources.
    void test_accordion_model() {
        native::button first_content("First", 0, 0, 120, 40);
        native::button second_content("Second", 0, 0, 120, 50);
        native::button third_content("Third", 0, 0, 120, 60);
        native::accordion sections(0, 0, 180, 54);
        sections.add_item("One", first_content);
        sections.add_item("Two", second_content);
        sections.add_item("Three", third_content);

        expect(sections.get_item_count() == 3 &&
                   sections.get_expanded_index() == 0 &&
                   first_content.get_parent() == &sections,
               "accordion expands its first borrowed section");
        sections.set_expanded_index(2);
        expect(!sections.get_item(0).get_expanded() &&
                   sections.get_item(2).get_expanded(),
               "single accordion expansion collapses the old section");

        int expansion_events = 0;
        int expanded = -2;
        sections.on_expanded_change.connect([&](int index) {
            ++expansion_events;
            expanded = index;
            return false;
        });
        sections.set_expanded_index(1);
        expect(expansion_events == 0,
               "programmatic accordion expansion does not emit");
        sections.on_native_toggle(0);
        expect(expansion_events == 1 && expanded == 0 &&
                   sections.get_expanded_index() == 0,
               "user accordion toggle emits exactly once");

        sections.set_mode(native::accordion_mode::multiple);
        sections.get_item(1).set_expanded(true);
        expect(sections.get_item(0).get_expanded() &&
                   sections.get_item(1).get_expanded(),
               "multiple accordion mode keeps independent sections open");

        sections.on_native_focus(true);
        sections.get_item(2).set_enabled(false);
        sections.on_native_navigation(
            native::accordion_navigation::last);
        expect(sections.get_focused_index() == 1,
               "accordion keyboard navigation skips disabled headers");
        sections.get_item(2).set_enabled(true);
        sections.on_native_navigation(
            native::accordion_navigation::last);
        expect(sections.get_focused_index() == 2,
               "accordion keyboard navigation reaches the last header");

        bool rejected_index = false;
        try {
            sections.set_expanded_index(3);
        } catch (const std::out_of_range &) {
            rejected_index = true;
        }
        expect(rejected_index, "accordion rejects an invalid index");

        sections.set_mode(native::accordion_mode::single)
            .set_expanded_index(1)
            .remove_item(0);
        expect(sections.get_expanded_index() == 0,
               "accordion expansion follows a preceding removal");
        for (std::size_t index = 0;
             index < sections.get_item_count();
             ++index) {
            const native::rect header =
                sections.get_header_bounds(index);
            const native::rect content =
                sections.get_content_bounds(index);
            expect(content.y2() >= header.y2(),
                   "accordion body geometry is never negative");
        }
        sections.clear_items();
        expect(sections.get_item_count() == 0 &&
                   sections.get_expanded_index() == -1 &&
                   first_content.get_parent() == nullptr &&
                   second_content.get_parent() == nullptr &&
                   third_content.get_parent() == nullptr,
               "clearing an accordion detaches all borrowed contents");
    }

    // Verify the portable icon-grid model, rebasing, layout, hit
    // testing, spatial navigation, activation, and empty state.
    void test_icon_view_model() {
        auto small = std::make_shared<native::img>(8, 12);
        auto wide = std::make_shared<native::img>(31, 7);
        std::vector<native::icon_view_item> items;
        for (std::uint64_t id = 1; id <= 8; ++id) {
            items.push_back({"Item " + std::to_string(id),
                             id == 1 ? small : wide,
                             id,
                             true});
        }
        native::icon_view icons(items, native::rect(0, 0, 90, 100));
        expect(icons.get_items().size() == 8 &&
                   icons.get_selected_index() == -1,
               "icon view caches its initial items without selection");

        icons.set_selected_index(2);
        std::vector<native::icon_view_item> reordered = items;
        std::swap(reordered[2], reordered[5]);
        icons.set_items(reordered);
        expect(icons.get_selected_index() == 5 &&
                   icons.get_items()[5].id == 3,
               "icon replacement retains selection by stable ID");
        icons.set_items(items);

        icons.set_selected_index(3);
        icons.remove_item(1);
        expect(icons.get_selected_index() == 2 &&
                   icons.get_items()[2].id == 4,
               "icon selection follows its item after a removal");
        icons.remove_item(2);
        expect(icons.get_selected_index() == -1,
               "removing the selected icon clears selection");

        bool rejected_selection = false;
        try {
            icons.set_selected_index(99);
        } catch (const std::out_of_range &) {
            rejected_selection = true;
        }
        expect(rejected_selection,
               "icon view rejects an invalid selected index");

        icons.set_scroll_offset(0);
        const native::rect narrow_first = icons.get_item_bounds(0);
        const native::rect narrow_second = icons.get_item_bounds(1);
        icons.set_dimensions({300, 100});
        const native::rect wide_second = icons.get_item_bounds(1);
        expect(narrow_second.p.y > narrow_first.p.y &&
                   wide_second.p.y == icons.get_item_bounds(0).p.y,
               "icon grid wraps differently at narrow and wide widths");
        const native::point inside(
            static_cast<native::coord>(wide_second.p.x + 2),
            static_cast<native::coord>(wide_second.p.y + 2));
        expect(icons.item_at(inside) == 1 &&
                   icons.item_at({299, 99}) == -1,
               "icon view hit testing distinguishes items and gaps");

        int selection_events = 0;
        int activation_events = 0;
        icons.on_selection_change.connect([&](int) {
            ++selection_events;
            return false;
        });
        icons.on_item_activate.connect([&](int) {
            ++activation_events;
            return false;
        });
        icons.set_selected_index(0);
        icons.on_native_navigation(native::icon_view_navigation::right);
        icons.on_native_navigation(native::icon_view_navigation::down);
        expect(icons.get_selected_index() == 4 &&
                   selection_events == 2,
               "icon keyboard navigation follows grid neighbors");
        icons.on_native_activate(icons.get_selected_index());
        expect(activation_events == 1,
               "user icon activation emits exactly once");

        std::vector<native::icon_view_item> with_disabled =
            icons.get_items();
        with_disabled.back().enabled = false;
        icons.set_items(std::move(with_disabled));
        icons.set_selected_index(0);
        icons.on_native_navigation(native::icon_view_navigation::end);
        expect(icons.get_selected_index() ==
                   static_cast<int>(icons.get_items().size()) - 2,
               "icon keyboard navigation skips disabled endpoints");

        const int selected_before_resize = icons.get_selected_index();
        icons.set_icon_size({24, 64});
        expect(icons.get_selected_index() == selected_before_resize,
               "thumbnail dimensions do not alter icon selection");
        icons.set_dimensions({90, 70});
        icons.set_scroll_offset(100000);
        expect(icons.get_scroll_offset() > 0 &&
                   icons.get_content_dimensions().h > 70,
               "icon view clamps a scrollable collection offset");

        icons.add_item({"Last", small, 99, true});
        expect(icons.get_items().back().id == 99,
               "icon view appends items");
        icons.clear_items();
        icons.on_native_navigation(native::icon_view_navigation::end);
        expect(icons.get_items().empty() &&
                   icons.get_selected_index() == -1 &&
                   icons.get_scroll_offset() == 0 &&
                   icons.get_content_dimensions().h == 0,
               "empty icon view has no selection, content, or scroll");
    }

    // Verify stable tree identity, hierarchy geometry, disclosure,
    // classic navigation, signal rules, mutation, and validation.
    void test_tree_view_model() {
        auto icon = std::make_shared<native::img>(12, 12);
        std::vector<native::tree_view_item> items = {
            {"Root",
             icon,
             100,
             {{"First", icon, 101},
              {"Branch",
               icon,
               102,
               {{"Leaf", icon, 103}},
               false}},
             true},
            {"Disabled", icon, 200, {}, false, false}};
        native::tree_view tree(items, native::rect(0, 0, 180, 65));
        expect(tree.get_visible_item_count() == 4 &&
                   tree.get_visible_item(0).id == 100 &&
                   tree.get_visible_item(2).depth == 1,
               "tree flattens only descendants of expanded branches");

        int selection_events = 0;
        int expansion_events = 0;
        int activation_events = 0;
        tree.on_selection_change.connect(
            [&](native::tree_item_id) {
                ++selection_events;
                return false;
            });
        tree.on_expanded_change.connect(
            [&](native::tree_item_id, bool) {
                ++expansion_events;
                return false;
            });
        tree.on_item_activate.connect(
            [&](native::tree_item_id) {
                ++activation_events;
                return false;
            });

        tree.set_selected_item(102);
        expect(tree.get_selected_item() == 102 &&
                   selection_events == 0,
               "programmatic tree selection is silent");
        tree.on_native_navigation(native::tree_view_navigation::right);
        expect(tree.get_expanded(102) &&
                   tree.get_visible_item_count() == 5 &&
                   expansion_events == 1,
               "tree Right expands a collapsed selected branch");
        tree.on_native_navigation(native::tree_view_navigation::right);
        expect(tree.get_selected_item() == 103 &&
                   selection_events == 1,
               "tree Right enters an expanded branch");
        tree.on_native_navigation(native::tree_view_navigation::left);
        tree.on_native_navigation(native::tree_view_navigation::left);
        expect(tree.get_selected_item() == 102 &&
                   !tree.get_expanded(102) &&
                   expansion_events == 2,
               "tree Left selects a parent and then collapses it");

        tree.on_native_navigation(native::tree_view_navigation::end);
        expect(tree.get_selected_item() == 102,
               "tree navigation skips a disabled endpoint");
        tree.on_native_activate(200);
        expect(activation_events == 0,
               "disabled tree items cannot activate");

        tree.set_selected_item(100);
        tree.on_native_double_click(100);
        expect(!tree.get_expanded(100) &&
                   activation_events == 1 &&
                   expansion_events == 3,
               "tree double-click toggles a branch and activates once");
        tree.set_expanded(100, true);
        expect(expansion_events == 3,
               "programmatic tree expansion is silent");

        const native::rect disclosure =
            tree.get_disclosure_bounds(0);
        const native::point disclosure_center(
            static_cast<native::coord>(
                disclosure.p.x + disclosure.d.w / 2),
            static_cast<native::coord>(
                disclosure.p.y + disclosure.d.h / 2));
        const native::tree_view_hit hit =
            tree.hit_test(disclosure_center);
        expect(hit.id == 100 &&
                   hit.part == native::tree_view_hit_part::disclosure &&
                   tree.item_at({180, 64}) ==
                       native::invalid_tree_item_id,
               "tree hit testing separates disclosure and empty space");

        tree.set_selected_item(103).remove_item(102);
        expect(!tree.contains_item(102) &&
                   !tree.contains_item(103) &&
                   tree.get_selected_item() ==
                       native::invalid_tree_item_id,
               "removing a tree branch clears descendant selection");
        tree.add_item({"Added", icon, 300}, 100);
        expect(tree.contains_item(300),
               "tree appends a child by stable parent ID");

        bool rejected_duplicate = false;
        try {
            tree.add_item({"Duplicate", icon, 300});
        } catch (const std::invalid_argument &) {
            rejected_duplicate = true;
        }
        bool rejected_zero = false;
        try {
            tree.set_items({{"Invalid", icon, 0}});
        } catch (const std::invalid_argument &) {
            rejected_zero = true;
        }
        expect(rejected_duplicate && rejected_zero,
               "tree rejects duplicate and zero stable IDs");

        std::vector<native::tree_view_item> many;
        for (native::tree_item_id id = 1; id <= 20; ++id)
            many.push_back(
                {"Row " + std::to_string(id), icon, id});
        tree.set_items(std::move(many));
        tree.set_selected_item(20);
        expect(tree.get_scroll_offset() > 0,
               "tree scrolls to keep programmatic selection visible");
        tree.clear_items();
        expect(tree.get_visible_item_count() == 0 &&
                   tree.get_scroll_offset() == 0 &&
                   tree.get_selected_item() ==
                       native::invalid_tree_item_id,
               "empty tree has no visible rows, selection, or scroll");
    }

    // Verify editor modes, cached values, and complete-value validation
    // without creating a backend widget.
    void test_text_edit_properties() {
        native::text_edit single("123");
        int changes = 0;
        single.on_change.connect([&](const std::string &) {
            ++changes;
            return false;
        });
        single.set_validator([](const std::string &text) {
            return text.size() <= 4;
        });
        expect(single.validate("1234") &&
                   !single.validate("12345") &&
                   !single.validate("one\ntwo") &&
                   !single.validate(std::string("a\0b", 3)),
               "single-line editors validate complete proposed values");
        expect(single.on_native_text("1234") &&
                   !single.on_native_text("12345") &&
                   single.get_text() == "1234" && changes == 1,
               "rejected native edits preserve cached editor text");
        single.set_text("12");
        expect(single.get_text() == "12" && changes == 1,
               "programmatic editor changes do not emit on_change");
        single.set_read_only(true);
        expect(single.get_read_only() &&
                   !single.on_native_text("13") &&
                   single.get_text() == "12" && changes == 1,
               "read-only editors reject native changes");

        native::text_edit multiline(
            "one\ntwo", native::text_edit_mode::multi_line);
        expect(multiline.get_mode() ==
                       native::text_edit_mode::multi_line &&
                   multiline.validate("three\nfour"),
               "multiline editors accept portable line feeds");
    }

    // Verify source-document state and user bridges before creation.
    void test_code_edit_properties() {
        native::code_edit editor("one\ntw\xc3\xb6\n");
        editor.set_path("sample.cpp")
            .set_line_ending(native::line_ending::crlf)
            .set_show_line_numbers(false)
            .set_tab_width(8)
            .set_language("cpp");
        editor.add_marker(
            {1, native::marker_kind::breakpoint});
        editor.set_diagnostics(
            {{{4, 8}, native::diagnostic_severity::warning,
              "example"}});
        editor.set_style_runs({{{0, 3}, 1}});

        expect(editor.line_count() == 3 &&
                   editor.line_start(1) == 4 &&
                   editor.line_text(1) == "tw\xc3\xb6",
               "code editor exposes canonical UTF-8 lines");
        expect(editor.get_path() == "sample.cpp" &&
                   editor.get_line_ending() ==
                       native::line_ending::crlf &&
                   !editor.get_show_line_numbers() &&
                   editor.get_tab_width() == 8 &&
                   editor.get_language() == "cpp",
               "code editor caches file and view properties");
        expect(editor.markers().size() == 1 &&
                   editor.diagnostics().size() == 1 &&
                   editor.style_runs().size() == 1,
               "code editor retains presentation overlays");

        int changes = 0;
        int completions = 0;
        editor.on_text_change.connect([&changes] {
            ++changes;
            return false;
        });
        editor.on_complete.connect(
            [&completions](native::completion_item item) {
                if (item.insert == "value")
                    ++completions;
                return false;
            });
        editor.go_to_offset(3).insert(3, "!");
        expect(editor.get_text() == "one!\ntw\xc3\xb6\n" &&
                   editor.can_undo() && changes == 1,
               "code edit command mutates source and emits once");
        expect(editor.undo() && editor.redo() && changes == 3,
               "code editor undo and redo are document-local edits");
        editor.show_completion(
            {{"value", "value", "completion"}});
        expect(editor.on_native_key(native::code_edit_key::enter) &&
                   !editor.get_completion_visible() && completions == 1,
               "code editor completion accepts from the keyboard");
        editor.set_validator([](const std::string &text) {
            return text.find("reject") == std::string::npos;
        });
        const std::string accepted = editor.get_text();
        expect(!editor.on_native_text_input("reject") &&
                   !editor.on_native_text("reject") &&
                   editor.get_text() == accepted && changes == 3,
               "code editor validation rejects all native bridges");
    }

    // Verify screen queries use only the current normalized snapshot.
    void test_screen_snapshot() {
        expect(native::screen::count() == 0,
               "screen snapshot starts empty");

        native::rect virtual_desktop = native::screen::virtual_bounds();
        expect(virtual_desktop.w() == 0 && virtual_desktop.h() == 0,
               "empty virtual bounds do not trigger native detection");

        native::screen clipped(7,
                               native::rect(100, 100, 200, 100),
                               native::rect(50, 50, 200, 100),
                               false);
        expect(clipped.work_area().x1() == 100 &&
                   clipped.work_area().y1() == 100 &&
                   clipped.work_area().w() == 150 &&
                   clipped.work_area().h() == 50,
               "screen clips its work area to its bounds");

        native::screen fallback(
            3, native::rect(10, 20, 100, 100), native::rect(), false);
        expect(
            fallback.work_area().x1() == fallback.bounds().x1() &&
                fallback.work_area().y1() == fallback.bounds().y1() &&
                fallback.work_area().w() == fallback.bounds().w() &&
                fallback.work_area().h() == fallback.bounds().h(),
            "screen falls back to bounds for an unavailable work area");
        expect(fallback.is_landscape(), "a square screen is landscape");
    }

    // Verify the selected backend supplies a usable portable-target
    // theme.
    void test_theme_factory() {
        recording_gpx graphics;
        auto painter = native::theme::create(graphics);

        expect(painter != nullptr,
               "backend creates a theme implementation");
        if (!painter)
            return;

        const native::theme::metrics metrics = painter->defaults();
        expect(metrics.menu_bar_height > 0 &&
                   metrics.check_height > 0 &&
                   metrics.radio_height > 0 &&
                   metrics.list_item_height > 0 &&
                   metrics.header_height > 0 &&
                   metrics.disclosure_size > 0 &&
                   metrics.icon_view_min_item_width > 0 &&
                   metrics.scrollbar_extent > 0,
               "theme reports usable control metrics");

        painter->draw_menu_bar(native::rect(0, 0, 40, 20));
        native::theme::state selected;
        selected.selected = true;
        painter->draw_check(
            native::rect(0, 0, 80, 20), "Check", selected);
        painter->draw_radio(
            native::rect(0, 0, 80, 20), "Radio", selected);
        painter->draw_list(
            native::rect(0, 0, 80, 40), {"First", "Second"}, 1);
        painter->draw_text_edit_frame(
            native::rect(0, 0, 80, 24), selected);
        selected.focused = true;
        painter->draw_surface(native::rect(0, 0, 80, 24),
                              native::surface_kind::header,
                              selected);
        painter->draw_selection(native::rect(0, 0, 80, 24),
                                native::selection_shape::tile,
                                selected);
        painter->draw_focus(native::rect(0, 0, 80, 24), selected);
        painter->draw_disclosure(
            native::rect(0, 0, 12, 12),
            native::disclosure_state::expanded,
            selected);
        painter->draw_separator(
            native::rect(0, 0, 80, 1),
            native::separator_orientation::horizontal);
        painter->draw_scrollbar_part(
            native::rect(0, 0, 16, 40),
            native::scrollbar_orientation::vertical,
            native::scrollbar_part::thumb,
            selected);
        expect(graphics.painted,
               "theme paints basic and advanced semantic parts");
    }

    // Verify backend-neutral graphics additions preserve state and
    // route geometry, bounded text, and scaled images through gpx.
    void test_advanced_graphics() {
        recording_gpx graphics;
        const native::rgba initial_ink(1, 2, 3, 4);
        const native::rgba initial_paper(5, 6, 7, 8);
        const native::rect initial_clip(2, 3, 30, 14);
        graphics.set_ink(initial_ink)
            .set_paper(initial_paper)
            .set_pen(3)
            .set_clip(initial_clip);
        {
            auto saved = graphics.save_state();
            graphics.set_ink(native::rgba(200, 0, 0, 255))
                .set_paper(native::rgba(0, 0, 0, 255))
                .set_pen(9)
                .set_clip(native::rect(0, 0, 1, 1));
        }
        expect(static_cast<std::uint32_t>(graphics.get_ink()) ==
                       static_cast<std::uint32_t>(initial_ink) &&
                   static_cast<std::uint32_t>(graphics.get_paper()) ==
                       static_cast<std::uint32_t>(initial_paper) &&
                   graphics.get_pen() == 3 &&
                   graphics.get_clip().p.x == initial_clip.p.x &&
                   graphics.get_clip().p.y == initial_clip.p.y &&
                   graphics.get_clip().d.w == initial_clip.d.w &&
                   graphics.get_clip().d.h == initial_clip.d.h,
               "graphics state guard restores every portable property");

        graphics.painted = false;
        graphics.draw_ellipse(native::rect(0, 0, 20, 12), true)
            .draw_polyline({{0, 0}, {5, 8}, {12, 2}})
            .draw_polygon({{0, 0}, {8, 0}, {4, 8}}, false)
            .draw_text("bounded text",
                       native::rect(0, 0, 25, 16),
                       {native::text_align::center,
                        native::text_valign::center,
                        native::text_overflow::ellipsis,
                        true});
        expect(graphics.painted &&
                   graphics.get_clip().p.x == initial_clip.p.x &&
                   graphics.get_clip().p.y == initial_clip.p.y,
               "portable geometry and bounded text draw and restore clip");

        native::img source(2, 2);
        source.pixels()[0] = native::rgba(255, 0, 0, 255);
        source.pixels()[1] = native::rgba(0, 255, 0, 255);
        source.pixels()[2] = native::rgba(0, 0, 255, 255);
        source.pixels()[3] = native::rgba(255, 255, 255, 128);
        const int before = graphics.image_count;
        graphics.draw_img(source,
                          native::rect(0, 0, 7, 5),
                          native::image_filter::linear);
        expect(graphics.image_count == before + 1,
               "scaled image drawing reaches the backend image primitive");

        bool rejected_crop = false;
        try {
            graphics.draw_img(source,
                              native::rect(1, 1, 2, 2),
                              native::rect(0, 0, 4, 4));
        } catch (const std::out_of_range &) {
            rejected_crop = true;
        }
        expect(rejected_crop,
               "cropped image drawing validates the source rectangle");
    }

    // Verify installed-font discovery and byte-identical portable font
    // creation, measurement, movement, and drawing.
    void test_portable_fonts() {
        constexpr native::font_role roles[] = {
            native::font_role::system,
            native::font_role::fixed,
            native::font_role::icon_label,
            native::font_role::title,
            native::font_role::small,
            native::font_role::control};
        for (native::font_role role : roles) {
            const native::font_t &stock = native::font_t::stock(role);
            const native::font_metrics metrics = stock.get_metrics();
            expect(stock.valid() && stock.spec().source ==
                       native::font_source::stock,
                   "every semantic stock-font role is valid");
            expect(metrics.ascent > 0 && metrics.descent > 0 &&
                       metrics.leading > 0 && metrics.height > 0 &&
                       metrics.max_advance > 0,
                   "stock fonts expose positive editor metrics");
        }

        const std::uint8_t malformed[] = {0, 1, 2, 3};
        native::font_t invalid = native::font_t::from_memory(
            malformed, sizeof(malformed), 16);
        expect(!invalid.valid(), "malformed font data is rejected");

        const std::vector<native::font_description> installed =
            native::font_t::enumerate_installed();
        expect(!installed.empty(), "installed fonts can be enumerated");

        native::font_t from_file;
        std::vector<std::uint8_t> bytes;
        for (const native::font_description &description : installed) {
            from_file = native::font_t::from_file(
                description.path, 18, description.face_index);
            if (!from_file.valid())
                continue;

            std::ifstream stream(description.path, std::ios::binary);
            bytes = std::vector<std::uint8_t>{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            if (!stream.bad() && !bytes.empty())
                break;
            from_file = native::font_t();
        }
        expect(from_file.valid(),
               "an enumerated font can be created from its file");
        if (!from_file.valid())
            return;

        native::font_t from_memory = native::font_t::from_memory(
            bytes.data(),
            bytes.size(),
            18,
            from_file.spec().face_index);
        expect(from_memory.valid(),
               "the same font can be created from copied memory");
        if (!from_memory.valid())
            return;

        const native::font_metrics file_metrics =
            from_file.get_metrics();
        const native::font_metrics memory_metrics =
            from_memory.get_metrics();
        expect(file_metrics.height > 0 && file_metrics.ascent > 0 &&
                   file_metrics.descent > 0,
               "portable fonts expose positive editor metrics");
        expect(file_metrics.height == memory_metrics.height &&
                   file_metrics.max_advance ==
                       memory_metrics.max_advance,
               "file and memory fonts have identical metrics");

        const native::text_metrics file_text =
            from_file.measure_text("Native AV");
        const native::text_metrics memory_text =
            from_memory.measure_text("Native AV");
        expect(file_text.width > 0 && file_text.advance > 0 &&
                   file_text.width == memory_text.width &&
                   file_text.advance == memory_text.advance,
               "file and memory fonts measure UTF-8 identically");
        expect(from_memory.measure_text("").height ==
                   memory_metrics.height,
               "empty text retains the selected line height");
        expect(from_memory.measure_character(U'A').advance > 0,
               "portable fonts measure individual characters");

        const std::uint32_t memory_id = from_memory.id();
        native::font_t moved = std::move(from_memory);
        expect(moved.valid() && moved.id() == memory_id &&
                   !from_memory.valid(),
               "moving a portable font preserves its registration");

        recording_gpx graphics;
        graphics.set_font(moved).draw_text(
            "Portable", native::point(0, 0));
        expect(graphics.painted,
               "portable text is rasterized through image drawing");
    }

    // Verify image contexts and both supported encoded representations.
    void test_image_io() {
        const native::rgba source_color(32, 96, 208, 255);
        native::img source(8, 8);
        source.get_gpx().clear(source_color);
        for (std::size_t index = 0; index < 64; ++index) {
            expect(source.pixels()[index].r == source_color.r &&
                       source.pixels()[index].g == source_color.g &&
                       source.pixels()[index].b == source_color.b &&
                       source.pixels()[index].a == source_color.a,
                   "image context begins with a full-image clip");
        }

        native::img clipped(4, 4);
        clipped.get_gpx()
            .clear(native::rgba(0, 0, 0, 255))
            .set_clip(native::rect(1, 1, 2, 2))
            .clear(native::rgba(255, 255, 255, 255));
        int white_pixels = 0;
        for (std::size_t index = 0; index < 16; ++index) {
            if (clipped.pixels()[index].r == 255)
                ++white_pixels;
        }
        expect(white_pixels == 4,
               "image context uses half-open clipping boundaries");

        native::img alpha_target(1, 1);
        alpha_target.get_gpx().clear(native::rgba(0, 0, 255, 255));
        native::img alpha_source(1, 1);
        alpha_source.pixels()[0] = native::rgba(255, 0, 0, 128);
        alpha_target.get_gpx().draw_img(
            alpha_source, native::point(0, 0));
        const native::rgba blended = alpha_target.pixels()[0];
        expect(blended.r >= 127 && blended.r <= 128 &&
                   blended.g == 0 && blended.b >= 127 &&
                   blended.b <= 128 && blended.a == 255,
               "image drawing applies straight-alpha source-over");

        const native::rgba alpha_color(255, 0, 255, 79);
        source.pixels()[1] = alpha_color;
        const std::vector<std::uint8_t> png =
            source.encode(native::image_format::png);
        expect(png.size() >= 8 && png[0] == 0x89 && png[1] == 'P' &&
                   png[2] == 'N' && png[3] == 'G',
               "PNG encoding has the expected signature");
        native::img png_copy =
            native::img::decode(png.data(), png.size());
        expect(png_copy.w() == source.w() && png_copy.h() == source.h(),
               "PNG memory round trip preserves dimensions");
        expect(static_cast<std::uint32_t>(png_copy.pixels()[0]) ==
                   static_cast<std::uint32_t>(source_color),
               "PNG memory round trip preserves RGBA pixels");
        expect(static_cast<std::uint32_t>(png_copy.pixels()[1]) ==
                   static_cast<std::uint32_t>(alpha_color),
               "PNG memory round trip preserves alpha");

        source.get_gpx()
            .set_clip(native::rect(0, 0, 8, 8))
            .clear(source_color);
        const std::vector<std::uint8_t> jpeg =
            source.encode(native::image_format::jpeg, 95);
        expect(jpeg.size() >= 2 && jpeg[0] == 0xff && jpeg[1] == 0xd8,
               "JPEG encoding has the expected signature");
        native::img jpeg_copy =
            native::img::decode(jpeg.data(), jpeg.size());
        expect(jpeg_copy.w() == source.w() &&
                   jpeg_copy.h() == source.h(),
               "JPEG memory round trip preserves dimensions");
        const native::rgba jpeg_pixel = jpeg_copy.pixels()[0];
        expect(
            std::abs(static_cast<int>(jpeg_pixel.r) - source_color.r) <=
                    8 &&
                std::abs(static_cast<int>(jpeg_pixel.g) -
                         source_color.g) <= 8 &&
                std::abs(static_cast<int>(jpeg_pixel.b) -
                         source_color.b) <= 8 &&
                jpeg_pixel.a == 255,
            "JPEG memory round trip preserves an opaque solid color");

        const std::string path = "native-image-codec-test.png";
        source.save(path);
        native::img file_copy = native::img::load(path);
        std::remove(path.c_str());
        expect(file_copy.w() == source.w() &&
                   file_copy.h() == source.h(),
               "PNG file round trip preserves dimensions");

        bool rejected_quality = false;
        try {
            (void)source.encode(native::image_format::jpeg, 0);
        } catch (const std::invalid_argument &) {
            rejected_quality = true;
        }
        expect(rejected_quality, "JPEG encoding validates quality");

        const std::uint8_t broken_jpeg[] = {0xff, 0xd8};
        bool rejected_image = false;
        try {
            (void)native::img::decode(broken_jpeg, sizeof(broken_jpeg));
        } catch (const std::runtime_error &) {
            rejected_image = true;
        }
        expect(rejected_image,
               "image decoding rejects malformed input");

        bool rejected_character = false;
        native::font_t invalid;
        const native::font_metrics invalid_metrics =
            invalid.get_metrics();
        const native::text_metrics invalid_text =
            invalid.measure_text("x");
        expect(invalid_metrics.height == 0 &&
                   invalid_metrics.max_advance == 0 &&
                   invalid_text.width == 0 && invalid_text.advance == 0,
               "invalid fonts have empty measurements");
        try {
            (void)invalid.measure_character(0x110000);
        } catch (const std::invalid_argument &) {
            rejected_character = true;
        }
        expect(rejected_character,
               "font measurement rejects an invalid Unicode character");
    }
    // Counts layout passes so tests can assert how many ran.
    class counting_layout_manager final : public native::layout_manager
    {
    public:
        void relayout(native::wnd *, const native::rect &) override {
            ++passes;
        }

        void add_child(native::wnd *child) override {
            _children.push_back(child);
        }

        void remove_child(native::wnd *) override {}

        const std::vector<native::wnd *> &children() const override {
            return _children;
        }

        int passes = 0;

    private:
        std::vector<native::wnd *> _children;
    };

    // Reports a native resize from inside its own layout pass, the way
    // a toolkit that manages geometry synchronously would.
    class reentrant_layout_manager final : public native::layout_manager
    {
    public:
        void relayout(native::wnd *parent,
                      const native::rect &bounds) override {
            ++passes;
            if (!parent || passes >= 8)
                return;

            parent->on_native_resize(native::size(
                static_cast<native::dim>(bounds.d.w + 1),
                static_cast<native::dim>(bounds.d.h + 1)));
        }

        void add_child(native::wnd *child) override {
            _children.push_back(child);
        }

        void remove_child(native::wnd *) override {}

        const std::vector<native::wnd *> &children() const override {
            return _children;
        }

        int passes = 0;

    private:
        std::vector<native::wnd *> _children;
    };

    // Compare a window's arranged bounds against expected values.
    bool bounds_are(const native::wnd &window,
                    int x,
                    int y,
                    int w,
                    int h) {
        const native::rect b = window.get_bounds();
        return b.p.x == x && b.p.y == y && b.d.w == w && b.d.h == h;
    }

    // Verify a declared grid is the grid that actually arranges, even
    // when children were parented before the layout was installed.
    void test_grid_layout_placement() {
        native::app_wnd window("Grid",
                               native::rect(0, 0, 400, 300));
        native::button top("Top", native::rect(0, 0, 10, 10));
        native::button left("Left", native::rect(0, 0, 10, 10));
        native::button right("Right", native::rect(0, 0, 10, 10));

        top.set_parent(&window);
        left.set_parent(&window);
        right.set_parent(&window);

        auto grid = std::make_unique<native::grid_layout_manager>();
        (*grid) << native::row(native::pixels(50))
                << native::row(native::star())
                << native::column(native::star(1.0f))
                << native::column(native::star(3.0f))
                << native::cell(top, 0, 0, 1, 2)
                << native::cell(left, 1, 0)
                << native::cell(right, 1, 1);
        window.set_layout(std::move(grid));

        expect(bounds_are(top, 0, 0, 400, 50) &&
                   bounds_are(left, 0, 50, 100, 250) &&
                   bounds_are(right, 100, 50, 300, 250),
               "a declared grid arranges the tracks it declares");

        window.on_native_resize({800, 500});
        expect(bounds_are(top, 0, 0, 800, 50) &&
                   bounds_are(left, 0, 50, 200, 450) &&
                   bounds_are(right, 200, 50, 600, 450),
               "fixed tracks keep their size and stars absorb growth");
    }

    // Verify margins and spans survive installation and resizing.
    void test_grid_layout_margins() {
        native::app_wnd window("Margins",
                               native::rect(0, 0, 200, 200));
        native::button cell("Cell", native::rect(0, 0, 10, 10));

        cell.set_parent(&window);

        auto grid = std::make_unique<native::grid_layout_manager>(2, 2);
        grid->add(cell, 0, 0, 2, 2, 10);
        window.set_layout(std::move(grid));

        expect(bounds_are(cell, 10, 10, 180, 180),
               "an explicit span and margin survive installation");
    }

    // Verify auto-placement fills free cells without covering
    // explicitly placed children or nested grids.
    void test_grid_layout_auto_placement() {
        native::app_wnd window("Auto",
                               native::rect(0, 0, 200, 100));
        native::button placed("Placed", native::rect(0, 0, 10, 10));
        native::button first("First", native::rect(0, 0, 10, 10));
        native::button second("Second", native::rect(0, 0, 10, 10));

        auto grid = std::make_unique<native::grid_layout_manager>(2, 2);
        grid->add(placed, 0, 0, 1, 2);
        window.set_layout(std::move(grid));

        first.set_parent(&window);
        second.set_parent(&window);

        expect(bounds_are(placed, 0, 0, 200, 50) &&
                   bounds_are(first, 0, 50, 100, 50) &&
                   bounds_are(second, 100, 50, 100, 50),
               "auto-placement steps over an explicit placement");
    }

    // Verify a child owned by a nested grid is not also claimed by the
    // grid above it.
    void test_grid_layout_nested_ownership() {
        native::app_wnd window("Nested",
                               native::rect(0, 0, 200, 200));
        native::button inner("Inner", native::rect(0, 0, 10, 10));
        native::button outer("Outer", native::rect(0, 0, 10, 10));

        auto root = std::make_unique<native::grid_layout_manager>(1, 2);
        auto nested =
            std::make_unique<native::grid_layout_manager>(1, 1);
        nested->add(inner, 0, 0);
        (*root) << native::child_grid(std::move(nested), 0, 1);

        inner.set_parent(&window);
        window.set_layout(std::move(root));
        outer.set_parent(&window);

        expect(bounds_are(inner, 100, 0, 100, 200),
               "a nested grid keeps the child it was given");
        expect(bounds_are(outer, 0, 0, 100, 200),
               "the grid above leaves a nested grid's cell alone");
    }

    // Verify absolute layout never moves the children it registers.
    void test_absolute_layout_preserves_bounds() {
        native::app_wnd window("Absolute",
                               native::rect(0, 0, 300, 200));
        native::button fixed("Fixed", native::rect(20, 30, 80, 24));

        fixed.set_parent(&window);
        auto layout =
            std::make_unique<native::absolute_layout_manager>();
        (*layout) << fixed;
        window.set_layout(std::move(layout));
        window.on_native_resize({600, 400});

        expect(bounds_are(fixed, 20, 30, 80, 24),
               "absolute layout preserves explicit bounds");
        expect(window.get_layout()->children().size() == 1,
               "absolute layout registers its children once");
    }

    // Verify layout passes run when geometry changes and not
    // otherwise, and that a pass cannot re-enter itself.
    void test_layout_pass_scheduling() {
        native::app_wnd window("Passes",
                               native::rect(0, 0, 320, 240));

        auto counting = std::make_unique<counting_layout_manager>();
        auto *passes = counting.get();
        window.set_layout(std::move(counting));
        expect(passes->passes == 1,
               "installing a layout arranges children once");

        window.on_native_resize({320, 240});
        expect(passes->passes == 1,
               "a resize notification repeating the cached size is "
               "ignored");

        window.on_native_resize({640, 480});
        expect(passes->passes == 2,
               "a resize notification carrying a new size arranges "
               "children");

        // An explicit geometry call is a request, not a
        // notification, so it always arranges children — but only
        // once, however many times the backend echoes it back.
        window.set_bounds(native::rect(10, 10, 800, 600));
        expect(passes->passes == 3,
               "setting new bounds arranges children exactly once");

        window.set_dimensions({320, 240});
        expect(passes->passes == 4,
               "setting new dimensions arranges children exactly "
               "once");

        window.set_position({40, 40});
        expect(passes->passes == 4,
               "moving a window does not arrange its children");

        native::app_wnd reentrant("Reentrant",
                                  native::rect(0, 0, 100, 100));
        auto guarded = std::make_unique<reentrant_layout_manager>();
        auto *nested_passes = guarded.get();
        reentrant.set_layout(std::move(guarded));
        expect(nested_passes->passes == 1,
               "a native resize reported during a layout pass does "
               "not re-enter it");
        expect(reentrant.get_dimensions().w == 101 &&
                   reentrant.get_dimensions().h == 101,
               "a native resize reported during a layout pass still "
               "updates the cache");
    }

    // Verify a destroyed child leaves the layout without disturbing
    // the children that remain.
    void test_layout_child_removal() {
        native::app_wnd window("Removal",
                               native::rect(0, 0, 200, 100));
        native::button keep("Keep", native::rect(0, 0, 10, 10));

        auto grid = std::make_unique<native::grid_layout_manager>(1, 2);
        grid->add(keep, 0, 0);
        window.set_layout(std::move(grid));
        keep.set_parent(&window);

        {
            native::button temporary("Temporary",
                                     native::rect(0, 0, 10, 10));
            temporary.set_parent(&window);
            expect(bounds_are(temporary, 100, 0, 100, 100),
                   "a later child is auto-placed in the free cell");
        }

        expect(bounds_are(keep, 0, 0, 100, 100),
               "a destroyed child leaves the remaining layout intact");
        expect(window.get_layout()->children().size() == 1,
               "a destroyed child is removed from the layout");
    }
} // namespace

int main() {
    test_cached_properties();
    test_parent_lifetime();
    test_owned_window_lifetime();
    test_file_dialog_properties();
    test_selection_controls();
    test_accordion_model();
    test_icon_view_model();
    test_tree_view_model();
    test_text_edit_properties();
    test_code_edit_properties();
    test_screen_snapshot();
    test_theme_factory();
    test_advanced_graphics();
    test_portable_fonts();
    test_image_io();
    test_grid_layout_placement();
    test_grid_layout_margins();
    test_grid_layout_auto_placement();
    test_grid_layout_nested_ownership();
    test_absolute_layout_preserves_bounds();
    test_layout_pass_scheduling();
    test_layout_child_removal();
    return failure_count == 0 ? 0 : 1;
}

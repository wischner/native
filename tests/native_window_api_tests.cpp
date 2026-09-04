//
// Tests backend-neutral window properties and hierarchy behavior.
// Native resources are not created, so the test requires no display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__HAIKU__)
#include <ScrollBar.h>
#endif

#include <native.h>

namespace
{
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::file_dialog &>()
                               .get_path()),
                  const std::filesystem::path &>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::code_edit &>()
                               .get_path()),
                  const std::filesystem::path &>);
    static_assert(std::is_same_v<
                  decltype(native::font_description{}.path),
                  std::filesystem::path>);
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
    static_assert(std::is_base_of_v<native::file_dialog,
                                    native::directory_dialog>);
    static_assert(std::is_same_v<native::list_box, native::list>);
    static_assert(std::is_base_of_v<native::wnd, native::combo_box>);
    static_assert(std::is_base_of_v<native::wnd, native::tab_view>);
    static_assert(!std::is_copy_constructible_v<native::tab_view>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::tab_view &>()
                               .get_tab_placement()),
                  native::tab_placement>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::tab_view &>()
                               .get_page_frame_visible()),
                  bool>);
    static_assert(std::is_base_of_v<native::wnd, native::split_view>);
    static_assert(std::is_base_of_v<native::wnd, native::panel>);
    static_assert(std::is_base_of_v<native::wnd, native::canvas>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::wnd &>()
                               .get_cursor()),
                  native::mouse_cursor>);
    static_assert(std::is_base_of_v<native::custom_control,
                                    native::canvas>);
    static_assert(std::is_base_of_v<native::collection_view,
                                    native::accordion>);
    static_assert(std::is_base_of_v<native::collection_view,
                                    native::icon_view>);
    static_assert(std::is_base_of_v<native::collection_view,
                                    native::tree_view>);
    static_assert(std::is_base_of_v<native::collection_view,
                                    native::table_view>);
    static_assert(!std::is_abstract_v<native::panel>);
    static_assert(!std::is_abstract_v<native::canvas>);
    // A container and a drawing surface are unrelated siblings; neither
    // may be used where the other's contract is expected.
    static_assert(!std::is_base_of_v<native::panel, native::canvas>);
    static_assert(!std::is_base_of_v<native::canvas, native::panel>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::canvas &>()
                               .get_horizontal_scrollbar_policy()),
                  native::scrollbar_policy>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const native::table_view &>()
                               .get_vertical_scrollbar_policy()),
                  native::scrollbar_policy>);
    static_assert(!std::is_copy_constructible_v<native::split_view>);
    static_assert(std::is_constructible_v<
                  native::combo_box,
                  const std::vector<std::string> &,
                  native::combo_box_style,
                  const native::point &,
                  const native::size &>);
    static_assert(std::is_base_of_v<native::wnd, native::text_edit>);
    static_assert(
        std::is_base_of_v<native::text_edit, native::code_edit>);
    static_assert(!std::is_copy_constructible_v<native::clipboard>);
    static_assert(std::is_move_constructible_v<native::clipboard>);

    int failure_count = 0;

    bool same_rect(const native::rect &left,
                   const native::rect &right) {
        return left.p.x == right.p.x && left.p.y == right.p.y &&
               left.d.w == right.d.w && left.d.h == right.d.h;
    }

    class simulated_page final : public native::wnd
    {
    public:
        using native::wnd::wnd;

        void create_native() override {}

        void destroy_native() override {}

        void show_native() override {}
    };

    class simulated_app_window final : public native::app_wnd
    {
    public:
        simulated_app_window()
            : app_wnd("simulated", 0, 0, 640, 480) {}

        ~simulated_app_window() override {
            destroy();
        }

        int cursor_applications = 0;

    protected:
        void create_native() override {}
        void destroy_native() override {}
        void show_native() override {}
        void apply_title() override {}
        void apply_position() override {}
        void apply_dimensions() override {}
        void apply_bounds() override {}
        void apply_parent() override {}
        void apply_cursor() override {
            ++cursor_applications;
        }
    };

    class simulated_tab_view final : public native::tab_view
    {
    public:
        using native::tab_view::tab_view;

        ~simulated_tab_view() override {
            destroy();
        }

        void create_native() override {
            refresh();
        }

        void destroy_native() override {
            destroy_children();
        }

        void show_native() override {}

        simulated_tab_view &preserve_pages() {
            configure_page_host(true, true);
            return *this;
        }

        int item_applications = 0;
        int selection_applications = 0;

    protected:
        void apply_items() override {
            ++item_applications;
        }

        void apply_selected_index() override {
            ++selection_applications;
        }
    };

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

        native::gpx &draw_line(native::point from,
                               native::point to) override {
            painted = true;
            lines.emplace_back(from, to);
            line_inks.push_back(get_ink());
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
        std::vector<std::pair<native::point, native::point>> lines;
        std::vector<native::rgba> line_inks;
    };

    class extensible_app_window final : public native::app_wnd
    {
    public:
        using native::app_wnd::app_wnd;

        void on_native_mouse_click(native::mouse_event event) override {
            ++mouse_hooks;
            native::app_wnd::on_native_mouse_click(event);
        }

        void on_native_menu(int command) override {
            ++menu_hooks;
            native::app_wnd::on_native_menu(command);
        }

        int mouse_hooks = 0;
        int menu_hooks = 0;
    };

    class extensible_button final : public native::button
    {
    public:
        using native::button::button;

        void on_native_click() override {
            ++click_hooks;
            native::button::on_native_click();
        }

        void paint(recording_gpx &graphics) {
            auto appearance = native::theme::create(graphics);
            draw_control(graphics,
                         *appearance,
                         native::rect(0, 0, 80, 24),
                         native::theme::state{});
        }

        int click_hooks = 0;
        int paint_hooks = 0;
        int background_hooks = 0;
        int border_hooks = 0;
        int text_hooks = 0;
        int focus_hooks = 0;

    protected:
        void draw_control(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++paint_hooks;
            native::button::draw_control(
                graphics, appearance, bounds, state);
        }

        void draw_background(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++background_hooks;
            native::button::draw_background(
                graphics, appearance, bounds, state);
        }

        void draw_border(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++border_hooks;
            native::button::draw_border(
                graphics, appearance, bounds, state);
        }

        void draw_text(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++text_hooks;
            native::button::draw_text(
                graphics, appearance, bounds, state);
        }

        void draw_focus(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++focus_hooks;
            native::button::draw_focus(
                graphics, appearance, bounds, state);
        }
    };

    class extensible_table final : public native::table_view
    {
    public:
        using native::table_view::table_view;

        void on_native_selection(
            const std::vector<native::table_row_id> &rows) override {
            ++selection_hooks;
            native::table_view::on_native_selection(rows);
        }

        void paint_cell(recording_gpx &graphics) {
            auto appearance = native::theme::create(graphics);
            native::table_column column;
            column.id = 1;
            native::table_cell cell;
            cell.text = "Cell";
            draw_cell_content(graphics,
                              *appearance,
                              1,
                              0,
                              column,
                              cell,
                              native::rect(0, 0, 80, 24),
                              native::theme::state{});
        }

        void paint_border(recording_gpx &graphics) {
            auto appearance = native::theme::create(graphics);
            draw_border(graphics,
                        *appearance,
                        native::rect(0, 0, 80, 24),
                        native::theme::state{});
        }

        int selection_hooks = 0;
        int cell_hooks = 0;
        int border_hooks = 0;

    protected:
        void draw_cell_content(
            native::gpx &graphics,
            native::theme &appearance,
            native::table_row_id row,
            std::size_t model_row,
            const native::table_column &column,
            const native::table_cell &cell,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++cell_hooks;
            native::table_view::draw_cell_content(
                graphics,
                appearance,
                row,
                model_row,
                column,
                cell,
                bounds,
                state);
        }

        void draw_border(
            native::gpx &graphics,
            native::theme &appearance,
            const native::rect &bounds,
            const native::theme::state &state) override {
            ++border_hooks;
            native::table_view::draw_border(
                graphics, appearance, bounds, state);
        }
    };

    class extensible_code_edit final : public native::code_edit
    {
    public:
        using native::code_edit::code_edit;

        void complete(const native::completion_item &item) {
            on_native_complete(item);
        }

        int completion_hooks = 0;

    protected:
        void on_native_complete(
            const native::completion_item &item) override {
            ++completion_hooks;
            native::code_edit::on_native_complete(item);
        }
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
        expect(window.get_cursor() == native::mouse_cursor::arrow,
               "windows default to the arrow cursor");
        native::wnd *chained =
            &window.set_cursor(native::mouse_cursor::crosshair);
        expect(chained == &window &&
                   window.get_cursor() ==
                       native::mouse_cursor::crosshair,
               "window cursor changes cache before creation and chain");
        window.set_cursor(native::mouse_cursor::resize_horizontal);
        expect(window.get_cursor() ==
                   native::mouse_cursor::resize_horizontal,
               "window caches the horizontal resize cursor");
        window.set_cursor(native::mouse_cursor::resize_vertical);
        expect(window.get_cursor() ==
                   native::mouse_cursor::resize_vertical,
               "window caches the vertical resize cursor");
        window.set_cursor(
            native::mouse_cursor::resize_northwest_southeast);
        expect(window.get_cursor() ==
                   native::mouse_cursor::resize_northwest_southeast,
               "window caches the northwest-southeast resize cursor");
        window.set_cursor(
            native::mouse_cursor::resize_northeast_southwest);
        expect(window.get_cursor() ==
                   native::mouse_cursor::resize_northeast_southwest,
               "window caches the northeast-southwest resize cursor");
        expect(window.get_native_title_visible(),
               "ordinary application windows retain native titles");
        expect(!window.get_created(),
               "construction does not create a window");

        window.on_native_mouse_move(
            native::point(5, 6), native::point(105, 206));
        expect(window.get_mouse_screen_position().x == 105 &&
                   window.get_mouse_screen_position().y == 206,
               "native pointer events retain exact screen coordinates");

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

    // Verify cursor state is applied at native lifecycle boundaries.
    void test_cursor_property() {
        simulated_app_window window;
        window.set_cursor(native::mouse_cursor::ibeam);
        expect(window.cursor_applications == 0,
               "an uncreated cursor change stays portable");

        window.create();
        expect(window.cursor_applications == 1,
               "creation applies the cached cursor");
        window.set_cursor(native::mouse_cursor::ibeam);
        expect(window.cursor_applications == 1,
               "an unchanged live cursor is not reapplied");
        window.set_cursor(native::mouse_cursor::resize_horizontal);
        expect(window.cursor_applications == 2,
               "a live resize-cursor change reaches the backend");
        window.show();
        expect(window.cursor_applications == 3,
               "show reapplies the cursor after realization");
    }

    // Verify that portable menu labels retain explicit presentation metadata
    // without exposing markup to a backend.
    void test_menu_label_metadata() {
        native::main_menu menu;
        menu << "&File"
             << (native::menu_items("&Open...\tCtrl+O")
                     << native::menu_separator
                     << std::make_pair(
                            42,
                            std::string("Save && E&xit\tAlt+F4")))
             << "Help"
             << native::menu_items("About");

        const auto &tops = menu.tops();
        expect(tops.size() == 2 &&
                   tops[0].title == "File" &&
                   tops[0].mnemonic_index == 0 &&
                   tops[1].title == "Help" &&
                   tops[1].mnemonic_index == 0,
               "menu titles strip mnemonic markup and retain access keys");
        expect(tops[0].items.size() == 3 &&
                   tops[0].items[0].label == "Open..." &&
                   tops[0].items[0].mnemonic_index == 0 &&
                   tops[0].items[0].shortcut == "Ctrl+O",
               "menu items retain explicit mnemonic and shortcut metadata");
        expect(tops[0].items[1].separator &&
                   tops[0].items[1].id == 0 &&
                   tops[0].items[1].label.empty(),
               "menu separators are non-command structural entries");
        expect(tops[0].items[2].id == 42 &&
                   tops[0].items[2].label == "Save & Exit" &&
                   tops[0].items[2].mnemonic_index == 8 &&
                   tops[0].items[2].shortcut == "Alt+F4",
               "escaped ampersands remain visible beside explicit mnemonics");
        expect(tops[1].items.size() == 1 &&
                   tops[1].items[0].mnemonic_index == 0 &&
                   tops[1].items[0].shortcut.empty(),
               "unmarked menu labels receive a first-character mnemonic");
    }

    // Verify tab ordering, borrowed-page ownership, and event semantics.
    void test_tab_view_model() {
        native::button first("First", 0, 0, 10, 10);
        native::button second("Second", 0, 0, 10, 10);
        native::tab_view tabs(0, 0, 300, 180);
        tabs.add_item("General", first);
        tabs.add_item("Advanced", second).set_enabled(false);
        int changes = 0;
        tabs.on_selection_change.connect([&](int) {
            ++changes;
            return true;
        });

        expect(tabs.get_item_count() == 2 &&
                   tabs.get_selected_index() == 0 &&
                   tabs.get_tab_placement() ==
                       native::tab_placement::top &&
                   tabs.get_page_frame_visible() &&
                   first.get_parent() == &tabs &&
                   second.get_parent() == &tabs,
               "tab_view borrows ordered page windows and selects the first");
        expect(tabs.get_content_bounds().p.y > 0 &&
                   tabs.get_content_bounds().d.h < tabs.get_dimensions().h,
               "tab_view reserves a tab strip above its page bounds");

        const native::rect top_tabs = tabs.get_tab_bounds(0);
        const native::rect top_content = tabs.get_content_bounds();
        expect(top_tabs.p.x == top_content.p.x,
               "framed top tabs align with the page border");
        tabs.set_page_frame_visible(false);
        const native::rect flush_top_tabs = tabs.get_tab_bounds(0);
        const native::rect flush_top_content =
            tabs.get_content_bounds();
        expect(!tabs.get_page_frame_visible() &&
                   tabs.get_selected_index() == 0 &&
                   flush_top_tabs.p.x == 0 &&
                   flush_top_content.p.x == 0 &&
                   flush_top_content.d.w == tabs.get_dimensions().w &&
                   flush_top_content.p.y == flush_top_tabs.y2() &&
                   flush_top_content.y2() ==
                       tabs.get_dimensions().h &&
                   changes == 0,
               "strip-only tabs use flush page bounds and change silently");
        recording_gpx flush_graphics;
        flush_graphics.set_clip(native::rect(0, 0, 300, 180));
        tabs.on_native_paint(native::wnd_paint_event(
            native::rect(0, 0, 300, 180), flush_graphics));
        const bool has_full_width_separator = std::any_of(
            flush_graphics.lines.begin(),
            flush_graphics.lines.end(),
            [](const auto &line) {
                return line.first.x == 0 && line.second.x == 299 &&
                       line.first.y == line.second.y;
            });
        expect(has_full_width_separator,
               "strip-only top tabs draw one full-width separator");
        tabs.set_page_frame_visible(true);
        tabs.set_tab_placement(native::tab_placement::bottom);
        const native::rect bottom_tabs = tabs.get_tab_bounds(0);
        const native::rect bottom_content = tabs.get_content_bounds();
        expect(bottom_tabs.p.y > top_tabs.p.y &&
                   bottom_content.p.y < top_content.p.y &&
                   bottom_content.y2() <= bottom_tabs.p.y + 1 &&
                   bottom_tabs.p.x == bottom_content.p.x,
               "bottom tab labels occupy the edge below their content");
        expect(tabs.get_item_count() == 2 &&
                   tabs.get_selected_index() == 0 &&
                   first.get_parent() == &tabs,
               "placement set before creation preserves tabs and selection");

        tabs.set_tab_placement(native::tab_placement::left);
        const native::rect left_tabs = tabs.get_tab_bounds(0);
        const native::rect left_content = tabs.get_content_bounds();
        expect(left_tabs.p.x == 0 &&
                   left_content.p.x + 1 >= left_tabs.x2() &&
                   left_tabs.p.y == left_content.p.y &&
                   left_tabs.d.h > left_tabs.d.w,
            "left tabs overlap the page edge without a gap");
        tabs.set_tab_placement(native::tab_placement::right);
        const native::rect right_tabs = tabs.get_tab_bounds(0);
        const native::rect right_content = tabs.get_content_bounds();
        expect(right_tabs.p.x + 1 >= right_content.x2() &&
                   right_tabs.p.y == right_content.p.y &&
                   right_tabs.d.h > right_tabs.d.w,
            "right tabs overlap the page edge without a gap");
        recording_gpx side_graphics;
        side_graphics.set_clip(native::rect(0, 0, 300, 180));
        tabs.on_native_paint(native::wnd_paint_event(
            native::rect(0, 0, 300, 180), side_graphics));
        expect(side_graphics.image_count == 2,
               "side tab labels use the portable rotated-text path");
        tabs.set_tab_placement(native::tab_placement::bottom);

        tabs.set_selected_index(1);
        expect(tabs.get_selected_index() == 1 && changes == 0,
               "programmatic tab selection is silent");
        tabs.set_selected_index(0);
        tabs.on_native_selection(1);
        expect(tabs.get_selected_index() == 0 && changes == 0,
               "disabled tabs reject user-originated selection");
        tabs.get_item(1).set_enabled(true);
        tabs.on_native_selection(1);
        expect(tabs.get_selected_index() == 1 && changes == 1,
               "enabled user tab selection emits once");

        tabs.remove_item(1);
        expect(tabs.get_item_count() == 1 &&
                   tabs.get_selected_index() == 0 &&
                   second.get_parent() == nullptr,
               "removing a tab detaches its borrowed page");
        tabs.clear_items();
        expect(tabs.get_selected_index() == -1 &&
                   first.get_parent() == nullptr,
               "clearing tabs detaches all pages and clears selection");

        simulated_app_window lifecycle_root;
        lifecycle_root.create();
        simulated_page live_first(0, 0, 10, 10);
        simulated_page live_second(0, 0, 10, 10);
        simulated_tab_view live_tabs(0, 0, 320, 200);
        live_tabs.set_parent(&lifecycle_root);
        live_tabs.add_item("Top", live_first);
        live_tabs.add_item("Other", live_second);
        live_tabs.set_selected_index(1);
        int placement_events = 0;
        live_tabs.on_selection_change.connect([&](int) {
            ++placement_events;
            return false;
        });
        live_tabs.create();
        expect(live_tabs.get_created() && live_second.get_created() &&
                   !live_first.get_created(),
               "created tabs materialize only their selected borrowed page");
        live_tabs.set_page_frame_visible(false);
        expect(!live_tabs.get_page_frame_visible() &&
                   live_tabs.get_selected_index() == 1 &&
                   live_tabs.get_item_count() == 2 &&
                   live_second.get_created() &&
                   same_rect(live_second.get_bounds(),
                             live_tabs.get_content_bounds()) &&
                   placement_events == 0,
               "page-frame changes after creation preserve page state and "
               "emit no selection signal");
        const int selected_before = live_tabs.get_selected_index();
        const int applications_before =
            live_tabs.selection_applications;
        live_tabs.set_tab_placement(native::tab_placement::bottom);
        expect(live_tabs.get_selected_index() == selected_before &&
                   live_tabs.get_item_count() == 2 &&
                   live_second.get_created() && placement_events == 0,
               "placement set after creation preserves pages and is silent");
        live_tabs.set_dimensions({360, 220});
        expect(same_rect(live_second.get_bounds(),
                         live_tabs.get_content_bounds()) &&
                   live_tabs.get_content_bounds().y2() <=
                       live_tabs.get_tab_bounds(0).p.y,
               "resizing preserves bottom placement and page layout");
        live_tabs.on_native_selection(0);
        expect(live_tabs.get_selected_index() == 0 &&
                   live_first.get_created() && !live_second.get_created() &&
                   live_tabs.selection_applications ==
                       applications_before + 2 &&
                   placement_events == 1,
               "user tab changes apply the backend page and emit once");
        live_tabs.set_selected_index(1);
        live_tabs.set_tab_placement(native::tab_placement::left);
        expect(live_tabs.get_selected_index() == selected_before &&
                   live_tabs.get_content_bounds().p.x > 0 &&
                   placement_events == 1,
               "left placement after creation is silent and preserves "
               "selection");
        live_tabs.set_tab_placement(native::tab_placement::right);
        const native::rect live_content =
            live_tabs.get_content_bounds();
        expect(same_rect(live_second.get_bounds(), live_content),
               "selected borrowed content receives side content bounds");
        live_tabs.set_dimensions({420, 260});
        expect(same_rect(live_second.get_bounds(),
                         live_tabs.get_content_bounds()) &&
                   live_tabs.get_tab_bounds(0).p.x + 1 >=
                       live_tabs.get_content_bounds().x2(),
               "resizing preserves side placement and selected page layout");
        live_tabs.set_tab_placement(native::tab_placement::top);
        expect(live_tabs.get_selected_index() == selected_before &&
                   placement_events == 1 &&
                   same_rect(live_second.get_bounds(),
                             live_tabs.get_content_bounds()),
               "returning to top placement remains silent and preserves "
               "selection");
        live_tabs.set_page_frame_visible(true);
        expect(live_tabs.get_page_frame_visible() &&
                   live_tabs.get_selected_index() == selected_before &&
                   live_second.get_created() &&
                   same_rect(live_second.get_bounds(),
                             live_tabs.get_content_bounds()) &&
                   placement_events == 1,
               "restoring the page frame preserves selected borrowed "
               "content without a selection signal");

        simulated_page retained_first(0, 0, 10, 10);
        simulated_page retained_second(0, 0, 10, 10);
        simulated_tab_view retained_tabs(0, 0, 240, 140);
        retained_tabs.set_parent(&lifecycle_root);
        retained_tabs.preserve_pages()
            .add_item("First", retained_first);
        retained_tabs.add_item("Second", retained_second);
        retained_tabs.create();
        retained_tabs.on_native_selection(1);
        expect(retained_first.get_created() &&
                   retained_second.get_created(),
               "independent page hosts preserve previously visited content");
        retained_tabs.on_native_selection(0);
        expect(retained_first.get_created() &&
                   retained_second.get_created(),
               "repeated side-page switching retains borrowed content");
    }

    // Verify native callbacks and paint stages dispatch virtually once,
    // while base calls retain state updates and public signals.
    void test_control_extension_hooks() {
        extensible_app_window window(
            "Hooks", native::rect(0, 0, 320, 200));
        int mouse_signals = 0;
        int menu_signals = 0;
        window.on_mouse_click.connect(
            [&](native::mouse_event) {
                ++mouse_signals;
                return false;
            });
        window.on_menu.connect([&](int command) {
            if (command == 42)
                ++menu_signals;
            return false;
        });
        window.on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            native::mouse_action::press,
            native::point(3, 4)));
        window.on_native_menu(42);
        expect(window.mouse_hooks == 1 && mouse_signals == 1 &&
                   window.menu_hooks == 1 && menu_signals == 1,
               "derived window event hooks can extend then call base");

        extensible_button button("Hooked");
        int click_signals = 0;
        button.on_click.connect([&]() {
            ++click_signals;
            return false;
        });
        button.on_native_click();
        recording_gpx graphics;
        button.paint(graphics);
        expect(button.click_hooks == 1 && click_signals == 1 &&
                   button.paint_hooks == 1 &&
                   button.background_hooks == 1 &&
                   button.border_hooks == 1 &&
                   button.text_hooks == 1 &&
                   button.focus_hooks == 1 && graphics.painted,
               "button behavior and default painting are virtual stages");

        extensible_table table;
        int selection_signals = 0;
        table.on_selection_change.connect(
            [&](const std::vector<native::table_row_id> &) {
                ++selection_signals;
                return false;
            });
        table.on_native_selection({});
        graphics.painted = false;
        table.paint_cell(graphics);
        table.paint_border(graphics);
        expect(table.selection_hooks == 1 &&
                   selection_signals == 0 &&
                   table.cell_hooks == 1 &&
                   table.border_hooks == 1 && graphics.painted,
               "table hooks own cell and final-border painting while "
               "preserving base behavior");

        extensible_code_edit editor;
        int completion_signals = 0;
        editor.on_complete.connect(
            [&](native::completion_item) {
                ++completion_signals;
                return false;
            });
        editor.complete(
            native::completion_item{"value", "value", "detail"});
        expect(editor.completion_hooks == 1 &&
                   completion_signals == 1,
               "code editor semantic events extend then call base");
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
            "Owner", native::rect(40, 30, 320, 200));
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

        modeless->center_to_parent();
        expect(modeless->get_position().x == 110 &&
                   modeless->get_position().y == 10,
               "an owned window centers relative to its owner");
        owner->center_to_parent();
        expect(owner->get_position().x == 40 &&
                   owner->get_position().y == 30,
               "centering a main window without an owner is a no-op");

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

    void test_input_and_window_chrome() {
        native::combo_box combo(
            {"First", "Second", "Third"},
            native::combo_box_style::editable);
        int selections = 0;
        int text_changes = 0;
        combo.on_selection_change.connect(
            [&](int) { ++selections; return true; });
        combo.on_text_change.connect(
            [&](std::string) { ++text_changes; return true; });
        combo.set_selected_index(1);
        expect(combo.get_selected_index() == 1 &&
                   combo.get_text() == "Second" &&
                   selections == 0 && text_changes == 0,
               "programmatic combo selection is cached and silent");
        combo.on_native_selection(2);
        combo.on_native_text("Custom");
        expect(combo.get_selected_index() == -1 &&
                   combo.get_text() == "Custom" &&
                   selections == 1 && text_changes == 2,
               "native combo selection and editing emit stable events");
        combo.on_native_text("Custom");
        combo.on_native_selection(-1);
        combo.on_native_selection(-1);
        expect(selections == 1 && text_changes == 3,
               "combo change events ignore repeated native state");

        combo.set_items({"Alpha", "Beta", "Gamma"});
        combo.set_selected_index(1);
        combo.set_items({"One", "Two", "Three"});
        expect(combo.get_selected_index() == 1 &&
                   combo.get_text() == "Two",
               "combo text follows a retained selection after "
               "replacement");

        native::app_wnd window("Chrome", 0, 0, 300, 200);
        native::ruler horizontal(window, native::window_edge::top, 20);
        native::ruler vertical(window, native::window_edge::left, 30);
        native::status_bar status(window, 22);
        status.set_parts({{"Ready", 0}, {"Ln 1", 70}});
        const native::rect client = window.get_client_bounds();
        expect(client.p.x == 30 && client.p.y == 20 &&
                   client.d.w == 270 && client.d.h == 158,
               "non-client rulers and status reserve stacked edge space");
        expect(horizontal.get_bounds().p.x == 30 &&
                   horizontal.get_bounds().d.w == 270 &&
                   vertical.get_bounds().y2() == 178 &&
                   status.get_bounds().p.x == 0 &&
                   status.get_bounds().p.y == 178 &&
                   status.get_bounds().d.w == 300,
               "non-client element bounds meet at the client corners");

        expect(!horizontal.get_edge_visible() &&
                   !vertical.get_edge_visible(),
               "ruler edge rules are hidden by default");
        horizontal.set_edge_visible(true);
        vertical.set_edge_visible(true);
        recording_gpx ruler_graphics;
        ruler_graphics.set_clip(native::rect(0, 0, 300, 200));
        window.on_native_paint(native::wnd_paint_event(
            native::rect(0, 0, 300, 200), ruler_graphics));
        const auto same_color = [](native::rgba left,
                                   native::rgba right) {
            return left.r == right.r && left.g == right.g &&
                   left.b == right.b && left.a == right.a;
        };
        const auto line_index = [&](native::point from,
                                    native::point to) {
            for (std::size_t index = 0;
                 index < ruler_graphics.lines.size(); ++index) {
                if (ruler_graphics.lines[index].first.x == from.x &&
                    ruler_graphics.lines[index].first.y == from.y &&
                    ruler_graphics.lines[index].second.x == to.x &&
                    ruler_graphics.lines[index].second.y == to.y) {
                    return index;
                }
            }
            return ruler_graphics.lines.size();
        };
        const std::size_t horizontal_tick =
            line_index({30, 19}, {30, 10});
        const std::size_t horizontal_edge =
            line_index({30, 19}, {299, 19});
        const std::size_t vertical_tick =
            line_index({29, 20}, {20, 20});
        const std::size_t vertical_edge =
            line_index({29, 20}, {29, 177});
        expect(horizontal_tick < ruler_graphics.lines.size() &&
                   horizontal_edge < ruler_graphics.lines.size() &&
                   vertical_tick < ruler_graphics.lines.size() &&
                   vertical_edge < ruler_graphics.lines.size() &&
                   same_color(ruler_graphics.line_inks[horizontal_tick],
                              ruler_graphics.line_inks[horizontal_edge]) &&
                   same_color(ruler_graphics.line_inks[vertical_tick],
                              ruler_graphics.line_inks[vertical_edge]),
               "rulers draw bottom and right edges in the tick color");

        double tracked = -1.0;
        horizontal.set_units_per_pixel(2.0).set_track_mouse(true);
        horizontal.on_tracking.connect(
            [&](double value) { tracked = value; return true; });
        window.on_native_mouse_move({80, 80});
        expect(tracked == 100.0 &&
                   horizontal.get_tracked_value().value_or(-1.0) == 100.0,
               "ruler tracking converts host pointer position to units");

        horizontal.set_edge(native::window_edge::right);
        expect(horizontal.get_orientation() ==
                   native::ruler_orientation::vertical,
               "ruler orientation follows its mutable edge");
        horizontal.set_edge(native::window_edge::top);

        vertical.set_visible(false);
        expect(window.get_client_bounds().p.x == 0 &&
                   window.get_client_bounds().d.w == 300,
               "hidden non-client elements release their reserved space");
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
                   first_content.get_parent() == &sections &&
                   sections.get_border_visible(),
               "accordion expands its first borrowed section");
        sections.set_border_visible(false);
        expect(!sections.get_border_visible(),
               "accordion outer border can be hidden");
        sections.set_border_visible(true);
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
        expect(tree.get_border_visible(),
               "tree outer border is visible by default");
        tree.set_border_visible(false);
        expect(!tree.get_border_visible(),
               "tree outer border can be hidden");
        tree.set_border_visible(true);
        tree.set_presentation(
            native::tree_view_presentation::three_dimensional);
        expect(tree.get_presentation() ==
                   native::tree_view_presentation::three_dimensional,
               "tree presentation is cached before native creation");
        tree.set_presentation(native::tree_view_presentation::native);
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
        expect(single.get_cursor() == native::mouse_cursor::ibeam,
               "text editors select the I-beam cursor by default");
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
        expect(metrics.button_height > 0 &&
                   metrics.menu_bar_height > 0 &&
                   metrics.check_height > 0 &&
                   metrics.radio_height > 0 &&
                   metrics.text_edit_height > 0 &&
                   metrics.list_item_height > 0 &&
                   metrics.table_row_height > 0 &&
                   metrics.header_height > 0 &&
                   metrics.disclosure_size > 0 &&
                   metrics.sort_indicator_size > 0 &&
                   metrics.caption_button_size > 0 &&
                   metrics.tab_height > 0 &&
                   metrics.icon_view_min_item_width > 0 &&
                   metrics.scrollbar_extent > 0 &&
                   metrics.status_bar_height > 0 &&
                   metrics.ruler_extent > 0,
               "theme reports usable control metrics");
#if defined(__HAIKU__)
        expect(metrics.scrollbar_extent ==
                       static_cast<int>(B_H_SCROLL_BAR_HEIGHT) &&
                   metrics.status_bar_height == 18,
               "Haiku status bars align with the system resize handle");
#endif
        expect(painter->get_button_height() == metrics.button_height &&
                   painter->get_menu_bar_height() ==
                       metrics.menu_bar_height &&
                   painter->get_menu_item_height() ==
                       metrics.menu_item_height &&
                   painter->get_popup_width() == metrics.popup_width &&
                   painter->get_text_padding_x() ==
                       metrics.text_padding_x &&
                   painter->get_text_edit_height() ==
                       metrics.text_edit_height &&
                   painter->get_check_height() == metrics.check_height &&
                   painter->get_radio_height() == metrics.radio_height &&
                   painter->get_list_item_height() ==
                       metrics.list_item_height &&
                   painter->get_table_row_height() ==
                       metrics.table_row_height &&
                   painter->get_disclosure_size() ==
                       metrics.disclosure_size &&
                   painter->get_sort_indicator_size() ==
                       metrics.sort_indicator_size &&
                   painter->get_caption_button_size() ==
                       metrics.caption_button_size &&
                   painter->get_header_height() ==
                       metrics.header_height &&
                   painter->get_tab_height() == metrics.tab_height &&
                   painter->get_separator_extent() ==
                       metrics.separator_extent &&
                   painter->get_scrollbar_extent() ==
                       metrics.scrollbar_extent &&
                   painter->get_scrollbar_min_thumb() ==
                       metrics.scrollbar_min_thumb &&
                   painter->get_status_bar_height() ==
                       metrics.status_bar_height &&
                   painter->get_ruler_extent() ==
                       metrics.ruler_extent,
               "theme exposes named getters for shape metrics");
        expect(painter->get_table_outer_border_extent() ==
                       metrics.table_outer_border_extent &&
                   painter->get_focus_inset() == metrics.focus_inset &&
                   painter->get_tree_lines_visible() ==
                       metrics.tree_lines_visible &&
                   painter->get_tree_row_height() ==
                       metrics.tree_row_height &&
                   painter->get_tree_horizontal_padding() ==
                       metrics.tree_horizontal_padding &&
                   painter->get_tree_indent_width() ==
                       metrics.tree_indent_width &&
                   painter->get_tree_item_gap() ==
                       metrics.tree_item_gap &&
                   painter->get_tree_icon_vertical_padding() ==
                       metrics.tree_icon_vertical_padding &&
                   painter->get_header_padding_x() ==
                       metrics.header_padding_x &&
                   painter->get_header_gap() == metrics.header_gap &&
                   painter->get_icon_view_padding_x() ==
                       metrics.icon_view_padding_x &&
                   painter->get_icon_view_padding_y() ==
                       metrics.icon_view_padding_y &&
                   painter->get_icon_view_item_gap_x() ==
                       metrics.icon_view_item_gap_x &&
                   painter->get_icon_view_item_gap_y() ==
                       metrics.icon_view_item_gap_y &&
                   painter->get_icon_view_label_gap() ==
                       metrics.icon_view_label_gap &&
                   painter->get_icon_view_min_item_width() ==
                       metrics.icon_view_min_item_width &&
                   painter->get_table_fill_last_column() ==
                       metrics.table_fill_last_column,
               "theme metric getters cover collection and frame shapes");

        const native::size horizontal_scrollbar =
            painter->get_scrollbar_size(
                native::scrollbar_orientation::horizontal, 120);
        const native::size vertical_scrollbar =
            painter->get_scrollbar_size(
                native::scrollbar_orientation::vertical, 90);
        expect(horizontal_scrollbar.w == 120 &&
                   horizontal_scrollbar.h == metrics.scrollbar_extent &&
                   vertical_scrollbar.w == metrics.scrollbar_extent &&
                   vertical_scrollbar.h == 90 &&
                   painter->get_separator_size(
                       native::separator_orientation::horizontal, 80)
                           .h == metrics.separator_extent &&
                   painter->get_status_bar_size(200).h ==
                       metrics.status_bar_height &&
                   painter->get_ruler_size(
                       native::ruler_orientation::vertical, 70)
                           .w == metrics.ruler_extent,
               "theme sizes linear shapes on the correct axis");

        const native::rgba colors[] = {
            painter->get_button_background_color(),
            painter->get_button_border_color(),
            painter->get_button_highlight_color(),
            painter->get_button_shadow_color(),
            painter->get_button_foreground_color(),
            painter->get_button_disabled_foreground_color(),
            painter->get_button_hot_background_color(),
            painter->get_button_hot_foreground_color(),
            painter->get_button_pressed_background_color(),
            painter->get_button_pressed_foreground_color(),
            painter->get_menu_bar_background_color(),
            painter->get_menu_bar_top_color(),
            painter->get_menu_bar_bottom_color(),
            painter->get_menu_foreground_color(),
            painter->get_menu_disabled_foreground_color(),
            painter->get_menu_hot_background_color(),
            painter->get_menu_hot_foreground_color(),
            painter->get_menu_popup_background_color(),
            painter->get_menu_popup_border_color(),
            painter->get_content_background_color(),
            painter->get_content_alternate_background_color(),
            painter->get_content_foreground_color(),
            painter->get_selection_background_color(),
            painter->get_selection_foreground_color(),
            painter->get_inactive_selection_background_color(),
            painter->get_inactive_selection_foreground_color(),
            painter->get_separator_color(),
            painter->get_focus_color()};
        expect(std::all_of(
                   std::begin(colors),
                   std::end(colors),
                   [](const native::rgba &color) {
                       return color.a == 255;
                   }),
               "theme color getters always return opaque colors");

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
        painter->draw_sort_indicator(
            native::rect(0, 0, 12, 12),
            native::sort_indicator_state::ascending,
            selected);
        painter->draw_caption_button(
            native::rect(0, 0, 14, 14),
            native::caption_button_kind::close,
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

        const std::filesystem::path path =
            "native-image-codec-test.png";
        source.save(path);
        native::img file_copy = native::img::load(path);
        std::filesystem::remove(path);
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


    // Simulate a structural container without a backend resource.
    class simulated_panel final : public native::panel
    {
    public:
        using native::panel::panel;

        ~simulated_panel() override {
            destroy();
        }

        void create_native() override {}

        void destroy_native() override {}

        void show_native() override {}
    };

    // Simulate a drawing surface without a backend resource.
    class simulated_canvas final : public native::canvas
    {
    public:
        using native::canvas::canvas;

        ~simulated_canvas() override {
            destroy();
        }

        void create_native() override {}

        void destroy_native() override {}

        void show_native() override {}
    };

    // Counts its own backend lifecycle without owning a real resource.
    class counted_child final : public native::wnd
    {
    public:
        using native::wnd::wnd;

        void create_native() override {
            ++creates;
        }

        void destroy_native() override {
            ++destroys;
        }

        void show_native() override {}

        int creates = 0;
        int destroys = 0;
    };

    // Reject creation after simulating a backend allocation failure.
    class failing_child final : public native::wnd
    {
    public:
        using native::wnd::wnd;

        void create_native() override {
            ++attempts;
            throw std::runtime_error("simulated creation failure");
        }

        void destroy_native() override {}
        void show_native() override {}

        int attempts = 0;
    };

    // Verify panel construction, hierarchy, layout, and destruction.
    void test_panel_container() {
        expect(bounds_are(simulated_panel(), 0, 0, 320, 240),
               "a default panel keeps the documented bounds");

        const simulated_panel scalar(5, 6, 70, 80);
        expect(bounds_are(scalar, 5, 6, 70, 80),
               "the scalar panel constructor preserves bounds");
        const simulated_panel split(native::point(7, 8),
                                    native::size(90, 100));
        expect(bounds_are(split, 7, 8, 90, 100),
               "the position/size panel constructor preserves bounds");
        const simulated_panel whole(native::rect(9, 10, 110, 120));
        expect(bounds_are(whole, 9, 10, 110, 120),
               "the rect panel constructor preserves bounds");

        // A child assigned before the layout and one assigned after it
        // must each be registered exactly once.
        simulated_panel host(native::rect(0, 0, 200, 100));
        int host_create_events = 0;
        host.on_wnd_create.connect([&host_create_events]() {
            ++host_create_events;
            return false;
        });
        simulated_app_window lifecycle_root;
        lifecycle_root.create();
        counted_child orphan;
        bool unparented_rejected = false;
        try {
            orphan.create();
        } catch (const std::logic_error &) {
            unparented_rejected = true;
        }
        expect(unparented_rejected && orphan.creates == 0,
               "the common lifecycle rejects an unparented child");

        bool uncreated_show_rejected = false;
        try {
            orphan.show();
        } catch (const std::logic_error &) {
            uncreated_show_rejected = true;
        }
        expect(uncreated_show_rejected,
               "the common lifecycle rejects showing an uncreated window");

        failing_child failing;
        failing.set_parent(&lifecycle_root);
        for (int attempt = 0; attempt < 2; ++attempt) {
            try {
                failing.create();
            } catch (const std::runtime_error &) {
            }
        }
        expect(!failing.get_created() && failing.attempts == 2,
               "failed creation rolls back and permits a later attempt");

        host.set_parent(&lifecycle_root);
        counted_child early(native::rect(0, 0, 10, 10));
        counted_child late(native::rect(0, 0, 10, 10));

        early.set_parent(&host);
        auto grid = std::make_unique<native::grid_layout_manager>(1, 2);
        host.set_layout(std::move(grid));
        late.set_parent(&host);

        expect(host.get_layout()->children().size() == 2,
               "a panel layout registers each child exactly once");
        expect(bounds_are(early, 0, 0, 100, 100),
               "a panel lays out the child assigned before the layout");
        expect(bounds_are(late, 100, 0, 100, 100),
               "a panel lays out the child assigned after the layout");

        // Detaching an uncreated child updates the panel and layout.
        late.set_parent(nullptr);
        expect(host.get_layout()->children().size() == 1,
               "detaching a child removes it from the panel layout");

        // Nested panels keep their parent pointers and reject cycles.
        simulated_panel inner(native::rect(0, 0, 50, 50));
        inner.set_parent(&host);
        expect(inner.get_parent() == &host,
               "a nested panel records its container as parent");
        bool rejected = false;
        try {
            host.set_parent(&inner);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        expect(rejected, "a panel cycle is rejected");

        // The layout region is the client area after non-client edges.
        native::ruler edge(host, native::ruler_orientation::horizontal);
        edge.set_visible(true);
        const int reserved = edge.get_extent();
        expect(host.get_client_bounds().p.y == reserved,
               "a visible panel edge reserves space before layout");

        // Destroying the panel releases created child resources without
        // deleting the borrowed child objects.
        host.create();
        host.create();
        host.show();
        early.create();
        early.create();
        inner.create();
        expect(host.get_visible() && early.creates == 1 &&
                   inner.get_created() && host_create_events == 1,
               "central creation is idempotent and emits once");
        host.destroy();
        host.destroy();
        expect(early.destroys == 1,
               "destroying a panel destroys created child resources");
        expect(!host.get_created() && !early.get_created() &&
                   !inner.get_created() && !host.get_visible(),
               "a destroyed panel and its children report no resource");
        expect(early.get_parent() == &host &&
                   inner.get_parent() == &host,
               "destroying a panel does not delete borrowed children");
    }

    // Verify canvas construction, content bounds, and clamping.
    void test_canvas_content_and_clamping() {
        constexpr std::int32_t int32_lowest =
            std::numeric_limits<std::int32_t>::min();

        expect(bounds_are(simulated_canvas(), 0, 0, 320, 240),
               "a default canvas keeps the documented bounds");
        const simulated_canvas scalar(3, 4, 60, 70);
        expect(bounds_are(scalar, 3, 4, 60, 70),
               "the scalar canvas constructor preserves bounds");
        const simulated_canvas split(native::point(5, 6),
                                     native::size(80, 90));
        expect(bounds_are(split, 5, 6, 80, 90),
               "the position/size canvas constructor preserves bounds");
        const simulated_canvas whole(native::rect(7, 8, 100, 110));
        expect(bounds_are(whole, 7, 8, 100, 110),
               "the rect canvas constructor preserves bounds");

        simulated_canvas surface(native::rect(0, 0, 100, 100));
        surface.set_vertical_scrollbar_policy(
            native::scrollbar_policy::never);
        surface.set_horizontal_scrollbar_policy(
            native::scrollbar_policy::never);

        // Empty content has no valid interval and reports zero.
        surface.set_scroll_position({50, 50});
        expect(surface.get_scroll_position().x == 0 &&
                   surface.get_scroll_position().y == 0,
               "empty canvas content clamps the position to zero");

        // Content smaller than the viewport pins the leading edge.
        surface.set_content_bounds({-20, -30, 40, 50});
        surface.set_scroll_position({100, 100});
        expect(surface.get_scroll_position().x == -20 &&
                   surface.get_scroll_position().y == -30,
               "content inside the viewport clamps to the origin");

        // A negative origin and a span past the screen range survive.
        surface.set_content_bounds({-4000, -3000, 12000, 9000});
        surface.set_scroll_position({0, 0});
        expect(surface.get_content_bounds().x == -4000 &&
                   surface.get_content_bounds().width == 12000,
               "canvas content bounds keep values outside coord range");
        expect(surface.get_scroll_position().x == 0,
               "a position inside the content interval is kept exactly");
        surface.set_scroll_position({99999, 99999});
        expect(surface.get_scroll_position().x == -4000 + 12000 - 100,
               "the horizontal position clamps to the last full page");
        expect(surface.get_scroll_position().y == -3000 + 9000 - 100,
               "the vertical position clamps to the last full page");
        surface.set_scroll_position({-99999, -99999});
        expect(surface.get_scroll_position().x == -4000 &&
                   surface.get_scroll_position().y == -3000,
               "a position below the origin clamps to the origin");

        // Overflowing bounds must saturate instead of wrapping.
        constexpr std::int32_t limit =
            std::numeric_limits<std::int32_t>::max();
        surface.set_content_bounds(
            {limit - 10, limit - 10,
             static_cast<std::uint32_t>(limit),
             static_cast<std::uint32_t>(limit)});
        surface.set_scroll_position({limit, limit});
        expect(surface.get_scroll_position().x == limit &&
                   surface.get_scroll_position().y == limit,
               "an overflowing content range saturates at the maximum");

        // The full unsigned span is a valid content width, and the
        // last valid position is still one page short of its end.
        surface.set_content_bounds(
            {int32_lowest, int32_lowest,
             std::numeric_limits<std::uint32_t>::max(),
             std::numeric_limits<std::uint32_t>::max()});
        const int page = surface.get_client_bounds().d.w;
        surface.set_scroll_position({limit, limit});
        expect(surface.get_scroll_position().x == limit - page &&
                   surface.get_scroll_position().y == limit - page,
               "a full-range content span clamps without wrapping");
        surface.set_scroll_position({int32_lowest, int32_lowest});
        expect(surface.get_scroll_position().x == int32_lowest &&
                   surface.get_scroll_position().y == int32_lowest,
               "a lowest-value content origin stays reachable");
    }

    // Verify scrollbar policy, viewport reservation, and signalling.
    void test_canvas_scrollbars() {
        simulated_canvas surface(native::rect(0, 0, 100, 100));
        const int viewport = surface.get_client_bounds().d.w;
        expect(viewport == 100,
               "an unscrolled canvas reserves no viewport space");
        expect(!surface.get_horizontal_scrollbar_visible() &&
                   !surface.get_vertical_scrollbar_visible(),
               "automatic scrollbars stay hidden without overflow");

        // never neither reserves nor shows, but still scrolls.
        surface.set_content_bounds({0, 0, 1000, 1000});
        surface.set_horizontal_scrollbar_policy(
            native::scrollbar_policy::never);
        surface.set_vertical_scrollbar_policy(
            native::scrollbar_policy::never);
        expect(!surface.get_horizontal_scrollbar_visible() &&
                   !surface.get_vertical_scrollbar_visible(),
               "the never policy hides an overflowing scrollbar");
        expect(surface.get_client_bounds().d.w == 100,
               "the never policy reserves no viewport space");
        surface.set_scroll_position({40, 40});
        expect(surface.get_scroll_position().x == 40,
               "the never policy still permits programmatic scrolling");

        // always reserves even when no movement is possible.
        simulated_canvas fixed(native::rect(0, 0, 100, 100));
        fixed.set_horizontal_scrollbar_policy(
            native::scrollbar_policy::always);
        fixed.set_vertical_scrollbar_policy(
            native::scrollbar_policy::always);
        expect(fixed.get_horizontal_scrollbar_visible() &&
                   fixed.get_vertical_scrollbar_visible(),
               "the always policy shows a scrollbar without overflow");
        const int reserved = 100 - fixed.get_client_bounds().d.w;
        expect(reserved > 0,
               "the always policy reserves viewport space");
        expect(100 - fixed.get_client_bounds().d.h == reserved,
               "both axes reserve the same themed scrollbar extent");

        // One scrollbar can push the other axis into overflow. The
        // automatic decision has to settle with both visible.
        simulated_canvas coupled(native::rect(0, 0, 100, 100));
        coupled.set_content_bounds({0, 0, 100, 200});
        expect(coupled.get_vertical_scrollbar_visible(),
               "overflowing height shows the vertical scrollbar");
        expect(coupled.get_horizontal_scrollbar_visible(),
               "a vertical scrollbar narrowing the viewport shows the "
               "horizontal scrollbar too");
        expect(coupled.get_client_bounds().d.w == 100 - reserved &&
                   coupled.get_client_bounds().d.h == 100 - reserved,
               "the resolved viewport reserves both scrollbars once");

        // Rulers reserve canvas space inside the scrollbar edges.
        native::ruler top(coupled,
                          native::ruler_orientation::horizontal);
        top.set_visible(true);
        expect(coupled.get_client_bounds().d.h ==
                   100 - reserved - top.get_extent(),
               "a canvas ruler reserves space inside its scrollbars");
        expect(top.get_bounds().d.w == 100 - reserved,
               "a horizontal ruler stops before a vertical scrollbar");

        // A backend scroll emits once and only when the position moves.
        int emitted = 0;
        native::canvas_scroll_position last{};
        coupled.on_scroll.connect(
            [&](native::canvas_scroll_position position) {
                ++emitted;
                last = position;
                return true;
            });
        coupled.set_scroll_position({0, 20});
        expect(emitted == 0,
               "a programmatic canvas scroll emits no action signal");
        coupled.on_native_scroll({0, 40});
        expect(emitted == 1 && last.y == 40,
               "a backend scroll emits the effective position once");
        coupled.on_native_scroll({0, 40});
        expect(emitted == 1,
               "an unchanged backend scroll emits nothing");
        coupled.on_native_scroll({0, 99999});
        expect(emitted == 2 &&
                   last.y == 200 - coupled.get_client_bounds().d.h,
               "a clamped backend scroll emits the clamped position");
    }

    // Verify split geometry, minimums, orientation, and borrowed ownership.
    void test_split_view_model() {
        native::button first("First");
        native::button second("Second");
        int changes = 0;

        {
            native::split_view split(
                first,
                second,
                native::split_orientation::horizontal,
                native::rect(0, 0, 400, 200));
            split.set_splitter_size(8)
                .set_minimums(80, 100)
                .set_ratio(0.25f);
            split.on_ratio_change.connect([&](float) {
                ++changes;
                return false;
            });

            expect(first.get_parent() == &split &&
                       second.get_parent() == &split,
                   "split view borrows and parents both panes");
            expect(bounds_are(first, 0, 0, 98, 200) &&
                       bounds_are(second, 106, 0, 294, 200),
                   "horizontal split resolves its ratio and separator");
            split.on_native_ratio(0.5f);
            expect(changes == 1 && split.get_ratio() == 0.5f,
                   "native splitter movement emits one ratio change");
            split.set_orientation(native::split_orientation::vertical);
            expect(first.get_bounds().d.w == 400 &&
                       second.get_bounds().d.w == 400 &&
                       first.get_bounds().p.y == 0 &&
                       second.get_bounds().p.y ==
                           split.get_splitter_bounds().y2(),
                   "vertical split stacks panes around the separator");
            split.set_ratio(0.0f);
            expect(first.get_dimensions().h == 80,
                   "split view enforces the first pane minimum");
        }

        expect(first.get_parent() == nullptr &&
                   second.get_parent() == nullptr,
               "destroying a split view detaches both borrowed panes");
    }

    // Verify every item collection offers the same append-only builder idiom.
    void test_collection_builders() {
        simulated_page first_page;
        simulated_page second_page;
        native::tab_view tabs;
        tabs << native::tab_page("First", first_page)
             << native::tab_page("Second", second_page);

        simulated_page section_body;
        native::accordion sections;
        sections << native::accordion_section("Section", section_body);

        native::icon_view icons;
        icons << native::icon_view_item{"Icon", nullptr, 7, true};

        native::list items;
        items << "One" << "Two";

        native::combo_box choices;
        choices << "Red" << "Blue";

        native::table_view table;
        table << native::table_column{1, "Name", 120}
              << native::table_column{2, "Value", 80};

        native::tree_view tree;
        tree << native::tree_node("Root", 1, {
            native::tree_node("Child", 2)});

        expect(tabs.get_item_count() == 2 &&
                   sections.get_item_count() == 1 &&
                   icons.get_items().size() == 1 &&
                   items.get_items().size() == 2 &&
                   choices.get_items().size() == 2 &&
                   table.get_columns().size() == 2 &&
                   tree.get_items().size() == 1 &&
                   tree.get_items().front().children.size() == 1,
               "collection builders append through their named APIs");
    }

    // Verify exact-size PNG fallbacks and special-directory snapshots.
    void test_filesystem_resources() {
        const native::file_icon file = native::file_icon::for_file({}, 24);
        expect(file.get_size() == 24, "file icon preserves requested size");
        expect(file.get_source() == native::file_icon_source::generic_file,
               "empty file path selects the generic file icon");
        expect(file.get_png().size() >= 8 &&
                   file.get_png()[0] == 0x89 &&
                   file.get_png()[1] == 0x50 &&
                   file.get_png()[2] == 0x4e &&
                   file.get_png()[3] == 0x47,
               "file icon is encoded as PNG");
        const native::img decoded = native::img::decode(
            file.get_png().data(), file.get_png().size());
        expect(decoded.w() == 24 && decoded.h() == 24,
               "file icon PNG has exact requested dimensions");

        const native::file_icon directory =
            native::file_icon::for_directory({}, 32);
        expect(directory.get_source() ==
                   native::file_icon_source::generic_directory,
               "empty directory path selects the generic folder icon");

        const native::file_icon system_directory =
            native::file_icon::from_path(
                std::filesystem::temp_directory_path(), 20);
        const native::img system_directory_image = native::img::decode(
            system_directory.get_png().data(),
            system_directory.get_png().size());
        expect(system_directory_image.w() == 20 &&
                   system_directory_image.h() == 20 &&
                   system_directory.get_source() !=
                       native::file_icon_source::generic_file,
               "existing directory icon uses directory semantics");

        const auto &special = native::special_directory::detect();
        expect(!special.empty(), "special-directory detection returns data");
        expect(std::all_of(
                   special.begin(),
                   special.end(),
                   [](const native::special_directory &entry) {
                       return entry.get_path().is_absolute();
                   }),
               "special-directory paths are absolute filesystem paths");
        expect(native::special_directory::count() ==
                   static_cast<int>(special.size()),
               "special-directory count matches the snapshot");
        expect(native::special_directory::find(
                   native::special_directory_kind::temporary) != nullptr,
               "special-directory detection includes temporary storage");
        expect(native::special_directory::at(-1) == nullptr &&
                   native::special_directory::at(
                       native::special_directory::count()) == nullptr,
               "special-directory indexing rejects invalid positions");

        bool rejected_size = false;
        try {
            (void)native::file_icon::for_file({}, 0);
        } catch (const std::invalid_argument &) {
            rejected_size = true;
        }
        expect(rejected_size, "file icons reject a zero size");
    }
} // namespace

int main() {
    test_cached_properties();
    test_cursor_property();
    test_menu_label_metadata();
    test_tab_view_model();
    test_control_extension_hooks();
    test_parent_lifetime();
    test_owned_window_lifetime();
    test_file_dialog_properties();
    test_selection_controls();
    test_input_and_window_chrome();
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
    test_split_view_model();
    test_collection_builders();
    test_filesystem_resources();
    test_panel_container();
    test_canvas_content_and_clamping();
    test_canvas_scrollbars();
    return failure_count == 0 ? 0 : 1;
}

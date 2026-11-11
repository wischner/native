//
// Drives ruler markers through the pointer, hosts controls in a toolbar,
// and checks the canvas text, focus and click contracts. Nothing here
// names a toolkit, so it runs on every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <native/canvas.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace native;

    void expect(bool valid, const char *message) {
        if (!valid)
            throw std::runtime_error(message);
    }

    class window final : public app_wnd
    {
    public:
        window()
            : app_wnd("chrome", 30, 30, 480, 320),
              bar_(*this, window_edge::top, 32),
              ruler_(surface_, window_edge::top, 22),
              status_(*this, surface_),
              tabs_(surface_, window_edge::bottom, 24) {
            tool_item choice;
            choice.id = "choice";
            choice.kind = tool_kind::control;
            choice.control = &box_;
            choice.length = 120;
            bar_.add_item(choice);
            ruler_.set_units_per_pixel(0.01).set_marker_snap(0.25);
            tabs_.set_visible(false);
            on_wnd_create.connect([this] {
                surface_.set_parent(this);
                surface_.create();
                surface_.set_bounds(get_client_bounds());
                surface_.show();
                app::post([this] { run(); });
                return true;
            });
        }

        bool failed = false;

    private:
        void run() {
            try {
                check_toolbar();
                check_ruler();
                check_canvas();
                check_status();
                check_tab_strip();
                check_password_edit();
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n';
                failed = true;
            }
            destroy();
        }

        void check_toolbar() {
            expect(box_.get_created(), "the toolbar creates its control");
            expect(box_.get_parent() != nullptr, "the control is parented");
            const auto metrics = theme::create(get_gpx())->defaults();
            expect(box_.get_dimensions().w ==
                       120 - 2 * metrics.toolbar_control_padding,
                   ("the toolbar places its control: width=" +
                    std::to_string(box_.get_dimensions().w) + " strip=" +
                    std::to_string(bar_.get_bounds().w()) + "x" +
                    std::to_string(bar_.get_bounds().h())).c_str());
            const auto original = bar_.get_items();
            auto filled = original;
            filled.front().control_fill_width = true;
            const auto initial_bounds = box_.get_bounds();
            bar_.set_items(filled);
            const auto slot = bar_.get_item_bounds("choice");
            expect(box_.get_bounds().x1() == slot.x1() &&
                       box_.get_bounds().w() == slot.w(),
                   "fill-width control occupies its complete slot width");
            expect(box_.get_bounds().y1() == initial_bounds.y1() &&
                       box_.get_bounds().h() == initial_bounds.h(),
                   "fill-width control retains vertical sizing and alignment");
            bar_.set_items(original);
            expect(box_.get_bounds().w() == initial_bounds.w() &&
                       box_.get_bounds().x1() == initial_bounds.x1(),
                   "default control insets can be restored");
            bool threw = false;
            try {
                tool_item empty;
                empty.id = "empty";
                empty.kind = tool_kind::control;
                bar_.add_item(empty);
            } catch (const std::invalid_argument &) {
                threw = true;
            }
            expect(threw, "a control tool needs a control");
            tool_item icon_tool;
            icon_tool.id = "icon";
            icon_tool.icon = std::make_shared<img>(16, 16);
            bar_.add_item(icon_tool);
            const rect tool = bar_.get_item_bounds("icon");
            expect(tool.w() > 0 && tool.w() == tool.h(),
                   "an icon-only tool is square");
            {
                // Separators have no lookup ID; measure each gap from the
                // square tool that follows it.
                const auto before = bar_.get_items();
                tool_item gap;
                gap.kind = tool_kind::separator;
                tool_item after = icon_tool;
                after.id = "after";
                const auto gap_width = [&](int length) {
                    gap.length = length;
                    bar_.set_items({icon_tool, gap, after});
                    const rect first = bar_.get_item_bounds("icon");
                    return int(bar_.get_item_bounds("after").x1()) -
                           int(first.x1()) - int(first.w());
                };
                const int expected = metrics.toolbar_separator_length > 0
                                         ? metrics.toolbar_separator_length
                                         : int(tool.w());
                const int measured = gap_width(0);
                expect(measured == expected,
                       ("a default separator uses the theme gap: width=" +
                        std::to_string(measured) + " expected=" +
                        std::to_string(expected)).c_str());
                expect(gap_width(5) == 5,
                       "an explicit separator length overrides the default");
                bar_.set_items(before);
            }
            const auto hosted = bar_.get_items();
            bar_.set_items({icon_tool});
            expect(!box_.get_created() && !box_.get_parent(),
                   "removing a hosted control destroys and detaches it");
            bar_.set_items(hosted);
            expect(box_.get_created() && box_.get_visible(),
                   "native-button to hosted-control transition recreates the child");
            expect(!bar_.on_native_command("choice"),
                   "hosted controls do not emit sticky toolbar commands");
            bar_.set_visible(false);
            expect(!box_.get_created(), "hiding a toolbar releases hosted resources");
            bar_.set_visible(true);
            expect(box_.get_created(), "showing a toolbar recreates hosted controls");
        }

        void check_ruler() {
            ruler_.set_active_range(1.0, 3.0);
            ruler_.set_markers(
                {{7, 2.0, ruler_marker_shape::down_triangle, true},
                 {8, 3.5, ruler_marker_shape::block, false}});
            std::vector<ruler_marker_move> moves;
            const auto marker_connection = ruler_.on_marker_move.connect_scoped([&](ruler_marker_move move) {
                moves.push_back(move);
                return true;
            });
            bool client_saw_press = false;
            const auto client_connection = surface_.on_mouse_click.connect_scoped([&](mouse_event) {
                client_saw_press = true;
                return true;
            });
            const rect strip = ruler_.get_bounds();
            const point on_marker(static_cast<coord>(strip.x1() + 200),
                                  static_cast<coord>(strip.y1() + 4));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::press, on_marker));
            expect(!client_saw_press, "a marker press stays in the ruler");
            const point moved(static_cast<coord>(strip.x1() + 248),
                              static_cast<coord>(strip.y1() + 60));
            surface_.on_native_mouse_move(moved);
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::release, moved));
            expect(moves.size() == 2, "a drag reports move and release");
            expect(moves.back().id == 7 && moves.back().finished,
                   "release finishes the gesture");
            expect(moves.back().value == 2.5, "drags snap to the interval");
            moves.clear();
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::press,
                {static_cast<coord>(strip.x1() + 250), on_marker.y}));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::release,
                {static_cast<coord>(strip.x1() + 275), moved.y}));
            expect(ruler_.get_markers()[0].value == 2.75,
                   "release position is committed without a final motion event");
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::press,
                {static_cast<coord>(strip.x1() + 275), on_marker.y}));
            surface_.on_native_mouse_cancel(mouse_cancel_reason::focus_lost);
            surface_.on_native_mouse_move(moved);
            expect(ruler_.get_markers()[0].value == 2.75,
                   "cancelled ruler drags do not resume on motion");
            moves.clear();
            const point fixed(static_cast<coord>(strip.x1() + 350),
                              static_cast<coord>(strip.y1() + 4));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::press, fixed));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::release, fixed));
            expect(moves.empty(), "a fixed marker cannot be dragged");
        }

        void check_canvas() {
            surface_.focus();
            expect(surface_.get_focused(), "focus() takes the keyboard");
            std::string typed;
            const auto text_connection = surface_.on_text_input.connect_scoped([&](const std::string &utf8) {
                typed += utf8;
                return true;
            });
            surface_.on_native_text_input("\xc5\xbe");
            expect(typed == "\xc5\xbe", "text reaches the canvas");
            surface_.on_key.connect([](key_event event) {
                return event.key == key_code::a;
            });
            surface_.on_native_key({key_code::a, key_action::press,
                                    false, key_modifiers::none});
            surface_.on_native_text_input("a");
            expect(typed == "\xc5\xbe", "consumed keys suppress separate text events");
            surface_.on_native_key({key_code::a, key_action::release,
                                    false, key_modifiers::none});
            surface_.on_native_key({key_code::b, key_action::press,
                                    false, key_modifiers::none});
            surface_.on_native_text_input("b");
            expect(typed == "\xc5\xbe" "b", "unconsumed keys restore text delivery");
            surface_.on_native_key({key_code::b, key_action::release,
                                    false, key_modifiers::none});
            int clicks = 0;
            key_modifiers held = key_modifiers::none;
            const auto click_connection = surface_.on_mouse_click.connect_scoped([&](mouse_event event) {
                clicks = event.clicks;
                held = event.modifiers;
                return true;
            });
            const point inside(40, 120);
            surface_.on_native_key({key_code::left_shift, key_action::press,
                                    false, key_modifiers::shift});
            for (int press = 1; press <= 2; ++press) {
                surface_.on_native_mouse_click(mouse_event(
                    mouse_button::left, mouse_action::press, inside));
                expect(clicks == press, "the canvas counts quick presses");
                expect((held & key_modifiers::shift) == key_modifiers::shift,
                       "held modifiers reach the press");
                surface_.on_native_mouse_click(mouse_event(
                    mouse_button::left, mouse_action::release, inside));
            }
            surface_.on_native_key({key_code::left_shift, key_action::release,
                                    false, key_modifiers::none});
        }

        // A press selects a tab, a second press on it activates, and
        // programmatic selection stays silent.
        void check_password_edit() {
            password_edit secret(0, 0, 160, 26);
            secret.set_parent(this);
            secret.create();
            std::string changed = "none";
            const auto change = secret.on_change.connect(
                [&](std::string value) { changed = value; return true; });
            secret.set_text("ab");
            expect(changed == "none", "set_text is silent");
            expect(secret.get_length() == 2, "the field counts characters");
            secret.on_native_text_input("c");
            expect(changed == "abc", "typing reports the value");
            secret.on_native_text_input("\xC4\x8D");
            expect(secret.get_length() == 4, "multibyte is one character");
            expect(secret.get_text() == "abc\xC4\x8D", "the value is exact");
            key_event back;
            back.key = key_code::backspace;
            back.action = key_action::press;
            secret.on_native_key(back);
            expect(secret.get_text() == "abc", "backspace removes a character");
            changed = "none";
            expect(!secret.on_native_text_input("x\ny"),
                   "control characters are refused");
            expect(changed == "none", "a refusal changes nothing");
            secret.on_change.disconnect(change);
        }

        void check_tab_strip() {
            tabs_.set_visible(true);
            int selected = -1;
            int activated = -1;
            const auto selection = tabs_.on_selection_change.connect(
                [&](int index) { selected = index; return true; });
            const auto activation = tabs_.on_activate.connect(
                [&](int index) { activated = index; return true; });
            tabs_.set_items({"First", "Second", "Third"});
            expect(tabs_.get_selected_index() == 0 ||
                   tabs_.get_selected_index() == -1, "items keep the selection");
            tabs_.set_selected_index(0);
            expect(selected == -1, "programmatic selection is silent");
            const rect strip = tabs_.get_bounds();
            expect(strip.d.h == 24, "the strip keeps its extent");
            expect(surface_.get_client_bounds().y2() <= strip.y1(),
                   "the client stops above the strip");
            const rect second = tabs_.get_tab_bounds(1);
            expect(second.d.w > 0, "a tab has a place on the strip");
            const point inside(static_cast<coord>(strip.p.x + second.p.x + 4),
                               static_cast<coord>(strip.p.y + 4));
            mouse_event press(mouse_button::left, mouse_action::press, inside);
            press.clicks = 1;
            surface_.on_native_mouse_click(press);
            expect(selected == 1, "a press selects a tab");
            expect(tabs_.get_selected_index() == 1, "selection is stored");
            surface_.on_native_mouse_click(
                mouse_event(mouse_button::left, mouse_action::release, inside));
            mouse_event twice = press;
            twice.clicks = 2;
            wnd &host = surface_;
            host.wnd::on_native_mouse_click(twice);
            expect(activated == 1, "a double click activates the selected tab");
            tabs_.set_items({"Only"});
            expect(tabs_.get_selected_index() == 0,
                   "a shrinking list keeps the selection in range");
            expect(!tabs_.get_tab_bounds(4).d.w, "unknown tabs have no place");
            bool threw = false;
            try {
                tabs_.set_selected_index(9);
            } catch (const std::out_of_range &) {
                threw = true;
            }
            expect(threw, "selection outside the list is rejected");
            tabs_.set_visible(false);
            tabs_.on_selection_change.disconnect(selection);
            tabs_.on_activate.disconnect(activation);
        }

        void check_status() {
            const bool inside = resources::current_context().toolkit == "gemix";
            expect(status_.get_owner() == (inside ? static_cast<wnd *>(&surface_) : this),
                   "document status follows backend docking convention");
            status_.set_extent(theme::create(get_gpx())->get_status_bar_height());
            for (auto policy : {scrollbar_policy::always, scrollbar_policy::never}) {
                surface_.set_horizontal_scrollbar_policy(policy);
                surface_.set_vertical_scrollbar_policy(policy);
                for (const auto width : {400, 440}) {
                    surface_.set_dimensions({dim(width), 240});
                    const auto bounds = status_.get_bounds();
                    if (inside) {
                        const int edge = policy == scrollbar_policy::always
                            ? theme::create(surface_.get_gpx())->defaults().scrollbar_extent : 0;
                        const auto dimensions = surface_.get_dimensions();
                        expect(bounds.y2() == dimensions.h - edge &&
                               bounds.x2() == dimensions.w - edge,
                               "GEM status stops before the bottom and right scrollbars");
                        expect(surface_.get_client_bounds().y2() == bounds.y1(),
                               "GEM document stops above its status bar");
                        if (policy == scrollbar_policy::always)
                            expect(bounds.y2() < surface_.get_dimensions().h &&
                                   bounds.x2() < surface_.get_dimensions().w,
                                   "GEM scrollbar space remains outside the status bar");
                    } else {
                        expect(bounds.y2() == get_chrome_bounds().y2(),
                               "other backends retain the outer window footer");
                    }
                }
            }
        }

        canvas surface_{0, 0, 480, 240};
        combo_box box_{{"one", "two"}};
        toolbar bar_;
        ruler ruler_;
        status_bar status_;
        tab_strip tabs_;
    };
} // namespace

int program(int, char **) {
    window main_window;
    const auto result = app::run(main_window);
    return result ? result : main_window.failed ? 1 : 0;
}

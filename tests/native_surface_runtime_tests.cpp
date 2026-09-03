//
// Exercises live panel and canvas lifecycle through the selected
// backend. The posted checks close their own window, so toolkit
// sessions can run this executable unattended as a smoke test.
//
// Pointer and scroll input is delivered through the portable
// on_native_* hooks every backend's own routing calls, so the checks
// hold wherever the control is implemented.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdint>
#include <iostream>
#include <limits>
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

    bool bounds_are(const native::wnd &window,
                    int x,
                    int y,
                    int width,
                    int height) {
        const native::rect bounds = window.get_bounds();
        return bounds.p.x == x && bounds.p.y == y &&
               bounds.d.w == width && bounds.d.h == height;
    }

    class surface_window final : public native::app_wnd
    {
    public:
        surface_window()
            : native::app_wnd("Surface runtime test",
                              native::rect(40, 40, 600, 400))
            , _page(0, 0, 600, 400)
            , _inner(0, 0, 10, 10)
            , _command("Run")
            , _editor("Text",
                      native::text_edit_mode::single_line,
                      native::rect(0, 0, 10, 10))
            , _drawing(0, 0, 10, 10)
            , _ruler(_drawing, native::ruler_orientation::horizontal) {
            _drawing.set_content_bounds({-4000, -3000, 12000, 9000});
            _drawing.on_wnd_paint.connect(
                this, &surface_window::on_canvas_paint);
            _drawing.on_scroll.connect(
                this, &surface_window::on_canvas_scroll);
            _drawing.on_mouse_move.connect(
                this, &surface_window::on_canvas_move);
            _drawing.on_mouse_click.connect(
                this, &surface_window::on_canvas_click);
            _drawing.on_mouse_wheel.connect(
                this, &surface_window::on_canvas_wheel);
            _page.on_mouse_click.connect(
                this, &surface_window::on_panel_click);
            on_wnd_create.connect(this, &surface_window::on_create);
        }

    private:
        native::panel _page;
        native::panel _inner;
        native::button _command;
        native::text_edit _editor;
        native::canvas _drawing;
        native::ruler _ruler;

        int _paints = 0;
        native::rect _last_invalid;
        bool _paint_threw = false;
        int _scrolls = 0;
        native::canvas_scroll_position _last_scroll{};
        int _moves = 0;
        int _clicks = 0;
        int _wheels = 0;
        native::point _last_pointer;
        int _panel_clicks = 0;

        bool on_canvas_paint(native::wnd_paint_event event) {
            ++_paints;
            _last_invalid = event.r;
            try {
                event.g.set_ink(native::rgba(40, 80, 160, 255))
                    .clear(native::rgba(250, 250, 252, 255))
                    .draw_rect(native::rect(2, 2, 40, 30), true)
                    .draw_line(native::point(0, 0),
                               native::point(60, 40))
                    .draw_ellipse(native::rect(4, 4, 30, 20))
                    .draw_polygon({native::point(10, 10),
                                   native::point(30, 12),
                                   native::point(20, 26)})
                    .draw_text("canvas", native::point(6, 6));
            } catch (const std::exception &) {
                _paint_threw = true;
            }
            return false;
        }

        bool on_canvas_scroll(native::canvas_scroll_position position) {
            ++_scrolls;
            _last_scroll = position;
            return false;
        }

        bool on_canvas_move(native::point position) {
            ++_moves;
            _last_pointer = position;
            return false;
        }

        bool on_canvas_click(native::mouse_event event) {
            ++_clicks;
            _last_pointer = event.position;
            return false;
        }

        bool on_canvas_wheel(native::mouse_wheel_event event) {
            ++_wheels;
            _last_pointer = event.position;
            return false;
        }

        bool on_panel_click(native::mouse_event) {
            ++_panel_clicks;
            return false;
        }

        // Build the composition and check everything the first paint
        // pass does not have to have happened for.
        bool on_create() {
            try {
                build();
                check_layout();
                check_nesting();
                check_scrollbars();
                check_scrolling();
                check_pointer();
                _drawing.invalidate();
            } catch (const std::exception &error) {
                std::cerr << "FAILED: unexpected exception: "
                          << error.what() << '\n';
                ++failure_count;
                destroy();
                return false;
            }

            native::app::post([this] { finish(); });
            return false;
        }

        void build() {
            _page.set_parent(this);
            _command.set_parent(&_page);
            _editor.set_parent(&_page);
            _drawing.set_parent(&_page);
            _inner.set_parent(&_page);

            auto host = std::make_unique<native::grid_layout_manager>();
            (*host) << native::row(native::star())
                    << native::column(native::star())
                    << native::cell(_page, 0, 0);
            set_layout(std::move(host));

            auto grid = std::make_unique<native::grid_layout_manager>();
            (*grid) << native::row(native::pixels(40))
                    << native::row(native::star())
                    << native::column(native::pixels(200))
                    << native::column(native::star())
                    << native::cell(_command, 0, 0)
                    << native::cell(_editor, 0, 1)
                    << native::cell(_inner, 1, 0)
                    << native::cell(_drawing, 1, 1);
            _page.set_layout(std::move(grid));

            // The panel supplies the native parent; children are
            // created and shown from here, never implicitly.
            _page.create();
            _inner.create();
            _command.create();
            _editor.create();
            _drawing.create();
            _page.show();
            _inner.show();
            _command.show();
            _editor.show();
            _drawing.show();
        }

        void check_layout() {
            expect(_page.get_created() && _drawing.get_created(),
                   "a panel and its canvas child create their "
                   "backend resources");
            const native::rect client = get_client_bounds();
            expect(bounds_are(_page, client.p.x, client.p.y,
                              client.d.w, client.d.h),
                   "a laid-out panel fills its host client area");
            expect(bounds_are(_command, 0, 0, 200, 40),
                   "a panel lays out its button child");
            expect(bounds_are(_editor, 200, 0,
                              client.d.w - 200, 40),
                   "a panel lays out its editor child");
            expect(bounds_are(_drawing, 200, 40,
                              client.d.w - 200, client.d.h - 40),
                   "a panel lays out its canvas child");

            // A resize must reach every descendant.
            const native::size before = _drawing.get_dimensions();
            set_dimensions(native::size(500, 320));
            expect(_drawing.get_dimensions().w != before.w ||
                       _drawing.get_dimensions().h != before.h,
                   "resizing the host relayouts panel descendants");
            expect(_drawing.get_position().x == 200 &&
                       _drawing.get_position().y == 40,
                   "panel children keep panel-local coordinates");
        }

        void check_nesting() {
            // A panel inside a panel keeps its own layout and children.
            native::button nested("Nested");
            nested.set_parent(&_inner);
            auto grid = std::make_unique<native::grid_layout_manager>();
            (*grid) << native::row(native::star())
                    << native::column(native::star())
                    << native::cell(nested, 0, 0);
            _inner.set_layout(std::move(grid));
            expect(nested.get_bounds().d.w == _inner.get_dimensions().w &&
                       nested.get_bounds().d.h ==
                           _inner.get_dimensions().h,
                   "a nested panel arranges its own descendants");
            expect(nested.get_parent() == &_inner &&
                       _inner.get_parent() == &_page,
                   "nested panels keep correct parent pointers");
            nested.create();
            nested.show();
            expect(nested.get_created(),
                   "a control creates inside a nested panel");
            nested.destroy();
        }

        void check_scrollbars() {
            // The content is far larger than the viewport, so both
            // automatic scrollbars resolve visible.
            expect(_drawing.get_horizontal_scrollbar_visible() &&
                       _drawing.get_vertical_scrollbar_visible(),
                   "overflowing canvas content shows both scrollbars");
            const native::rect viewport = _drawing.get_client_bounds();
            expect(viewport.d.w < _drawing.get_dimensions().w &&
                       viewport.d.h < _drawing.get_dimensions().h,
                   "visible canvas scrollbars reserve viewport space");
            expect(viewport.p.y == _ruler.get_extent(),
                   "a canvas ruler reserves space above the viewport");
            expect(_ruler.get_bounds().d.w <
                       _drawing.get_dimensions().w,
                   "a horizontal ruler stops before the vertical "
                   "scrollbar");

            // Crossing the automatic threshold hides them again.
            _drawing.set_content_bounds({0, 0, 4, 4});
            expect(!_drawing.get_horizontal_scrollbar_visible() &&
                       !_drawing.get_vertical_scrollbar_visible(),
                   "content inside the viewport hides both scrollbars");
            expect(_drawing.get_client_bounds().d.w ==
                       _drawing.get_dimensions().w,
                   "hidden canvas scrollbars reserve no space");
            _drawing.set_content_bounds({-4000, -3000, 12000, 9000});
            expect(_drawing.get_vertical_scrollbar_visible(),
                   "restoring overflowing content shows the scrollbars "
                   "again");
        }

        void check_scrolling() {
            _drawing.set_scroll_position({-4000, -3000});
            const int scrolls = _scrolls;
            expect(scrolls == 0,
                   "a programmatic canvas scroll emits no signal");

            // A backend line, page, or thumb action all arrive as one
            // absolute position.
            _drawing.on_native_scroll({-3900, -3000});
            expect(_scrolls == scrolls + 1 && _last_scroll.x == -3900,
                   "a backend canvas scroll emits the new position");
            _drawing.on_native_scroll({-3900, -3000});
            expect(_scrolls == scrolls + 1,
                   "an unchanged backend canvas scroll emits nothing");

            const int viewport = _drawing.get_client_bounds().d.h;
            _drawing.on_native_scroll(
                {-3900, -3000 + viewport});
            expect(_last_scroll.y == -3000 + viewport,
                   "a page-sized canvas scroll lands on the page");

            // Both endpoints of the content range stay reachable.
            _drawing.on_native_scroll(
                {std::numeric_limits<std::int32_t>::max(),
                 std::numeric_limits<std::int32_t>::max()});
            expect(_last_scroll.x == -4000 + 12000 -
                       _drawing.get_client_bounds().d.w,
                   "the canvas clamps to the end of its content");
            _drawing.on_native_scroll(
                {std::numeric_limits<std::int32_t>::min(),
                 std::numeric_limits<std::int32_t>::min()});
            expect(_last_scroll.x == -4000 && _last_scroll.y == -3000,
                   "the canvas clamps back to its content origin");
        }

        void check_pointer() {
            const native::rect viewport = _drawing.get_client_bounds();
            const native::point inside(
                static_cast<native::coord>(viewport.p.x + 5),
                static_cast<native::coord>(viewport.p.y + 6));

            _drawing.on_native_mouse_move(inside);
            expect(_moves == 1 && _last_pointer.x == inside.x &&
                       _last_pointer.y == inside.y,
                   "canvas motion reports canvas-local coordinates");

            _drawing.on_native_mouse_click(
                native::mouse_event(native::mouse_button::left,
                                    native::mouse_action::press,
                                    inside));
            _drawing.on_native_mouse_click(
                native::mouse_event(native::mouse_button::left,
                                    native::mouse_action::release,
                                    inside));
            expect(_clicks == 2,
                   "a canvas press and release each emit once");

            const int scrolls = _scrolls;
            _drawing.on_native_mouse_wheel(native::mouse_wheel_event(
                inside, -24, native::wheel_direction::vertical));
            expect(_wheels == 1,
                   "a canvas wheel event emits exactly once");
            expect(_scrolls == scrolls + 1,
                   "a canvas wheel over movable content scrolls it");

            // A press inside the scrollbar track is chrome, never a
            // client click.
            const int clicks = _clicks;
            const native::point track(
                static_cast<native::coord>(
                    _drawing.get_dimensions().w - 2),
                static_cast<native::coord>(
                    _drawing.get_dimensions().h / 2));
            _drawing.on_native_mouse_click(
                native::mouse_event(native::mouse_button::left,
                                    native::mouse_action::press,
                                    track));
            _drawing.on_native_mouse_click(
                native::mouse_event(native::mouse_button::left,
                                    native::mouse_action::release,
                                    track));
            expect(_clicks == clicks,
                   "a canvas scrollbar press is not a client click");

            // Empty panel space reports the event and does nothing.
            _page.on_native_mouse_click(
                native::mouse_event(native::mouse_button::left,
                                    native::mouse_action::press,
                                    native::point(1, 1)));
            expect(_panel_clicks == 1,
                   "empty panel space reports an unconsumed press");
        }

        // Runs after the backend has presented at least one frame.
        void finish() {
            try {
                expect(_paints >= 1,
                       "a shown canvas receives at least one paint");
                expect(_last_invalid.d.w > 0 && _last_invalid.d.h > 0,
                       "a canvas paint event carries valid bounds");
                expect(!_paint_threw,
                       "canvas drawing primitives complete without "
                       "throwing");

                // Destroying and recreating the composition must leave
                // no stale binding behind.
                _drawing.destroy();
                _command.destroy();
                _editor.destroy();
                _inner.destroy();
                _page.destroy();
                expect(!_page.get_created() && !_drawing.get_created(),
                       "a destroyed panel and canvas report no resource");

                build();
                expect(_page.get_created() && _drawing.get_created(),
                       "a panel and canvas recreate after destruction");
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
    surface_window window;
    const int result = native::app::run(window);
    return result == 0 && failure_count == 0 ? 0 : 1;
}

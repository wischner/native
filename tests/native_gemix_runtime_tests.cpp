//
// Exercises GEM menu topology, clipped/coalesced painting, pressed feedback,
// popup overlays, splitter capture, and original GEM scrollbar arrows
// against the rasta framebuffer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include <native.h>
#include "../lib/native/toolkits/gemix/globals.h"
#include "../lib/native/toolkits/gemix/stock_text.h"
#include "../lib/native/gpx_wnd.h"
#include "../lib/native/code_render.h"

namespace
{
    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    std::vector<unsigned char> framebuffer() {
        std::ifstream file(std::getenv("GEM_RASTA_FRAMEBUFFER"), std::ios::binary);
        expect(bool(file), "framebuffer is readable");
        return {std::istreambuf_iterator<char>(file), {}};
    }

    bool colors_only = false;
    bool editor_frame_only = false;
    bool chrome_only = false;

    // Read a packed physical bit or the darkness of an RGBA pixel.
    bool raw_pixel(const std::vector<unsigned char> &frame, int x, int y) {
        if (frame.size() == 904U * 900U * 4U) {
            const auto offset = (y * 904 + x) * 4;
            return int(frame.at(offset)) + frame.at(offset + 1) +
                frame.at(offset + 2) < 3 * 128;
        }
        expect(frame.size() == 113U * 900U, "known Rasta framebuffer format");
        return (frame.at(y * 113 + x / 8) & (0x80 >> (x % 8))) != 0;
    }

    // The configured non-inverted raster uses set bits/dark RGBA for ink.
    bool pixel(const std::vector<unsigned char> &frame, int x, int y) {
        return raw_pixel(frame, x, y);
    }

    // The selected trailing outline must survive opening its page join.
    void check_tab_joins(native::app_wnd &owner) {
        using namespace native;
        using namespace linux::gemix;
        modeless_wnd host(owner, "Tab joins", 60, 60, 320, 220);
        panel first, second;
        tab_view tabs(10, 10, 280, 180);
        tabs.add_item("One", first);
        tabs.add_item("Two", second);
        tabs.set_parent(&host);
        host.create();
        tabs.create();
        tabs.show();
        host.show();
        for (const auto placement : {tab_placement::bottom,
                                      tab_placement::right}) {
            tabs.set_tab_placement(placement);
            for (bool framed : {true, false}) {
                tabs.set_page_frame_visible(framed);
                for (dim height : {dim(20), dim(32)}) {
                    tabs.set_tab_height(height);
                    for (int selected : {0, 1}) {
                        tabs.set_selected_index(selected);
                        tabs.invalidate();
                        flush_repaints();
                        WORD attributes[6] = {};
                        vqt_attributes(runtime.vdi_handle, attributes);
                        const auto pixels = framebuffer();
                        const auto work = work_rect(
                            wnd_bindings.handle_from_object(&host));
                        const auto control = root_bounds(tabs);
                        const auto tab = tabs.get_tab_bounds(selected);
                        expect(pixel(pixels,
                                   work.x1() + control.x1() + tab.x1() + 1,
                                   work.y1() + control.y1() + tab.y1() + 1),
                               "selected bottom/right tab outline touches its page join");
                        const int far_x = placement == tab_placement::bottom
                            ? tab.x2() - 1 : tab.x1() + 1;
                        const int far_y = placement == tab_placement::right
                            ? tab.y2() - 1 : tab.y1() + 1;
                        expect(pixel(pixels, work.x1() + control.x1() + far_x,
                                     work.y1() + control.y1() + far_y),
                               "both ends of the selected tab meet the page line");
                    }
                }
            }
        }
        host.destroy();
    }

    // Verify the displayed grip contains exactly three separated ink pixels.
    void check_splitter_grips(native::app_wnd &owner) {
        using namespace native;
        using namespace linux::gemix;
        modeless_wnd host(owner, "Splitter grips", 60, 60, 320, 220);
        list first({"First pane"}), second({"Second pane"});
        split_view split(first, second, split_orientation::horizontal,
                         10, 10, 280, 180);
        split.set_parent(&host);
        host.create();
        split.create();
        split.show();
        host.show();
        for (auto orientation : {split_orientation::horizontal,
                                  split_orientation::vertical}) {
            split.set_orientation(orientation);
            for (float ratio : {0.5f, 0.3f, 0.7f}) {
                split.set_ratio(ratio);
                flush_repaints();
                WORD attributes[6] = {};
                vqt_attributes(runtime.vdi_handle, attributes);
                const auto pixels = framebuffer();
                const auto work = work_rect(
                    wnd_bindings.handle_from_object(&host));
                const auto control = root_bounds(split);
                const auto grip = split.get_splitter_bounds();
                const int cx = grip.x1() + grip.w() / 2;
                const int cy = grip.y1() + grip.h() / 2;
                for (int y = grip.y1(); y < grip.y2(); ++y) {
                    for (int x = grip.x1(); x < grip.x2(); ++x) {
                        const bool expected =
                            orientation == split_orientation::horizontal
                                ? x == cx && (y == cy - 2 || y == cy || y == cy + 2)
                                : y == cy && (x == cx - 2 || x == cx || x == cx + 2);
                        expect(pixel(pixels, work.x1() + control.x1() + x,
                                     work.y1() + control.y1() + y) == expected,
                               "GEM splitter has exactly three centered grip pixels");
                    }
                }
            }
        }
        host.destroy();
    }

    // Verify absolute displayed colors. Calibrating against VDI's logical
    // names would hide the hosted SDK's reversed monochrome index mapping.
    void check_color_indices(native::wnd &owner) {
        const auto handle = linux::gemix::runtime.vdi_handle;
        WORD area[4] = {640, 600, 671, 615};
        vs_clip(handle, 1, area);
        vswr_mode(handle, MD_REPLACE);
        vsf_interior(handle, FIS_SOLID);
        WORD white[4] = {640, 600, 655, 607};
        WORD black[4] = {656, 600, 671, 607};
        vsf_color(handle, BLACK);
        vr_recfl(handle, white);
        vsf_color(handle, WHITE);
        vr_recfl(handle, black);
        vs_clip(handle, 0, nullptr);
        // A query fences void drawing commands on the gemd transport.
        WORD attributes[6] = {};
        expect(vqt_attributes(handle, attributes), "VDI color reference is complete");
        auto frame = framebuffer();
        expect(!pixel(frame, 644, 604) && pixel(frame, 660, 604),
            "hosted VDI reference displays white paper and black ink");
        native::gpx_wnd graphics(&owner, {640, 608});
        graphics.set_clip({0, 0, 32, 8}).clear({255, 255, 255, 255});
        graphics.set_ink({0, 0, 0, 255}).draw_rect({16, 0, 16, 8}, true);
        expect(vqt_attributes(handle, attributes), "Native color reference is complete");
        frame = framebuffer();
        expect(!pixel(frame, 644, 612) && pixel(frame, 660, 612),
            "Native white backgrounds and black ink have correct displayed colors");
    }

    // Fill a titleless popup's client up to every edge, as a custom editor can.
    class canvas_popup final : public native::modeless_wnd
    {
    public:
        canvas_popup(native::app_wnd &owner)
            : modeless_wnd(owner, "Canvas popup", 180, 90, 240, 140) {
            surface.set_parent(this);
            surface.on_wnd_paint.connect([](native::wnd_paint_event event) {
                event.g.clear(native::rgba(255, 255, 255, 255)); return true;
            });
            on_wnd_create.connect([this] {
                surface.set_bounds(get_client_bounds());
                surface.create(); surface.show(); return true;
            });
        }
        bool get_native_title_visible() const override { return false; }
        native::canvas surface;
    };

    // The same top-left text origin must align VDI and image glyph cells.
    void check_text_origin(native::wnd &owner) {
        native::gpx_wnd graphics(&owner, {640, 600});
        native::img reference(32, 24);
        for (native::gpx *target : {static_cast<native::gpx *>(&graphics), &reference.get_gpx()})
            target->set_clip(native::rect(0, 0, 32, 24)).clear({255, 255, 255, 255})
                .set_font(native::font_t::stock(native::font_role::control))
                .set_ink({0, 0, 0, 255}).draw_text("New", native::point(0, 3));
        const auto frame = framebuffer();
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 32; ++x)
                expect(pixel(frame, 640 + x, 600 + y) ==
                    (reference.pixels()[y * 32 + x].r == 0),
                    "GEM window text and toolbar image cells share a vertical origin");
    }

    // A first-row highlight must preserve the same frame as later rows.
    void check_code_editor_frame(native::wnd &owner) {
        native::code_edit editor("FIRST\nSECOND", 0, 0, 160, 64);
        editor.set_show_line_numbers(false);
        native::code_theme colors;
        colors.current_line = native::rgba(255, 255, 255, 255);
        editor.set_code_theme(colors);
        native::gpx_wnd graphics(&owner, {640, 600});
        for (auto sides : {native::border_sides::all,
                           native::border_sides::none,
                           native::border_sides::top}) {
            editor.set_border_sides(sides);
            for (int row : {0, 1, 0}) {
                editor.go_to_line(row);
                graphics.set_clip({0, 0, 160, 64});
                native::draw_code_edit(editor, graphics, {});
                WORD attributes[6]{};
                vqt_attributes(linux::gemix::runtime.vdi_handle, attributes);
                const auto frame = framebuffer();
                for (int x = 24; x < 159; ++x)
                    expect(pixel(frame, 640 + x, 600) ==
                               native::has_border(sides, native::border_sides::top),
                           "editor top border is stable when caret changes row");
            }
        }
    }

    // Compare image painting to VDI's original system-font arrow glyphs.
    void check_scrollbar_arrows() {
        using native::scrollbar_orientation;
        using native::scrollbar_part;
        struct arrow_case
        {
            scrollbar_orientation axis;
            scrollbar_part part;
            BYTE glyph;
            int offset_y;
        };
        const arrow_case arrows[] = {
            {scrollbar_orientation::vertical, scrollbar_part::decrement, 1, -1},
            {scrollbar_orientation::vertical, scrollbar_part::increment, 2, 0},
            {scrollbar_orientation::horizontal, scrollbar_part::decrement, 4, 2},
            {scrollbar_orientation::horizontal, scrollbar_part::increment, 3, 2}
        };
        const native::rect bounds_cases[] = {
            native::rect(3, 3, 16, 16), native::rect(3, 3, 20, 20),
            native::rect(3, 3, 6, 7), native::rect(3, 3, 1, 1)
        };
        const native::rgba sentinel(64, 128, 192, 255);
        const native::rgba ink(0, 0, 0, 255), paper(255, 255, 255, 255);
        const auto handle = linux::gemix::runtime.vdi_handle;
        WORD previous_text[6] = {};
        expect(vqt_attributes(handle, previous_text), "VDI text state is available");
        vst_font(handle, 1);
        for (const auto &arrow : arrows) {
            // Reserve an unused desktop cell for the original 8x16 glyph.
            WORD cell[4] = {640, 640, 647, 655};
            vs_clip(handle, 1, cell);
            vswr_mode(handle, MD_REPLACE);
            vsf_interior(handle, FIS_SOLID);
            vsf_color(handle, BLACK);
            vr_recfl(handle, cell);
            vst_color(handle, WHITE);
            const BYTE text[2] = {arrow.glyph, 0};
            v_gtext(handle, 640, 656, text);
            vs_clip(handle, 0, nullptr);
            const auto original = framebuffer();
            int glyph_pixels = 0;
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 8; ++x)
                    glyph_pixels += pixel(original, 640 + x, 640 + y);
            expect(glyph_pixels > 0, "VDI reference arrow is visible");

            for (const auto &bounds : bounds_cases) {
                for (bool pressed : {false, true}) {
                    for (bool clipped : {false, true}) {
                        native::img surface(26, 26);
                        auto &graphics = surface.get_gpx();
                        const native::rect clip = clipped
                            ? native::rect(5, 5, 7, 9) : native::rect(0, 0, 26, 26);
                        graphics.clear(sentinel).set_clip(clip);
                        auto appearance = native::theme::create(graphics);
                        native::theme::state state;
                        state.pressed = pressed;
                        appearance->draw_scrollbar_part(
                            bounds, arrow.axis, arrow.part, state);
                        // AES centers the complete font cell, with its
                        // original per-direction vertical adjustment.
                        const int left = bounds.x1() + (int(bounds.w()) - 8) / 2;
                        const int top = bounds.y1() + (int(bounds.h()) - 16) / 2
                            + arrow.offset_y;
                        for (int y = 0; y < 26; ++y) {
                            for (int x = 0; x < 26; ++x) {
                                native::rgba expected = sentinel;
                                const native::point p(x, y);
                                if (bounds.contains(p) && clip.contains(p)) {
                                    const bool border = x == bounds.x1() ||
                                        x == bounds.x2() - 1 || y == bounds.y1() ||
                                        y == bounds.y2() - 1;
                                    const bool glyph = x >= left && x < left + 8 &&
                                        y >= top && y < top + 16 &&
                                        pixel(original, 640 + x - left, 640 + y - top);
                                    expected = border || (glyph != pressed) ? ink : paper;
                                }
                                expect(surface.pixels()[y * 26 + x] == expected,
                                    "scrollbar arrows match AES glyphs, inversion, and clipping");
                            }
                        }
                    }
                }
            }
        }
        vst_font(handle, previous_text[0]);
    }

    class test_window final : public native::app_wnd
    {
    public:
        test_window() : native::app_wnd("GEM regression", 36, 36, 500, 400),
            button("Activate", 20, 20, 120, 30),
            list({"First", "Second", "Third"}, 170, 20, 200, 130),
            combo({"One", "Two", "Three", "Four"},
                  native::combo_box_style::drop_down_list, 170, 20, 200, 24),
            editor("Editing", native::text_edit_mode::single_line, 20, 160, 400, 26),
            first({"Left"}), second({"Right"}),
            split(first, second, native::split_orientation::horizontal,
                  20, 200, 440, 150) {
            menu << "File" << native::menu_items("Open") << "Edit"
                 << native::menu_items("Copy");
            on_wnd_create.connect(this, &test_window::create_children);
        }

        int failures = 0;

    private:
        native::button button;
        native::list list;
        native::combo_box combo;
        native::text_edit editor;
        native::list first, second;
        native::split_view split;

        bool create_children() {
            for (native::wnd *child : {static_cast<native::wnd *>(&button),
                    static_cast<native::wnd *>(&list),
                    static_cast<native::wnd *>(&combo),
                    static_cast<native::wnd *>(&editor),
                    static_cast<native::wnd *>(&split)}) {
                child->set_parent(this);
                child->create();
                child->show();
            }
            native::app::post([this] { check(); });
            return true;
        }

        void check() {
            using namespace linux::gemix;
            try {
                v_hide_c(runtime.vdi_handle);
                flush_repaints();
                check_color_indices(*this);
                if (colors_only) {
                    std::cout << "GEM displayed color checks passed\n";
                    destroy();
                    return;
                }
                if (editor_frame_only) {
                    check_code_editor_frame(*this);
                    std::cout << "GEM editor frame checks passed\n";
                    destroy();
                    return;
                }
                if (chrome_only) {
                    check_tab_joins(*this);
                    check_splitter_grips(*this);
                    std::cout << "GEM tab joins and splitter grip checks passed\n";
                    destroy();
                    return;
                }
                const auto initial = framebuffer();
                for (int y = 800; y < 808; ++y) {
                    for (int x = 600; x < 608; ++x) {
                        expect(pixel(initial, x, y) != pixel(initial, x + 1, y) &&
                               pixel(initial, x, y) != pixel(initial, x, y + 1),
                               "desktop checker is painted before any window movement");
                    }
                }
                check_scrollbar_arrows();
                check_text_origin(*this);
                check_code_editor_frame(*this);
                auto *tree = menu_tree_for(this);
                expect(tree && tree[tree[ROOT].ob_head].ob_next == tree[ROOT].ob_tail,
                       "menu bar and popups are root siblings");
                expect(tree[tree[ROOT].ob_tail].ob_next == ROOT,
                       "menu root sibling chain terminates at its parent");

                invalidate(native::rect(0, 0, 3, 4));
                auto *peer = window_states.object_from_handle(this);
                expect(peer && peer->dirty.p.x == 0 && peer->dirty.p.y == 0,
                       "root invalidation does not add the screen position");
                flush_repaints();

                const auto work = work_rect(wnd_bindings.handle_from_object(this));
                const auto appearance = native::theme::create(get_gpx());
                // The pinned SDK uses a 16-pixel font and a 20-pixel
                // gemctl application band (Gemscape/Ted), not a title strip.
                expect(runtime.char_h == 16 &&
                       appearance->get_status_bar_height() == 20,
                       "status height matches Gemscape and Ted's compact band");
                native::img status_surface(40, 22);
                auto status_theme = native::theme::create(status_surface.get_gpx());
                status_theme->draw_surface(native::rect(0, 0, 40, 22),
                    native::surface_kind::status_part, {});
                const auto status_colors = status_theme->native_palette();
                for (int x = 1; x < 40; ++x) {
                    expect(status_surface.pixels()[21 * 40 + x] == status_colors.button_bg,
                           "status part has no bottom border");
                    expect(status_surface.pixels()[x] == status_colors.button_border,
                           "status part retains its top separator");
                }
                for (int y = 1; y < 22; ++y) {
                    expect(status_surface.pixels()[y * 40 + 39] == status_colors.button_bg,
                           "status part has no right border");
                    expect(status_surface.pixels()[y * 40] == status_colors.button_border,
                           "status part retains its leading divider");
                }
                const auto before = framebuffer();
                runtime.pressed_button = &button;
                button.invalidate();
                flush_repaints();
                const auto held = framebuffer();
                expect(!pixel(before, work.x1() + 24, work.y1() + 24) &&
                       pixel(held, work.x1() + 24, work.y1() + 24),
                       "button press is visible before release");
                expect(pixel(before, 40, 36) == pixel(held, 40, 36),
                       "button repaint preserves window chrome");
                runtime.pressed_button = nullptr;
                button.invalidate();
                flush_repaints();

                auto *choice = combo_box_bindings.object_from_handle(&combo);
                choice->open = true;
                invalidate();
                flush_repaints();
                const auto popup = framebuffer();
                expect(pixel(popup, work.x1() + 170, work.y1() + 100),
                       "popup side border survives underlying list painting");
                choice->open = false;

                expect(focus_text_edit(this, native::point(25, 170)),
                       "click focuses the text field");
                expect(handle_text_edit_key(this, 0, (4 << 8) | 'a') &&
                       editor.get_text() == "aEditing",
                       "focused field accepts keyboard text");
                editor.select_all();
                expect(handle_text_edit_key(this, 4, (6 << 8) | 'c'),
                       "focused field handles Copy");
                editor.set_text("");
                expect(handle_text_edit_key(this, 4, (25 << 8) | 'v') &&
                       editor.get_text() == "aEditing",
                       "focused field pastes its copied selection");

                native::code_edit source("abc", 20, 200, 440, 130);
                source.set_parent(this);
                source.create();
                source.show();
                source.on_native_focus(true);
                expect(handle_collection_key(this, 4, (4 << 8) | 'a'),
                       "source editor handles Select All");
                expect(handle_collection_key(this, 0, (27 << 8) | 'x') &&
                       source.get_text() == "x", "source editor accepts text");
                expect(handle_collection_key(this, 0, (40 << 8) | 10) &&
                       source.get_text() == "x\n", "source editor accepts LF Enter");
                handle_collection_key(this, 0, (28 << 8) | 'y');
                expect(handle_collection_key(this, 0, 80 << 8),
                       "source editor accepts hosted Left");
                handle_collection_key(this, 0, (29 << 8) | 'z');
                expect(source.get_text() == "x\nzy", "Left moves one character");
                handle_collection_key(this, 4, (4 << 8) | 'a');
                handle_collection_key(this, 4, (6 << 8) | 'c');
                source.set_text("");
                handle_collection_key(this, 4, (25 << 8) | 'v');
                expect(source.get_text() == "x\nzy", "source clipboard round trip");
                source.destroy();

                const auto divider = split.get_splitter_bounds();
                const native::point start(20 + divider.x1() + 1, 240);
                expect(dispatch_drag_click(this, start, true), "splitter captures press");
                dispatch_drag_move(this, native::point(start.x + 70, start.y));
                dispatch_drag_click(this, native::point(start.x + 70, start.y), false);
                expect(split.get_ratio() > 0.6f, "splitter drag changes both pane geometry");

                native::modeless_wnd overlay(*this, "Overlay", 180, 90, 240, 140);
                invalidate();
                flush_repaints();
                const auto uncovered = framebuffer();
                overlay.create();
                overlay.show();
                const auto opening = framebuffer();
                const auto opening_work = work_rect(
                    wnd_bindings.handle_from_object(&overlay));
                for (int y = opening_work.y1() + 2; y < opening_work.y2() - 2; ++y)
                    for (int x = opening_work.x1() + 2; x < opening_work.x2() - 2; ++x)
                        expect(pixel(uncovered, x, y) == pixel(opening, x, y),
                               ("opening erased covered content at " +
                                std::to_string(x) + "," + std::to_string(y)).c_str());
                flush_repaints();
                const auto covered = framebuffer();
                invalidate();
                flush_repaints();
                const auto repainted = framebuffer();
                for (int y = 91; y < 230; ++y)
                    for (int x = 181; x < 420; ++x)
                        expect(pixel(covered, x, y) == pixel(repainted, x, y),
                               "owner controls cannot paint through an overlapping window");
                wind_update(BEG_UPDATE);
                overlay.destroy();
                expect(framebuffer() == repainted,
                       "window close respects an outer update boundary");
                wind_update(END_UPDATE);
                const auto immediate = framebuffer();
                // close() must restore our exposed content before publishing.
                flush_repaints();
                const auto restored = framebuffer();
                expect(immediate == restored,
                       "closing overlay publishes restored content, not desktop");
                for (int y = 90; y < 230; ++y)
                    for (int x = 180; x < 420; ++x)
                        expect(pixel(uncovered, x, y) == pixel(restored, x, y),
                               "exposed owner content is restored synchronously");
                for (int y = 37; y < 56; ++y)
                    for (int x = 40; x < 170; ++x)
                        expect(pixel(before, x, y) == pixel(restored, x, y),
                               "closing overlay restores the whole active title");

                native::modeless_wnd partial(*this, "Partial", 480, 90, 240, 140);
                native::modal_wnd modal(*this, "Modal", 180, 90, 240, 140);
                native::modal_wnd partial_modal(*this, "Partial modal",
                                                480, 90, 240, 140);
                canvas_popup canvas_editor(*this);
                for (native::app_wnd *opened : {
                        static_cast<native::app_wnd *>(&partial),
                        static_cast<native::app_wnd *>(&modal),
                        static_cast<native::app_wnd *>(&partial_modal),
                        static_cast<native::app_wnd *>(&canvas_editor)}) {
                    const auto previous = framebuffer();
                    opened->create();
                    opened->show();
                    const auto pending = framebuffer();
                    const auto interior = work_rect(
                        wnd_bindings.handle_from_object(opened));
                    for (int y = interior.y1() + 2; y < interior.y2() - 2; ++y)
                        for (int x = interior.x1() + 2; x < interior.x2() - 2; ++x)
                            expect(pixel(previous, x, y) == pixel(pending, x, y),
                                   "opening preserves pixels until client painting");
                    flush_repaints();
                    const auto painted = framebuffer();
                    expect(!pixel(painted, interior.x1() + 10, interior.y1() + 10),
                           "new window client paints over owner or desktop");
                    if (opened->get_modal() || !opened->get_native_title_visible()) {
                        const auto edge = outer_rect(wnd_bindings.handle_from_object(opened));
                        WORD kind = -1, ignored = 0;
                        wind_get(wnd_bindings.handle_from_object(opened), WF_KIND,
                            &kind, &ignored, &ignored, &ignored);
                        expect(kind == 0 && interior.x1() - edge.x1() == 4 &&
                            interior.y1() - edge.y1() == 4 &&
                            interior.w() == 240 && interior.h() == 140,
                            "titleless hosts reserve their frame outside requested client size");
                        for (int inset = 0; inset < 4; ++inset) {
                            const bool ink = inset != 1;
                            expect(pixel(painted, edge.x1() + 20, edge.y1() + inset) == ink &&
                                pixel(painted, edge.x1() + 20, edge.y2() - 1 - inset) == ink &&
                                pixel(painted, edge.x1() + inset, edge.y1() + 20) == ink &&
                                pixel(painted, edge.x2() - 1 - inset, edge.y1() + 20) == ink,
                                "all modal and custom canvas popup edges use the 1011 frame");
                        }
                    }
                    opened->destroy();
                }

                {
                    // The same panel > tabs > split > canvas nesting used
                    // by sprite editors must survive the complete paint pass.
                    native::app_wnd composed("Nested canvases", 40, 40, 480, 380);
                    native::panel host;
                    native::canvas upper, lower;
                    native::split_view panes(upper, lower,
                        native::split_orientation::vertical);
                    native::tab_view pages;
                    pages.set_tab_placement(native::tab_placement::bottom);
                    pages.add_item("Sheet", panes);
                    host.set_parent(&composed);
                    pages.set_parent(&host);
                    composed.create();
                    composed.show();
                    host.set_bounds(composed.get_client_bounds());
                    host.create();
                    host.show();
                    pages.set_bounds(host.get_client_bounds());
                    pages.create();
                    pages.show();
                    upper.on_wnd_paint.connect([](native::wnd_paint_event e) {
                        e.g.clear(native::rgba(255, 255, 255, 255));
                        e.g.set_ink(native::rgba(0, 0, 0, 255));
                        e.g.draw_rect(native::rect(12, 12, 20, 20), true);
                        return true;
                    });
                    lower.on_wnd_paint.connect([](native::wnd_paint_event e) {
                        e.g.clear(native::rgba(0, 0, 0, 255));
                        return true;
                    });
                    composed.invalidate();
                    flush_repaints();
                    const auto nested = framebuffer();
                    const auto nested_work = work_rect(
                        wnd_bindings.handle_from_object(&composed));
                    const auto top = root_bounds(upper);
                    const auto bottom = root_bounds(lower);
                    expect(pixel(nested, nested_work.x1() + top.x1() + 16,
                                 nested_work.y1() + top.y1() + 16) &&
                           !pixel(nested, nested_work.x1() + top.x1() + 40,
                                  nested_work.y1() + top.y1() + 16) &&
                           pixel(nested, nested_work.x1() + bottom.x1() + 16,
                                 nested_work.y1() + bottom.y1() + 16),
                           "tab backgrounds do not erase nested canvas pixels");
                    native::mouse_button received = native::mouse_button::none;
                    upper.on_mouse_click.connect([&received](native::mouse_event e) {
                        received = e.button;
                        return true;
                    });
                    for (auto button : {native::mouse_button::right,
                                        native::mouse_button::middle}) {
                        dispatch_surface_click(&composed,
                            native::point(top.x1() + 16, top.y1() + 16),
                            true, button);
                        expect(received == button,
                               "canvas dispatch preserves the secondary button");
                    }
                    composed.destroy();
                }

                expect(stock_text("Alternating\xe2\x80\xa6") == "Alternating...",
                       "UTF-8 ellipsis maps to printable GEM characters");
                native::img glyph(16, 20);
                glyph.get_gpx().clear(native::rgba(255, 255, 255, 255));
                expect(draw_stock_text(glyph, native::rect(0, 0, 16, 20), "A",
                                       native::point(), native::rgba(0, 0, 0, 255)),
                       "offscreen text loads the stock GEM font resource");
                destroy();
                const auto without_menu = framebuffer();
                expect(!menu_owner() && !menu_tree_for(this),
                       "destroying the window releases its installed menu");
                // AES retains its desktop system menu on current SDKs.
                // Compare Native's detach with an explicit native hide,
                // rather than requiring the OS-owned strip to disappear.
                menu_bar(nullptr, 0);
                WORD desktop_x, desktop_y, desktop_w, desktop_h;
                wind_get(0, WF_WORKXYWH, &desktop_x, &desktop_y,
                         &desktop_w, &desktop_h); // also fences gemd drawing
                const auto desktop = framebuffer();
                for (int y = 0; y < std::max(32, int(desktop_y)); ++y)
                    for (int x = 0; x < 904; ++x)
                        expect(pixel(without_menu, x, y) == pixel(desktop, x, y),
                               "Native menu detach matches AES desktop restoration");
                for (int y = 800; y < 808; ++y)
                    for (int x = 600; x < 608; ++x)
                        expect(pixel(desktop, x, y) != pixel(desktop, x + 1, y),
                               "menu detach preserves the desktop below its system strip");
                std::cout << "GEM runtime regression checks passed\n";
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n';
                failures = 1;
            }
            runtime.pressed_button = nullptr;
            destroy();
        }
    };
}

int program(int argc, char **argv) {
    colors_only = argc > 1 && std::string(argv[1]) == "--colors-only";
    editor_frame_only = argc > 1 &&
        std::string(argv[1]) == "--editor-frame-only";
    chrome_only = argc > 1 && std::string(argv[1]) == "--chrome-only";
    test_window window;
    const int result = native::app::run(window);
    return result ? result : window.failures;
}

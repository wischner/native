//
// Compares GTK/Granite controls and theme primitives in light/dark
// states.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "native_gtk_theme_tests.h"
#include "../lib/native/gpx_wnd.h"
#include "../lib/native/toolkits/wayland/theme_snapshot.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
namespace
{
    // Read the reference Cairo target without using the theme's image
    // path.
    std::unique_ptr<native::img>
    read_surface(cairo_surface_t *surface, int w, int h) {
        cairo_surface_flush(surface);
        auto result = std::make_unique<native::img>(w, h);
        const auto *pixels = reinterpret_cast<const std::uint32_t *>(
            cairo_image_surface_get_data(surface));
        const int stride = cairo_image_surface_get_stride(surface) / 4;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto pixel = pixels[y * stride + x];
                const unsigned a = pixel >> 24;
                result->pixels()[y * w + x] =
                    a ? native::rgba(((pixel >> 16) & 255) * 255 / a,
                                     ((pixel >> 8) & 255) * 255 / a,
                                     (pixel & 255) * 255 / a,
                                     a)
                      : native::rgba(0, 0, 0, 0);
            }
        return result;
    }
    // Supply a 2x frame to the actual window gpx backend, including its
    // offset.
    class window_frame
    {
    public:
        window_frame(native::app_wnd &window, int w, int h)
            : _peer(linux::wayland::state(window))
            , _graphics(&window, native::point(7, 9))
            , _surface(cairo_image_surface_create(
                  CAIRO_FORMAT_ARGB32, (w + 14) * 2, (h + 18) * 2))
            , _context(cairo_create(_surface))
            , _previous(_peer.context) {
            _peer.context = _context;
            cairo_scale(_context, 2, 2);
            _graphics.set_clip(native::rect(0, 0, w, h));
        }
        ~window_frame() {
            _peer.context = _previous;
            cairo_destroy(_context);
            cairo_surface_destroy(_surface);
        }
        native::gpx &graphics() {
            return _graphics;
        }
        std::unique_ptr<native::img> image() {
            return read_surface(
                _surface,
                cairo_image_surface_get_width(_surface),
                cairo_image_surface_get_height(_surface));
        }

    private:
        linux::wayland::widget_state &_peer;
        native::gpx_wnd _graphics;
        cairo_surface_t *_surface;
        cairo_t *_context;
        cairo_t *_previous;
    };
    // Capture the live widget directly through its actual parent,
    // without routing the reference image through the theme
    // implementation.
    std::unique_ptr<native::img> capture(GtkWidget *widget,
                                         int w,
                                         int h,
                                         int scale = 1,
                                         bool frame = false,
                                         bool clipped = false) {
        auto *snapshot = gtk_snapshot_new();
        graphene_rect_t bounds{};
        if (!gtk_widget_compute_bounds(
                widget, gtk_widget_get_parent(widget), &bounds))
            throw std::runtime_error(
                "Native control has no parent bounds.");
        GtkBorder margin{};
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_style_context_get_margin(
            gtk_widget_get_style_context(widget), &margin);
        G_GNUC_END_IGNORE_DEPRECATIONS
        const graphene_point_t translation{
            -bounds.origin.x + margin.left,
            -bounds.origin.y + margin.top};
        gtk_snapshot_translate(snapshot, &translation);
        gtk_widget_snapshot_child(
            gtk_widget_get_parent(widget), widget, snapshot);
        auto *node = gtk_snapshot_free_to_node(snapshot);
        if (!node)
            throw std::runtime_error(
                "Native control has no render node.");
        auto *surface =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                       (w + (frame ? 14 : 0)) * scale,
                                       (h + (frame ? 18 : 0)) * scale);
        auto *cr = cairo_create(surface);
        cairo_scale(cr, scale, scale);
        if (frame) {
            cairo_translate(cr, 7, 9);
            cairo_rectangle(cr,
                            clipped ? 5 : 0,
                            clipped ? 3 : 0,
                            clipped ? w - 10 : w,
                            clipped ? h - 6 : h);
            cairo_clip(cr);
            cairo_rectangle(cr, 0, 0, w, h);
            cairo_clip(cr);
        }
        gsk_render_node_draw(node, cr);
        auto result =
            read_surface(surface,
                         cairo_image_surface_get_width(surface),
                         cairo_image_surface_get_height(surface));
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        gsk_render_node_unref(node);
        return result;
    }
    // Compare all channels, including transparent rounded corners and
    // shadows.
    void compare(const native::img &expected,
                 const native::img &actual,
                 const char *name,
                 int state,
                 bool dark) {
        std::size_t different = 0;
        const int count = expected.w() * expected.h();
        for (int i = 0; i < count; ++i)
            if (std::memcmp(expected.pixels() + i,
                            actual.pixels() + i,
                            sizeof(native::rgba)))
                ++different;
        if (different) {
            if (const char *path =
                    std::getenv("NATIVE_GTK_THEME_SNAPSHOT")) {
                const auto directory =
                    std::filesystem::path(path).parent_path();
                expected.save(directory / "gtk-theme-expected.png");
                actual.save(directory / "gtk-theme-actual.png");
            }
            std::cerr << name << " state=" << state << " dark=" << dark
                      << " differing pixels=" << different << '\n';
            throw std::runtime_error(
                "Theme primitive differs from native GTK.");
        }
    }
    // Restore application desktop settings even if one pixel assertion
    // fails.
    class test_settings
    {
    public:
        test_settings() {
            g_object_get(gtk_settings_get_default(),
                         "gtk-enable-animations",
                         &_animations,
                         "gtk-application-prefer-dark-theme",
                         &_dark,
                         nullptr);
            g_object_set(gtk_settings_get_default(),
                         "gtk-enable-animations",
                         false,
                         nullptr);
        }
        ~test_settings() {
            g_object_set(gtk_settings_get_default(),
                         "gtk-enable-animations",
                         _animations,
                         "gtk-application-prefer-dark-theme",
                         _dark,
                         nullptr);
        }

    private:
        gboolean _animations = false, _dark = false;
    };
} // namespace
void test_native_gtk_theme(native::app_wnd &window) {
    using namespace native;
    using linux::wayland::state;
    using linux::wayland::theme_snapshot;
    test_settings settings;
    std::unique_ptr<img> proof;
    if (std::getenv("NATIVE_GTK_THEME_SNAPSHOT"))
        proof = std::make_unique<img>(400, 940);
    std::uint64_t light_hash = 0, dark_hash = 0;
    for (bool dark : {false, true}) {
        g_object_set(gtk_settings_get_default(),
                     "gtk-application-prefer-dark-theme",
                     dark,
                     nullptr);
        if (proof) {
            auto appearance = theme::create(proof->get_gpx());
            auto &g = proof->get_gpx();
            const int y = dark ? 470 : 0;
            g.set_clip(rect(0, y, 400, 470));
            g.clear(appearance->get_menu_bar_background_color());
            g.set_ink(appearance->get_content_foreground_color());
            g.draw_text(dark ? "Native (dark)" : "Native (light)",
                        point(10, y + 3));
            g.draw_text("Theme primitive", point(210, y + 3));
        }
        for (int interaction = 0; interaction < 6; ++interaction) {
            theme::state value;
            value.hot = interaction == 1;
            value.pressed = interaction == 2;
            value.disabled = interaction == 3;
            value.focused = interaction == 4;
            value.active = interaction != 5;
            button push("Native parity", 0, 0, 180, 40);
            check tick("Native parity", 0, 0, 180, 40);
            radio option("Native parity", 0, 0, 180, 40);
            text_edit entry(
                "", text_edit_mode::single_line, 0, 0, 180, 40);
            tick.set_checked(true);
            option.set_selected(true);
            list choices({"First", "Second"}, 0, 0, 180, 80);
            choices.set_selected_index(1);
            std::array<wnd *, 5> controls{
                &push, &tick, &option, &entry, &choices};
            int item = 0;
            for (auto *control : controls) {
                const int height = control == &choices ? 80 : 40;
                control->set_parent(&window);
                control->create();
                control->show();
                theme_snapshot::apply_state(state(*control).widget,
                                            value);
                gtk_widget_set_sensitive(state(*control).widget,
                                         !value.disabled);
                gtk_widget_allocate(
                    state(*control).widget, 180, height, -1, nullptr);
                auto expected =
                    capture(state(*control).widget, 180, height);
                img actual(180, height);
                auto theme = theme::create(actual.get_gpx());
                auto selected = value;
                selected.selected = true;
                auto draw = [&](native::theme &appearance) {
                    if (control == &push)
                        appearance.draw_button(
                            {0, 0, 180, 40}, "Native parity", value);
                    else if (control == &tick)
                        appearance.draw_check(
                            {0, 0, 180, 40}, "Native parity", selected);
                    else if (control == &option)
                        appearance.draw_radio(
                            {0, 0, 180, 40}, "Native parity", selected);
                    else if (control == &choices)
                        appearance.draw_list(rect(0, 0, 180, height),
                                             {"First", "Second"},
                                             1,
                                             value);
                    else
                        appearance.draw_text_edit_frame({0, 0, 180, 40},
                                                        value);
                };
                draw(*theme);
                compare(*expected,
                        actual,
                        control == &push      ? "button"
                        : control == &tick    ? "check"
                        : control == &option  ? "radio"
                        : control == &choices ? "list"
                                              : "entry",
                        interaction,
                        dark);
                if (interaction == 0) {
                    auto expected2x = capture(
                        state(*control).widget, 180, height, 2, true);
                    window_frame frame(window, 180, height);
                    auto vector_theme =
                        native::theme::create(frame.graphics());
                    draw(*vector_theme);
                    auto vector_pixels = frame.image();
                    compare(*expected2x,
                            *vector_pixels,
                            "HiDPI vector",
                            interaction,
                            dark);
                    if (control == &push) {
                        window_frame clipped(window, 180, height);
                        clipped.graphics().set_clip(
                            rect(5, 3, 170, height - 6));
                        auto clipped_theme =
                            native::theme::create(clipped.graphics());
                        draw(*clipped_theme);
                        auto clipped_pixels = clipped.image();
                        expected2x = capture(state(*control).widget,
                                             180,
                                             height,
                                             2,
                                             true,
                                             true);
                        compare(*expected2x,
                                *clipped_pixels,
                                "HiDPI clipped vector",
                                interaction,
                                dark);
                    }
                }
                if (control == &push && interaction == 0) {
                    std::uint64_t hash = 1469598103934665603ULL;
                    const auto *bytes =
                        reinterpret_cast<const unsigned char *>(
                            expected->pixels());
                    for (std::size_t i = 0;
                         i < 180 * height * sizeof(rgba);
                         ++i)
                        hash = (hash ^ bytes[i]) * 1099511628211ULL;
                    (dark ? dark_hash : light_hash) = hash;
                }
                if (proof && interaction == 0) {
                    const int y = (dark ? 470 : 0) + 25 + item * 85;
                    auto &g = proof->get_gpx();
                    g.set_clip(rect(0, y, 400, 85));
                    g.clear(theme->native_palette().menu_bar_bg);
                    g.draw_img(*expected, point(10, y + 3));
                    g.draw_img(actual, point(210, y + 3));
                }
                if (control == &tick || control == &option) {
                    // Compare steady unchecked controls, not a live CSS
                    // transition which has not reached a frame
                    // boundary.
                    check off_check("Native parity", 0, 0, 180, 40);
                    radio off_radio("Native parity", 0, 0, 180, 40);
                    wnd *off = control == &tick
                                   ? static_cast<wnd *>(&off_check)
                                   : static_cast<wnd *>(&off_radio);
                    off->set_parent(&window);
                    off->create();
                    off->show();
                    theme_snapshot::apply_state(state(*off).widget,
                                                value);
                    gtk_widget_set_sensitive(state(*off).widget,
                                             !value.disabled);
                    gtk_widget_allocate(
                        state(*off).widget, 180, 40, -1, nullptr);
                    auto unchecked =
                        capture(state(*off).widget, 180, 40);
                    actual.get_gpx().clear(rgba(0, 0, 0, 0));
                    if (control == &tick)
                        theme->draw_check(
                            {0, 0, 180, 40}, "Native parity", value);
                    else
                        theme->draw_radio(
                            {0, 0, 180, 40}, "Native parity", value);
                    compare(*unchecked,
                            actual,
                            control == &tick ? "unchecked check"
                                             : "unchecked radio",
                            interaction,
                            dark);
                }
                ++item;
            }
        }
    }
    if (!std::getenv("GTK_THEME") && light_hash == dark_hash)
        throw std::runtime_error("Light/dark preference must exercise "
                                 "distinct native styles.");
    if (proof)
        proof->save(std::getenv("NATIVE_GTK_THEME_SNAPSHOT"));
    std::cout << "GTK theme parity: 96 live-control pixel comparisons "
                 "passed.\n";
}

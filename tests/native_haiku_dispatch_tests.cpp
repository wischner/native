//
// Stresses serialization of Haiku window callbacks and posted UI work across
// two loopers, while resizing, replacing shader sources and closing windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/platforms/haiku/globals.h"
#include <Autolock.h>
#include <Bitmap.h>
#include <Screen.h>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

namespace
{
    // Exercise the real Haiku graphics adapter, including partial damage.
    void check_code_editor_border(native::app_wnd &root) {
        native::code_edit editor("FIRST\nSECOND\nTHIRD", 0, 0, 160, 64);
        editor.set_parent(&root);
        editor.set_read_only(true).set_show_line_numbers(false);
        editor.create();
        BBitmap buffer(BRect(0, 0, 159, 63),
                       B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
        if (buffer.InitCheck() != B_OK || !buffer.Lock())
            throw std::runtime_error("Haiku border bitmap unavailable");
        auto *target = new BView(buffer.Bounds(), "border_probe",
                                 B_FOLLOW_NONE, B_WILL_DRAW);
        buffer.AddChild(target);
        {
            const haiku::scoped_gpx_target redirect(editor, target);
            auto &graphics = editor.get_gpx();
            auto pixel = [&](int x, int y) {
                const auto *row = static_cast<const unsigned char *>(buffer.Bits()) +
                                  y * buffer.BytesPerRow();
                return unsigned(row[x * 4]) | (unsigned(row[x * 4 + 1]) << 8) |
                       (unsigned(row[x * 4 + 2]) << 16);
            };
            for (auto sides : {native::border_sides::all,
                               native::border_sides::top,
                               native::border_sides::none}) {
                editor.set_border_sides(sides);
                const bool bordered = native::has_border(sides, native::border_sides::top);
                unsigned reference = 0;
                for (int pass = 0; pass < 12; ++pass) {
                    editor.go_to_line(pass % 2);
                    editor.on_native_focus(pass % 3 == 0);
                    const native::rect damage = pass % 3 == 1
                        ? native::rect(30, 0, 100, 2)
                        : native::rect(0, 0, 160, 64);
                    graphics.set_clip(damage).clear({173, 23, 47, 255});
                    editor.on_native_paint(native::wnd_paint_event(damage, graphics));
                    target->Sync();
                    const unsigned current = pixel(80, 0);
                    if (pass == 0) reference = current;
                    if (bordered && current != reference)
                        throw std::runtime_error("Haiku code top border changed during repaint");
                    if (bordered && current == pixel(80, 4))
                        throw std::runtime_error("Haiku code top border was erased");
                }
            }
        }
        buffer.Unlock();
        editor.destroy();
    }

    void check_scroll_input(native::app_wnd &root) {
        native::canvas surface(0, 0, 160, 100);
        surface.set_parent(&root);
        surface.set_content_bounds({0, 0, 2000, 100000});
        surface.create();
        surface.set_scroll_position({100, 50000});
        auto *host = haiku::view_from_control(&surface);
        unsigned events = 0;
        surface.on_mouse_wheel.connect([&](native::mouse_wheel_event) {
            ++events;
            return false;
        });
        auto wheel = [](BHandler *target, float x, float y) {
            BMessage message(B_MOUSE_WHEEL_CHANGED);
            message.AddFloat("be:wheel_delta_x", x);
            message.AddFloat("be:wheel_delta_y", y);
            target->MessageReceived(&message);
        };
        wheel(host, 1, 1);
        if (surface.get_scroll_position().x != 124 ||
            surface.get_scroll_position().y != 50024 || events != 2)
            throw std::runtime_error("Haiku wheel down/right reversed");
        wheel(host, -0.5f, -1);
        if (surface.get_scroll_position().x != 112 ||
            surface.get_scroll_position().y != 50000)
            throw std::runtime_error("Haiku wheel up/left reversed");
        const unsigned before = events;
        wheel(host, 0, 0);
        if (events != before)
            throw std::runtime_error("Haiku zero wheel emitted movement");
        // Programmatic memory navigation must retain full 32-bit positions.
        surface.set_scroll_position({0, 80000});
        if (surface.get_scroll_position().y != 80000)
            throw std::runtime_error("Haiku canvas navigation narrowed");
        surface.destroy();

        native::code_edit editor("", native::rect(0, 0, 160, 100));
        editor.set_parent(&root);
        std::string listing;
        for (int row = 0; row < 100; ++row) listing += "row\n";
        editor.set_text(listing);
        editor.create();
        editor.go_to_line(50);
        int top = -1;
        editor.on_hover.connect([&](native::text_span span) {
            top = editor.line_at(span.start);
            return false;
        });
        auto first_row = [&] {
            editor.on_native_mouse_move(native::point(40, 1));
            return top;
        };
        const int initial = first_row();
        host = haiku::view_from_control(&editor);
        wheel(host, 0, 1);
        if (first_row() != initial + 1)
            throw std::runtime_error("Haiku code wheel down reversed");
        wheel(host, 0, -1);
        if (first_row() != initial)
            throw std::runtime_error("Haiku code wheel up reversed");
        editor.go_to_line(0);
        if (first_row() != 0)
            throw std::runtime_error("Haiku code navigation did not reveal");
        editor.destroy();

        native::coord horizontal = 0, vertical = 0;
        const auto connection = root.on_mouse_wheel.connect(
            [&](native::mouse_wheel_event event) {
                (event.direction == native::wheel_direction::horizontal
                     ? horizontal : vertical) = event.delta;
                return false;
            });
        wheel(haiku::wnd_bindings.handle_from_object(&root), 1, 1);
        root.on_mouse_wheel.disconnect(connection);
        if (horizontal >= 0 || vertical >= 0)
            throw std::runtime_error("Haiku window wheel direction reversed");
    }

    std::atomic<unsigned> active = 0;
    std::atomic<bool> overlap = false;
    thread_local unsigned depth = 0;
    // Widen each callback's overlap window without depending on a race
    // detector.
    struct callback_probe
    {
        callback_probe() {
            if (depth++ == 0 && active.fetch_add(1) != 0)
                overlap = true;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        ~callback_probe() {
            if (--depth == 0)
                --active;
        }
    };
} // namespace
int program(int, char **) {
    native::app_wnd root("Haiku UI serialization", 40, 40, 260, 140);
    native::menu_items_proxy items;
    items << std::pair<int, std::string>(1, "Item");
    root.menu << "Menu" << items;
    native::modeless_wnd second(root, "Second looper", 320, 40, 200, 100);
    native::shader_view view(native::rect(0, 0, 32, 32));
    view.set_parent(&root);
    native::canvas paint_probe(50, 10, 32, 32);
    paint_probe.set_parent(&root);
    unsigned probe_paints = 0;
    bool probe_checked = false, pixels_preserved = false;
    paint_probe.on_wnd_paint.connect([&](native::wnd_paint_event event) {
        if (probe_paints++ > 0 && !probe_checked) {
            auto *host = haiku::view_from_control(&paint_probe);
            host->Sync();
            BRect sample(8, 8, 8, 8);
            host->ConvertToScreen(&sample);
            BBitmap pixel(BRect(0, 0, 0, 0), B_RGB32);
            BScreen screen(host->Window());
            if (pixel.InitCheck() == B_OK &&
                screen.ReadBitmap(&pixel, false, &sample) == B_OK) {
                const auto *bytes = static_cast<const unsigned char *>(pixel.Bits());
                pixels_preserved = bytes[0] == 47 && bytes[1] == 23 && bytes[2] == 173;
            }
            probe_checked = true;
        }
        event.g.clear(native::rgba(173, 23, 47, 255));
        return false;
    });
    std::optional<native::ui_dispatch_scope> delivery;
    std::optional<std::jthread> producer;
    unsigned first_paints = 0, second_paints = 0, updates = 0;
    root.on_wnd_paint.connect([&](auto) {
        const callback_probe probe;
        ++first_paints;
        return false;
    });
    second.on_wnd_paint.connect([&](auto) {
        const callback_probe probe;
        ++second_paints;
        return false;
    });
    root.on_wnd_resize.connect([&](auto) {
        const callback_probe probe;
        return false;
    });
    root.on_wnd_create.connect([&] {
        auto *window = haiku::wnd_bindings.handle_from_object(&root);
        BAutolock lock(window);
        if (!lock.IsLocked()) throw std::runtime_error("Haiku window lock failed");
        auto *content = haiku::content_view(window);
        for (int pass = 0; pass < 2; ++pass) {
            root.set_dimensions(native::size(260, 140));
            const auto bounds = content->Bounds();
            if (bounds.Width() + 1 != 260 || bounds.Height() + 1 != 140)
                throw std::runtime_error("Haiku menu consumes requested client size");
        }
        if (auto *focus = window->CurrentFocus()) focus->MakeFocus(false);
        window->WindowActivated(true);
        if (window->CurrentFocus() != content)
            throw std::runtime_error("Haiku activation did not focus the client");
        BView control(BRect(0, 0, 10, 10), "focus_probe", B_FOLLOW_NONE, B_NAVIGABLE);
        content->AddChild(&control);
        control.MakeFocus(true);
        window->WindowActivated(true);
        const bool retained = window->CurrentFocus() == &control;
        control.RemoveSelf();
        if (!retained) throw std::runtime_error("Haiku activation stole control focus");
        check_code_editor_border(root);
        check_scroll_input(root);
        view.create();
        view.show();
        paint_probe.create();
        paint_probe.show();
        second.create();
        second.show();
        delivery.emplace(root);
        producer.emplace([sender = delivery->sender(), &view, &second,
                          &paint_probe, &probe_checked,
                          &updates, &first_paints,
                          &second_paints](std::stop_token stop) {
            for (unsigned i = 0; i < 100 && !stop.stop_requested(); ++i) {
                sender.post(
                    [i, &view, &second, &updates, &paint_probe](native::wnd &target) {
                        const callback_probe probe;
                        native::img source(2, 2);
                        view.set_source(source, i, i / 100.0);
                        target.set_dimensions(
                            native::size(260 + i % 3, 140 + i % 2));
                        target.invalidate();
                        second.invalidate();
                        paint_probe.invalidate();
                        ++updates;
                    });
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!stop.stop_requested()) {
                if (sender.post_latest([deadline, &first_paints,
                                        &second_paints,
                                        &second, &paint_probe, &probe_checked](native::wnd &target) {
                        if ((first_paints && second_paints && probe_checked) ||
                            std::chrono::steady_clock::now() > deadline)
                            target.destroy();
                        else {
                            target.invalidate();
                            second.invalidate();
                            paint_probe.invalidate();
                        }
                    }) == native::ui_post_result::closed)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        return false;
    });
    native::app::run(root);
    if (delivery)
        delivery->close();
    if (producer)
        producer->request_stop();
    // The producer never dereferences its borrowed controls; all such access
    // lives in guarded UI deliveries, closed before these stack objects
    // leave.
    const bool passed =
        !overlap && updates == 100 && first_paints && second_paints &&
        probe_checked && pixels_preserved;
    if (!passed)
        std::cerr << "Haiku overlap=" << overlap << " updates=" << updates
                  << " paints=" << first_paints << ',' << second_paints
                  << " repaint_checked=" << probe_checked
                  << " pixels_preserved=" << pixels_preserved
                  << '\n';
    return passed ? 0 : 1;
}

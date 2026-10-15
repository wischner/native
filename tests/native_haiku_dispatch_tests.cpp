//
// Stresses serialization of Haiku window callbacks and posted UI work across
// two loopers, while resizing, replacing shader sources and closing windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

namespace
{
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
    native::modeless_wnd second(root, "Second looper", 320, 40, 200, 100);
    native::shader_view view(native::rect(0, 0, 32, 32));
    view.set_parent(&root);
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
        view.create();
        view.show();
        second.create();
        second.show();
        delivery.emplace(root);
        producer.emplace([sender = delivery->sender(), &view, &second,
                          &updates, &first_paints,
                          &second_paints](std::stop_token stop) {
            for (unsigned i = 0; i < 100 && !stop.stop_requested(); ++i) {
                sender.post(
                    [i, &view, &second, &updates](native::wnd &target) {
                        const callback_probe probe;
                        native::img source(2, 2);
                        view.set_source(source, i, i / 100.0);
                        target.set_dimensions(
                            native::size(260 + i % 3, 140 + i % 2));
                        target.invalidate();
                        second.invalidate();
                        ++updates;
                    });
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!stop.stop_requested()) {
                if (sender.post_latest([deadline, &first_paints,
                                        &second_paints,
                                        &second](native::wnd &target) {
                        if ((first_paints && second_paints) ||
                            std::chrono::steady_clock::now() > deadline)
                            target.destroy();
                        else {
                            target.invalidate();
                            second.invalidate();
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
        !overlap && updates == 100 && first_paints && second_paints;
    if (!passed)
        std::cerr << "Haiku overlap=" << overlap << " updates=" << updates
                  << " paints=" << first_paints << ',' << second_paints
                  << '\n';
    return passed ? 0 : 1;
}

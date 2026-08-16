//
// Exercises live modeless and nested modal windows through the hosted
// SDL2 dummy video driver without requiring a display server.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <iostream>
#include <string>

#include <native.h>

namespace
{
    int failure_count = 0;

    // Record a failed condition without stopping cleanup checks.
    void expect(bool condition, const std::string &description) {
        if (condition)
            return;

        std::cerr << "FAILED: " << description << '\n';
        ++failure_count;
    }

    // Exercise owner exclusion, direct modal stacking, and results.
    void test_modal_stack() {
        native::app_wnd owner(
            "Owner", native::rect(10, 10, 320, 200));
        native::modeless_wnd palette(
            owner, "Palette", native::rect(350, 10, 160, 200));
        native::modal_wnd first(
            owner, "First", native::rect(30, 30, 200, 120));
        native::modal_wnd second(
            owner, "Second", native::rect(50, 50, 200, 120));

        owner.create();
        owner.show();
        palette.create();
        palette.show();
        first.create();
        first.show();

        expect(owner.get_active_modal() == &first,
               "showing a dialog starts its modal session");
        expect(!owner.get_input_enabled() &&
                   !palette.get_input_enabled() &&
                   first.get_input_enabled(),
               "the first modal blocks its owner and sibling");

        int close_events = 0;
        native::dialog_result last_result =
            native::dialog_result::none;
        second.on_modal_close.connect(
            [&](native::dialog_result result) {
                ++close_events;
                last_result = result;
                return false;
            });

        second.create();
        second.show();
        expect(owner.get_active_modal() == &second &&
                   !first.get_input_enabled() &&
                   second.get_input_enabled(),
               "a newer direct modal becomes the active stack entry");

        second.close(native::dialog_result::accepted);
        expect(owner.get_active_modal() == &first &&
                   first.get_input_enabled() &&
                   !owner.get_input_enabled(),
               "closing the top modal restores the previous dialog");
        expect(close_events == 1 &&
                   last_result == native::dialog_result::accepted &&
                   second.get_result() ==
                       native::dialog_result::accepted,
               "an accepted modal result is delivered exactly once");

        first.destroy();
        expect(owner.get_active_modal() == nullptr &&
                   owner.get_input_enabled() &&
                   palette.get_input_enabled() &&
                   first.get_result() ==
                       native::dialog_result::cancelled,
               "destroying an unfinished modal cancels and restores");

        palette.destroy();
        owner.destroy();
    }
} // namespace

int main() {
    try {
        test_modal_stack();
    } catch (const std::exception &error) {
        std::cerr << "FAILED: unexpected exception: " << error.what()
                  << '\n';
        ++failure_count;
    }
    return failure_count == 0 ? 0 : 1;
}

//
// Exercises live modeless and nested modal windows through the hosted
// SDL2 dummy video driver without requiring a display server.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include <SDL2/SDL.h>

#include <native.h>

namespace linux::sdl2
{
    bool handle_text_edit_mouse(
        native::wnd *parent, int x, int y, bool pressed);
    bool handle_text_edit_key(
        native::wnd *parent, const SDL_KeyboardEvent &event);
} // namespace linux::sdl2

namespace
{
    int failure_count = 0;

    // Send one key press through the SDL editor event adapter.
    bool send_editor_key(native::wnd *parent,
                         SDL_Keycode key,
                         SDL_Keymod modifiers = KMOD_NONE) {
        SDL_KeyboardEvent event = {};
        event.type = SDL_KEYDOWN;
        event.keysym.sym = key;
        event.keysym.mod = static_cast<Uint16>(modifiers);
        return linux::sdl2::handle_text_edit_key(parent, event);
    }

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

    // Exercise the SDL system clipboard and emulated editor through
    // their public contracts while the dummy video service is live.
    void test_clipboard_and_text_edit() {
        native::app_wnd owner(
            "Editors", native::rect(10, 10, 320, 200));
        owner.create();

        native::img image(2, 1);
        image.pixels()[0] = native::rgba(10, 20, 30, 40);
        image.pixels()[1] = native::rgba(50, 60, 70, 255);
        native::clipboard output = native::clipboard::open_write();
        output.write_text("clip \xc3\xa6\ntext")
            .write_image(image)
            .commit();
        native::clipboard input = native::clipboard::open_read();
        expect(input.has(native::clipboard_format::text) &&
                   input.has(native::clipboard_format::image) &&
                   input.read_text() == "clip \xc3\xa6\ntext",
               "clipboard preserves a multi-format UTF-8 snapshot");
        std::uint8_t text_slice[4] = {};
        expect(output.get_committed() &&
                   input.size(native::clipboard_format::text) == 12 &&
                   input.read(native::clipboard_format::text,
                              8,
                              text_slice,
                              sizeof(text_slice)) == 4 &&
                   std::string(
                       reinterpret_cast<const char *>(text_slice),
                       sizeof(text_slice)) == "text",
               "clipboard exposes committed state and bounded reads");
        native::img copied_image = input.read_image();
        expect(copied_image.w() == 2 && copied_image.h() == 1 &&
                   static_cast<std::uint32_t>(
                       copied_image.pixels()[0]) ==
                       static_cast<std::uint32_t>(image.pixels()[0]),
               "clipboard PNG round trip preserves RGBA pixels");

        SDL_SetClipboardText("one\r\ntwo\rthree");
        native::clipboard native_lines =
            native::clipboard::open_read();
        expect(native_lines.read_text() == "one\ntwo\nthree" &&
                   !native_lines.has(
                       native::clipboard_format::image),
               "clipboard normalizes lines and observes ownership "
               "loss");

        native::clipboard previous = native::clipboard::open_write();
        previous.write_text("preserved").commit();
        bool failed_commit = false;
        try {
            native::clipboard invalid =
                native::clipboard::open_write();
            invalid.commit();
        } catch (const std::logic_error &) {
            failed_commit = true;
        }
        expect(failed_commit &&
                   native::clipboard::open_read().read_text() ==
                       "preserved",
               "a rejected commit preserves prior clipboard data");

        {
            native::clipboard abandoned =
                native::clipboard::open_write();
            abandoned.write_text("discarded");
        }
        expect(native::clipboard::open_read().read_text() ==
                   "preserved",
               "an abandoned write leaves the clipboard unchanged");

        native::clipboard empty_output =
            native::clipboard::open_write();
        empty_output.write_text("").commit();
        native::clipboard empty_input = native::clipboard::open_read();
        expect(empty_input.has(native::clipboard_format::text) &&
                   empty_input.read_text().empty() &&
                   !empty_input.has(native::clipboard_format::image),
               "clipboard preserves an advertised empty text value");

        native::text_edit source("Copy \xc3\xa6");
        source.set_parent(&owner);
        source.create();
        source.show();
        int source_changes = 0;
        source.on_change.connect([&](const std::string &) {
            ++source_changes;
            return false;
        });
        source.select_all();
        expect(source.copy(),
               "direct editor copy publishes the current selection");

        native::text_edit target;
        target.set_parent(&owner);
        target.set_validator([](const std::string &text) {
            return text != "Rejected";
        });
        int changes = 0;
        target.on_change.connect([&](const std::string &) {
            ++changes;
            return false;
        });
        target.create();
        target.show();
        expect(target.paste() && target.get_text() == "Copy \xc3\xa6" &&
                   changes == 1,
               "direct editor paste validates, changes, and emits");

        source.select_all();
        expect(source.cut() && source.get_text().empty() &&
                   source_changes == 1,
               "direct editor cut commits before removing selection");
        source.set_read_only(true);
        source.select_all();
        expect(!source.cut() && !source.paste() &&
                   source_changes == 1,
               "read-only editors reject cut and paste");

        native::clipboard rejected = native::clipboard::open_write();
        rejected.write_text("Rejected").commit();
        target.select_all();
        expect(!target.paste() &&
                   target.get_text() == "Copy \xc3\xa6" &&
                   changes == 1,
               "live validation rejects pasted complete values");

        native::text_edit keyboard(
            "A\xc3\xa6" "B",
            native::text_edit_mode::single_line,
            native::rect(0, 60, 180, 28));
        keyboard.set_parent(&owner);
        keyboard.create();
        keyboard.show();
        expect(linux::sdl2::handle_text_edit_mouse(
                   &owner, 170, 70, true) &&
                   linux::sdl2::handle_text_edit_mouse(
                       &owner, 170, 70, false),
               "pointer input focuses the SDL editor");
        expect(send_editor_key(&owner, SDLK_LEFT) &&
                   send_editor_key(&owner, SDLK_BACKSPACE) &&
                   keyboard.get_text() == "AB",
               "keyboard navigation deletes a complete UTF-8 scalar");
        expect(send_editor_key(&owner, SDLK_a, KMOD_CTRL) &&
                   send_editor_key(&owner, SDLK_x, KMOD_CTRL) &&
                   keyboard.get_text().empty() &&
                   send_editor_key(&owner, SDLK_v, KMOD_CTRL) &&
                   keyboard.get_text() == "AB",
               "standard select, cut, and paste shortcuts are routed");

        keyboard.destroy();
        target.destroy();
        source.destroy();
        owner.destroy();
    }

    // Exercise chooser cancellation without a desktop helper.
    void test_unavailable_file_dialogs() {
        native::app_wnd owner(
            "File panels", native::rect(10, 10, 320, 200));
        native::open_file_dialog open(owner);
        native::save_file_dialog save(owner);

        const char *old_path_value = std::getenv("PATH");
        const bool had_path = old_path_value != nullptr;
        const std::string old_path = had_path ? old_path_value : "";
        setenv("PATH", "/native/no-file-choosers", 1);

        int cancelled = 0;
        open.on_modal_close.connect(
            [&](native::dialog_result result) {
                if (result == native::dialog_result::cancelled)
                    ++cancelled;
                return false;
            });
        save.on_modal_close.connect(
            [&](native::dialog_result result) {
                if (result == native::dialog_result::cancelled)
                    ++cancelled;
                return false;
            });

        owner.create();
        open.create();
        open.show();
        save.create();
        save.show();

        expect(cancelled == 2 &&
                   !open.get_created() &&
                   !save.get_created() &&
                   owner.get_input_enabled(),
               "unavailable file choosers cancel without throwing");

        owner.destroy();
        if (had_path)
            setenv("PATH", old_path.c_str(), 1);
        else
            unsetenv("PATH");
    }

    // Keep an explicitly off-screen SDL window's title area reachable.
    void test_window_placement() {
        native::app_wnd window(
            "Placement", native::rect(-500, -500, 160, 120));
        window.create();
        expect(window.get_position().x >= 0 &&
                   window.get_position().y >= 32,
               "SDL constrains top-level placement to its work area");
        window.destroy();
    }

    // Exercise the portable two-pane splitter with live native controls.
    void test_split_view_lifecycle() {
        native::app_wnd owner(
            "Split View", native::rect(30, 30, 720, 480));
        native::list project({"include", "lib", "tests"});
        native::text_edit editor(
            "Split editor", native::text_edit_mode::multi_line);
        native::split_view split(
            project,
            editor,
            native::split_orientation::horizontal,
            native::rect(20, 20, 680, 420));
        split.set_ratio(0.3f).set_minimums(120, 180);
        split.set_parent(&owner);

        owner.create();
        owner.show();
        split.create();
        split.show();
        expect(project.get_created() && editor.get_created(),
               "split view creates and shows both native panes");
        split.set_ratio(0.45f);
        expect(project.get_dimensions().w > 120 &&
                   editor.get_dimensions().w > 180,
               "split view resizes both panes");
        split.destroy();
        expect(!project.get_created() && !editor.get_created(),
               "split view destroys both native pane resources");
        owner.destroy();
    }
} // namespace

int main() {
    try {
        test_modal_stack();
        test_clipboard_and_text_edit();
        test_unavailable_file_dialogs();
        test_window_placement();
        test_split_view_lifecycle();
    } catch (const std::exception &error) {
        std::cerr << "FAILED: unexpected exception: " << error.what()
                  << '\n';
        ++failure_count;
    }
    return failure_count == 0 ? 0 : 1;
}

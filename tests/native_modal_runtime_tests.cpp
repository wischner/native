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
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <SDL2/SDL.h>

#include <native.h>

namespace linux::sdl2
{
    bool handle_text_edit_mouse(
        native::wnd *parent, int x, int y, bool pressed);
    bool handle_text_edit_key(
        native::wnd *parent, const SDL_KeyboardEvent &event);
    bool handle_button_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released);
    bool handle_collection_mouse(native::wnd *owner,
                                 int x,
                                 int y,
                                 bool pressed,
                                 bool released,
                                 int clicks);
    bool handle_split_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released);
    bool handle_split_motion(native::wnd *owner, int x, int y);
    bool handle_collection_motion(native::wnd *owner, int x, int y);
    bool handle_combo_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released);
    bool handle_combo_motion(native::wnd *owner, int x, int y);
    void restore_window_focus(native::app_wnd *window);
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

    // Keep SDL's emulated desktop controls on a neutral system-gray palette.
    void test_neutral_control_palette() {
        native::app_wnd owner(
            "Palette", native::rect(10, 10, 160, 100));
        owner.create();
        auto appearance = native::theme::create(owner.get_gpx());
        const native::theme::palette colors =
            appearance->native_palette();
        const auto neutral = [](native::rgba color) {
            return color.r == color.g && color.g == color.b &&
                   color.a == 255;
        };
        expect(neutral(colors.button_bg) &&
                   neutral(colors.button_hot_bg) &&
                   neutral(colors.button_pressed_bg) &&
                   neutral(colors.menu_bar_bg) &&
                   neutral(colors.menu_hot_bg) &&
                   neutral(colors.selection_bg) &&
                   neutral(colors.focus),
               "SDL controls and selections use a neutral gray palette");
        expect(!appearance->get_tree_lines_visible(),
               "SDL trees omit connector lines by default");
        expect(appearance->get_disclosure_size() == 9,
               "SDL uses compact disclosure indicators");
        owner.destroy();
    }

    // Verify SDL popup selection and painted collection-thumb dragging.
    void test_responsive_painted_controls() {
        native::app_wnd owner(
            "Painted controls", native::rect(10, 10, 320, 220));
        native::combo_box combo(
            {"First", "Second", "Third"},
            native::combo_box_style::drop_down_list,
            20,
            20,
            160,
            24);
        native::combo_box covered_combo(
            {"Covered first", "Covered second"},
            native::combo_box_style::drop_down_list,
            20,
            60,
            160,
            24);
        native::combo_box editable_combo(
            {"Editable first", "Editable second"},
            native::combo_box_style::editable,
            190,
            20,
            110,
            24);
        std::vector<native::icon_view_item> items;
        for (std::uint64_t id = 1; id <= 20; ++id)
            items.push_back({"Item " + std::to_string(id), nullptr, id, true});
        native::icon_view icons(items, 20, 70, 120, 90);
        combo.set_selected_index(0);
        combo.set_parent(&owner);
        covered_combo.set_parent(&owner);
        editable_combo.set_parent(&owner);
        icons.set_parent(&owner);
        owner.create();
        owner.show();
        combo.create();
        combo.show();
        covered_combo.create();
        covered_combo.show();
        editable_combo.create();
        editable_combo.show();
        icons.create();
        icons.show();

        expect(linux::sdl2::handle_combo_mouse(
                   &owner, 30, 30, true, false) &&
                   linux::sdl2::handle_combo_mouse(
                       &owner, 30, 30, false, true),
               "SDL combo opens from one click");
        expect(linux::sdl2::handle_combo_mouse(
                   &owner, 30, 70, true, false) &&
                   linux::sdl2::handle_combo_mouse(
                       &owner, 30, 70, false, true) &&
                   combo.get_selected_index() == 1 &&
                   covered_combo.get_selected_index() == -1,
               "SDL combo popup commits an overlapping item on its first "
               "press");
        expect(linux::sdl2::handle_combo_mouse(
                   &owner, 200, 30, true, false) &&
                   linux::sdl2::handle_combo_mouse(
                       &owner, 200, 30, false, true) &&
                   linux::sdl2::handle_combo_motion(
                       &owner, 200, 70),
               "SDL editable combo opens from its text and tracks popup "
               "hover");

        expect(linux::sdl2::handle_collection_mouse(
                   &owner, 132, 90, true, false, 1) &&
                   linux::sdl2::handle_collection_motion(
                       &owner, 132, 140) &&
                   linux::sdl2::handle_collection_mouse(
                       &owner, 132, 140, false, true, 1) &&
                   icons.get_scroll_offset() > 0,
               "SDL collection scrollbar thumb captures and drags");

        icons.destroy();
        editable_combo.destroy();
        covered_combo.destroy();
        combo.destroy();
        owner.destroy();
    }

    // A control callback may grow the same registry being dispatched.
    void test_callback_registry_mutation() {
        native::app_wnd owner(
            "Registry mutation", native::rect(10, 10, 240, 120));
        native::button opener("Open", 10, 10, 80, 28);
        std::unique_ptr<native::button> created;
        opener.on_click.connect([&] {
            created = std::make_unique<native::button>(
                "Created", 100, 10, 100, 28);
            created->set_parent(&owner);
            created->create();
            created->show();
            return true;
        });
        owner.create();
        owner.show();
        opener.set_parent(&owner);
        opener.create();
        opener.show();

        expect(linux::sdl2::handle_button_mouse(
                   &owner, 20, 20, true, false) &&
                   linux::sdl2::handle_button_mouse(
                       &owner, 20, 20, false, true) &&
                   created && created->get_created(),
               "button dispatch survives callback registry growth");

        created.reset();
        opener.destroy();
        owner.destroy();
    }

    // Route a real SDL emulated-table click without a child gpx binding.
    void test_table_pointer_selection() {
        native::app_wnd owner(
            "Table click", native::rect(10, 10, 300, 180));
        native::table_store store({
            {1, {{1, {"First", nullptr}}}},
            {2, {{1, {"Second", nullptr}}}},
            {3, {{1, {"Third", nullptr}}}},
            {4, {{1, {"Fourth", nullptr}}}},
            {5, {{1, {"Fifth", nullptr}}}},
            {6, {{1, {"Sixth", nullptr}}}},
            {7, {{1, {"Seventh", nullptr}}}},
            {8, {{1, {"Eighth", nullptr}}}}
        });
        native::table_column column;
        column.id = 1;
        column.title = "Name";
        column.width = 180;
        native::table_view table(10, 10, 220, 100);
        table.set_columns({column}).set_model(&store);
        table.set_parent(&owner);
        owner.create();
        owner.show();
        table.create();
        table.show();

        expect(linux::sdl2::handle_collection_mouse(
                   &owner, 20, 45, true, false, 1) &&
                   linux::sdl2::handle_collection_mouse(
                       &owner, 20, 45, false, true, 1) &&
                   table.get_selected_rows() ==
                       std::vector<native::table_row_id>{1},
               "SDL table pointer hit-testing selects its first row");
        expect(table.get_visible_row_range().count == 4,
               "SDL table rendering fills its body with complete rows");
        expect(linux::sdl2::handle_collection_mouse(
                   &owner, 222, 102, true, false, 1) &&
                   linux::sdl2::handle_collection_mouse(
                       &owner, 222, 102, false, true, 1) &&
                   table.get_vertical_scroll_row() == 1,
               "SDL table scrollbar increment arrow scrolls one row");

        table.destroy();
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

    // Keep a synchronous message dialog live, dismiss it through SDL,
    // and prove its owner accepts the very next control click.
    void test_message_box_focus_restoration() {
        native::app_wnd owner(
            "Message owner", native::rect(10, 10, 320, 200));
        native::button action("Action", 20, 20, 90, 28);
        int clicks = 0;
        action.on_click.connect([&] {
            ++clicks;
            return true;
        });
        owner.create();
        owner.show();
        const char *click_through = SDL_GetHint(
            SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH);
        expect(click_through && std::string(click_through) == "1",
               "SDL delivers the activation click to controls");
        action.set_parent(&owner);
        action.create();
        action.show();

        std::thread dismiss([] {
            for (int attempt = 0; attempt < 2000; ++attempt) {
                SDL_Window *window = nullptr;
                for (Uint32 id = 1; id < 256 && !window; ++id) {
                    SDL_Window *candidate = SDL_GetWindowFromID(id);
                    const char *candidate_title = candidate
                                                      ? SDL_GetWindowTitle(
                                                            candidate)
                                                      : nullptr;
                    if (candidate_title &&
                        std::string(candidate_title) == "Message focus") {
                        window = candidate;
                    }
                }
                const char *title = window ? SDL_GetWindowTitle(window)
                                           : nullptr;
                if (title && std::string(title) == "Message focus") {
                    SDL_Event event{};
                    event.type = SDL_KEYDOWN;
                    event.key.windowID = SDL_GetWindowID(window);
                    event.key.keysym.sym = SDLK_RETURN;
                    SDL_PushEvent(&event);
                    return;
                }
                SDL_Delay(1);
            }
            SDL_Event event{};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        });
        const native::message_box_result result =
            native::message_box::show(
                owner,
                "The message uses the application control font.",
                "Message focus",
                native::message_box_buttons::yes_no_cancel,
                native::message_box_icon::question);
        dismiss.join();

        expect(result == native::message_box_result::yes &&
                   owner.get_input_enabled(),
               "SDL message boxes return a result and restore their owner");
        expect(linux::sdl2::handle_button_mouse(
                   &owner, 30, 30, true, false) &&
                   linux::sdl2::handle_button_mouse(
                       &owner, 30, 30, false, true) &&
                   clicks == 1,
               "the first owner click works after an SDL message box");

        action.destroy();
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
                   !open.get_visible() &&
                   !save.get_visible() &&
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
        const int splitter_x = split.get_position().x +
                               split.get_splitter_bounds().p.x + 1;
        expect(linux::sdl2::handle_split_mouse(
                   &owner, splitter_x, 80, true, false) &&
                   linux::sdl2::handle_split_motion(
                       &owner, 480, 80) &&
                   linux::sdl2::handle_split_mouse(
                       &owner, 480, 80, false, true) &&
                   split.get_ratio() > 0.6f &&
                   split.get_cursor() ==
                       native::mouse_cursor::resize_horizontal,
               "SDL split divider captures and applies pointer dragging");
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
        test_neutral_control_palette();
        test_callback_registry_mutation();
        test_responsive_painted_controls();
        test_table_pointer_selection();
        test_modal_stack();
        test_clipboard_and_text_edit();
        test_unavailable_file_dialogs();
        test_message_box_focus_restoration();
        test_window_placement();
        test_split_view_lifecycle();
    } catch (const std::exception &error) {
        std::cerr << "FAILED: unexpected exception: " << error.what()
                  << '\n';
        ++failure_count;
    }
    return failure_count == 0 ? 0 : 1;
}

//
// Implements public-API-only manual tests for keyboard, PCM and cancellable parallel work.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "infinity_window.h"
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <mutex>

namespace vision
{
    infinity_window::infinity_window()
        : native::app_wnd("Native Infinity acceptance test", 60, 60, 820, 540)
        , _editor("Type here: emulator keys must stay unchanged.",
            native::text_edit_mode::single_line, 20, 330, 760, 30)
        , _tone("Stereo tone", 20, 390, 140, 32)
        , _work("Worker", 170, 390, 120, 32)
        , _cancel("Cancel", 300, 390, 120, 32)
        , _child("Child process", 430, 390, 160, 32)
        , _modal_button("Modal reset", 600, 390, 160, 32)
        , _modal(*this, "Hold a key, then open this dialog", 120, 120, 380, 120)
        , _modal_close("Close", 20, 40, 100, 32) {
        on_wnd_create.connect([this] { return create_children(); });
        on_wnd_paint.connect([this](auto event) { return paint(event); });
        on_key.connect([this](native::key_event event) {
            _last = std::string(native::key_name(event.key));
            _last += event.action == native::key_action::press ? " press" : " release";
            if (event.action == native::key_action::press) {
                _held.insert(event.key); ++_presses;
                if (event.repeat) { ++_repeats; _last += " repeat"; }
            } else { _held.erase(event.key); ++_releases; }
            _last += " modifiers=" + std::to_string(unsigned(event.modifiers));
            invalidate(); return true;
        });
        on_key_reset.connect([this] {
            _held.clear(); ++_resets; _last = "Held input cancelled.";
            invalidate(); return true;
        });
        _tone.on_click.connect([this] { start_tone(); return true; });
        _work.on_click.connect([this] { start_worker(); return true; });
        _cancel.on_click.connect([this] {
            _worker.request_stop(); _sound.request_stop();
            _status = "Cancellation requested."; invalidate(); return true;
        });
        _child.on_click.connect([this] { start_child(); return true; });
        _modal_button.on_click.connect([this] {
            if (!_modal.get_created()) _modal.create();
            _modal.show(); return true;
        });
        _modal.on_wnd_create.connect([this] {
            _modal_close.set_parent(&_modal).create(); _modal_close.show(); return true;
        });
        _modal_close.on_click.connect([this] {
            _modal.close(native::dialog_result::accepted); return true;
        });
    }
    infinity_window::~infinity_window() { stop(); }
    void infinity_window::stop() {
        if (_delivery) _delivery->close();
        _worker.request_stop(); _sound.request_stop();
        if (_worker.joinable()) _worker.join();
        if (_sound.joinable()) _sound.join();
        _audio.close();
    }
    void infinity_window::on_native_destroy() {
        stop();
        native::app_wnd::on_native_destroy();
    }
    bool infinity_window::create_children() {
        for (native::wnd *child : std::array<native::wnd *, 6>{
            &_editor, &_tone, &_work, &_cancel, &_child, &_modal_button}) {
            child->set_parent(this).create(); child->show();
        }
        _delivery = std::make_unique<native::ui_dispatch_scope>(*this);
        if (!native::app::get_physical_keyboard_supported())
            _status = "Physical input unavailable: GEM SDK needs a raw release integration.";
        return true;
    }
    bool infinity_window::paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text("Physical input: letters, digits, arrows, punctuation and both modifiers.", {20, 20});
        event.g.draw_text("Hold several keys. Switch windows, click a control, or open a modal: held must clear.", {20, 48});
        event.g.draw_text("Last: " + _last, {20, 92});
        std::string held = "Held: ";
        for (auto key : _held) held += std::string(native::key_name(key)) + " ";
        event.g.draw_text(held, {20, 122});
        event.g.draw_text("Press=" + std::to_string(_presses) + " Release=" +
            std::to_string(_releases) + " Repeat=" + std::to_string(_repeats) +
            " Reset=" + std::to_string(_resets), {20, 152});
        event.g.draw_text("Sound: 440 Hz left, 660 Hz right. Missing output is reported explicitly.", {20, 208});
        event.g.draw_text("Worker and child results must leave painting and input responsive.", {20, 238});
        event.g.draw_text("Status: " + _status, {20, 280});
        event.g.draw_text("Close during work to test cancellation and receiver lifetime.", {20, 450});
        return true;
    }
    void infinity_window::start_worker() {
        _worker.request_stop();
        if (_worker.joinable()) _worker.join();
        auto sender = _delivery->sender();
        _worker = std::jthread([sender](std::stop_token stop) {
            std::mutex mutex;
            std::condition_variable_any changed;
            std::unique_lock lock(mutex);
            for (int i = 0; i <= 100 && !stop.stop_requested(); ++i) {
                sender.post_latest([i](native::wnd &target) {
                    auto &self = static_cast<infinity_window &>(target);
                    self._status = "Worker progress " + std::to_string(i) + "%";
                    self.invalidate();
                });
                changed.wait_for(lock, stop, std::chrono::milliseconds(20), [] { return false; });
            }
            sender.post([cancelled = stop.stop_requested()](native::wnd &target) {
                auto &self = static_cast<infinity_window &>(target);
                self._status = cancelled ? "Worker cancelled." : "Worker completed.";
                self.invalidate();
            });
        });
    }
    void infinity_window::start_tone() {
        _sound.request_stop();
        if (_sound.joinable()) _sound.join();
        if (!_audio.open()) { _status = "No audio device / unsupported format."; invalidate(); return; }
        auto sender = _delivery->sender();
        _sound = std::jthread([this, sender](std::stop_token stop) {
            std::array<std::int16_t, 882> samples{};
            unsigned accepted = 0;
            for (unsigned block = 0; block < 100 && !stop.stop_requested(); ++block) {
                for (unsigned frame = 0; frame < 441; ++frame) {
                    const double time = (block * 441 + frame) / 44100.0;
                    samples[frame * 2] = static_cast<std::int16_t>(6000 * std::sin(time * 440 * 6.283185307));
                    samples[frame * 2 + 1] = static_cast<std::int16_t>(6000 * std::sin(time * 660 * 6.283185307));
                }
                if (_audio.queue(samples)) ++accepted;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            sender.post([accepted](native::wnd &target) {
                auto &self = static_cast<infinity_window &>(target);
                self._status = "Tone admitted " + std::to_string(accepted) + "/100 blocks.";
                self.invalidate();
            });
        });
    }
    void infinity_window::start_child() {
        _worker.request_stop();
        if (_worker.joinable()) _worker.join();
        auto sender = _delivery->sender();
        native::process_config config;
        config.executable = std::filesystem::absolute(native::app::argv[0]);
        config.arguments = {"--native-process-child", "argument with spaces", "quote\"slash\\"};
        _worker = std::jthread([sender, config](std::stop_token stop) {
            try {
                native::process child(config);
                std::stop_callback cancel(stop, [&child] { child.request_stop(); });
                const bool started = child.start();
                if (stop.stop_requested()) child.request_stop();
                child.wait();
                const auto result = child.get_result();
                sender.post([started, result](native::wnd &target) {
                    auto &self = static_cast<infinity_window &>(target);
                    self._status = result.reason == native::process_exit::cancelled ?
                        "Child cancelled." : started ? "Child exit=" + std::to_string(result.exit_code) +
                        " stdout=" + result.output + " stderr=" + result.error : result.launch_error;
                    self.invalidate();
                });
            } catch (const std::exception &error) {
                const std::string text = error.what();
                sender.post([text](native::wnd &target) {
                    auto &self = static_cast<infinity_window &>(target);
                    self._status = text; self.invalidate();
                });
            }
        });
    }
}

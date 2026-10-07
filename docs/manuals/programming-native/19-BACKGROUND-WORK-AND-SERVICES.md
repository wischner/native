# Background Work and Services

Use an owning controller and standard C++20 cancellation. Keep UI operations
on the UI thread and pass owned input snapshots into workers. Do not detach
workers or assume that requesting stop interrupts a blocking OS call.

## Results on the UI thread

Bind after the receiver is created:

```cpp
native::ui_dispatch_scope delivery(window, 64);
auto sender = delivery.sender();
std::jthread worker([sender](std::stop_token stop) {
    if (stop.stop_requested()) return;
    const std::string message = "Completed on a worker";
    const auto admitted = sender.post([message](native::wnd &target) {
        static_cast<native::app_wnd &>(target).set_title(message);
    });
    // accepted means queued, full requires caller policy, closed means gone.
    (void)admitted;
});
// UI thread, before receiver destruction:
delivery.close();
worker.request_stop();
// Join independently of UI delivery; avoid an unbounded wait on the UI.
```

Include `<thread>` and keep the scope and worker in the owning controller.
Handle worker exceptions as explicit result values. `post_latest()` replaces
obsolete progress/display snapshots. Use `post()` and handle full/closed for
completion; never use lossy progress delivery for protocol bytes. Window
resource destruction automatically invalidates its generation. A new window
creation needs a new scope; old senders cannot reach the replacement.
Move-only results can be stored in `shared_ptr<const result>` for the copyable
callable boundary. Stop-aware `condition_variable_any` waits suit tickers.

## Live PCM sound

```cpp
native::audio_out sound(44100, 1, 22050);
if (sound.open()) {
    std::array<std::int16_t, 441> samples{};
    // Fill samples on one producer. queue() copies a complete block.
    if (!sound.queue(samples)) {
        // Application policy: drop a block when ahead, or remain muted.
    }
}
// Stop/detach and join the producer before closing:
sound.close();
```

The capacity is frames, not bytes. Stereo uses alternating left/right samples.
`get_queued_frames()` includes submitted device data conservatively. No device
means `open()` returns false; the application can run muted. PCM underflow is
silence. Configuration errors and partial stereo frames throw. Serialize
open/close against producer access. Spectrum sound generation and emulation
pacing stay in the application, not in Native.

## An external helper

Configure `process_config` with an executable path, separate UTF-8 arguments,
optional working directory/environment overrides and stream policies. The
constructor caches configuration; `start()` explicitly launches without a
shell. Read `get_result()` after `wait()` for exit reason, captured byte strings
and truncation flags. `request_stop()` is thread-safe and nonblocking.

Start, monitor and finally join helpers on a worker/controller so the UI stays
responsive. A controller worker can register a `std::stop_callback` which
calls `child.request_stop()`, then `child.wait()`, and post its owned result
through the scope. Child completion must not require the UI callback to run.
The bounded capture API drains output after its limit and flags truncation;
it is unsuitable for a lossless protocol. Inherit/discard streams when capture
is unnecessary. Input is disconnected and no descendant-process management
is implied. POSIX stop attempts SIGTERM before timeout escalation; Windows
requires an application-specific cooperative protocol and then terminates the
owned child after the timeout. The destructor also cancels and joins.

## Manual acceptance tomorrow

Build the usual backend target and run its Vision binary with
`--infinity-test`. On macOS pass that argument to the executable inside the
Vision bundle. The same public-API UI runs on every selected backend.

1. Click the background. Hold letters, arrows, both Shift/Control keys and
   punctuation. Observe separate positions, paired releases and repeat counts.
2. Switch applications while holding keys. Click a child control or open the
   modal test. Held keys must clear; typing into the editor must not press
   emulator client keys. Recheck using another keyboard layout.
3. Play the stereo tone: 440 Hz left and 660 Hz right for one second. Verify
   actual sound, channel routing, repeated playback and unavailable-device
   reporting. A dummy device validates ownership but cannot validate sound.
4. Start/cancel progress and the child helper. Painting and keyboard input
   should remain responsive. The helper waits two seconds, reports exit 7 and separate streams,
   and can be cancelled while painting remains responsive.
5. Close during each operation and reopen the test executable; check shutdown
   and resource lifetime. GEMix reports physical keys unavailable pending its
   SDK/viewer integration; its other diagnostics remain usable.

On cross-built Windows and Haiku targets, copy the three service test
executables from the backend's `tests/` directory alongside their runtime
libraries and run each on the target system. Cross-build CTest files contain
host absolute paths; use CTest on a target only with a correctly configured
target-side tree. Windows tests need the same three MinGW runtime DLLs as
Vision. Launch `vision.exe --infinity-test` in the logged-in desktop.

CTest also builds `native_infinity_tests`, `native_audio_queue_tests` and
`native_process_tests` on every target, plus real SDL dispatch coverage in
`native_keyboard_runtime_tests`. Consult the feature matrix for verified
build/runtime status rather than treating a successful compile as a test pass.

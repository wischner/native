# Infinity: Portable Emulator Input, Audio, and Background Work

Reviewed and implemented: 2026-10-06.

This exceptional consumer integration note records the extensions prompted by
`../infinity/lib/nui`, their implementation, and remaining platform limits.
The original review proposed physical keyboard events, bounded PCM output,
scoped worker delivery and an optional child-process service. Those APIs now
exist. **Complete held-key parity is still blocked on the GEMix SDK/viewer;
macOS adapters have not been compiled in this pass.**

The [architecture standard](../standards/ARCHITECTURE.md), including its new
Section 20, and [C++ style standard](../standards/CPP-CODING-STYLE.md) govern
the code. Current API explanations belong in the
[background-services book chapter](../manuals/book-of-native/PATTERNS-BACKGROUND-SERVICES.md)
and [application chapter](../manuals/programming-native/19-BACKGROUND-WORK-AND-SERVICES.md).
This note does not promote compiled adapters to verified runtime support.

## Findings from the original code review

The review inspected Infinity's keyboard table, window/application lifecycle,
audio observer, host/session threads, Make linkage, startup and existing tests,
and Native's standards, manuals, public headers and backend dispatchers.

The original keyboard table contained 51 entries, not the supplied description's
36. Left Shift supplies Spectrum CAPS SHIFT; right Shift and either Control
supply SYMBOL SHIFT. Those emulator rules remain in Infinity. Its SDL event
watch ran before ordinary Native routing, admitted presses using a window
ID/title/focus helper, and did not clear held keys on focus loss.

Audio supplied native-endian signed 16-bit mono PCM at 44,100 frames/second
in 441-sample blocks. The queued-byte threshold represented approximately
half a second and could be exceeded by one block. Its observer ran on the
emulation thread under the machine lock. The ticker already used
`app::post()`, but queued closures captured a window pointer without a lifetime
guard. Paint damage bounds were also being used as display layout bounds.

## Implemented Native facilities

### Physical keyboard input

`key_code` identifies physical positions using US reference names, independently
of text/layout. Sided modifiers, keypad Enter, punctuation and navigation keys
remain distinct. OS scan codes never appear in the public API. `key_event`
contains a position, press/release action, repeat flag and typed modifiers.
`key_name()` supplies diagnostic text.

`wnd::on_key`, `on_key_reset`, `on_native_key()` and
`on_native_key_reset()` follow synchronous signal semantics. Newest handlers
run first; `true` consumes the event. The shared peer tracks admitted keys,
rejects unpaired repeats/releases, and clears state before cancellation signals.
Focused controls retain ordinary editing/selection handling and accelerators
retain command handling. Once a physical press is admitted, its repeats and
release stay paired with it. No synthesized text is derived from physical input.

Focus loss, relevant focus transfer, menus, modal entry and destruction cancel
held input. Cancellation replaces missing releases; it is not a fake hardware
release stream. Derived focus hooks call the base. Resource recreation establishes
a new lifetime generation.

Adapters use SDL scancodes, XKB physical key names for the X11/Xt/XView/WINGs
families, Win32 scan/extended bits, AppKit key codes and Haiku raw positions.
Mappings and native callbacks remain private to their platform/toolkit layers.
AppKit sided modifier state uses the event flags defined by
[Apple's HID event header](https://github.com/apple-oss-distributions/IOHIDFamily/blob/main/IOHIDSystem/IOKit/hidsystem/IOLLEvent.h),
so a release after focus cancellation cannot become a new press.
Haiku uses the per-message modifier bitmap and repeat marker produced by
[its keyboard input server](https://github.com/haiku/haiku/blob/master/src/add-ons/input_server/devices/keyboard/KeyboardInputDevice.cpp),
preserving event order instead of reading only the latest global key state.

**GEMix limitation:** the pinned GEM SDK transforms raw HID positions into
layout-oriented key values and AES discards releases. Both direct and gemd
transport are affected. The viewer's raw UDP stream has one subscriber and no
complete focus contract; registering another subscriber would interfere with
GEM input. A supported SDK/viewer raw-input hook carrying physical positions,
press/release, focus and cancellation is required. Native does not guess releases
with timers or steal the viewer connection. `app::get_physical_keyboard_supported()`
reports false for GEMix; Vision and nemu-gui report that limitation explicitly.

### Bounded PCM output

`audio_out(rate, channels, capacity_frames)` caches configuration. `open()`
explicitly opens output; destruction/`close()` release it. Input is signed
16-bit native-endian, mono or interleaved stereo. `queue(span)` copies a complete
block or rejects it; capacity includes submitted device frames conservatively.
`get_queued_frames()` is a snapshot, not an admission guarantee. Underflow
produces silence. Invalid configuration or incomplete stereo frames throws;
unavailable devices/formats fail explicitly. Stop/join the producer before
closing output. No real-time callback invokes application code.

The shared bounded ring is independent of adapters: SDL owns an audio subsystem
reference and callback device; other Linux toolkits use private ALSA loading;
Windows uses WASAPI; macOS uses AudioQueue/CoreAudio; Haiku uses BSoundPlayer.
Video-loop shutdown preserves separately owned SDL audio. All audio handles,
callback ABI details and hardware occupancy accounting remain private.

### Scoped C++20 worker delivery

`ui_dispatch_scope(created_window, capacity)` owns a bounded FIFO and one latest
snapshot slot. A copyable `ui_sender` holds only a weak endpoint. `post()` reports
accepted/full/closed; `post_latest()` replaces stale snapshots. Admission is not
a promise that a callback runs after the event loop ends. One pending wake uses
existing `app::post()`; each turn drains a bounded batch. Callbacks receive a
borrowed receiver on the UI thread after lifetime validation.

Close on the UI thread before teardown. Old queued callbacks and senders cannot
reach a destroyed/recreated receiver. Captured payload destructors and callbacks
run outside the queue mutex. A callback which triggers reentrant destruction
must stop using its receiver. Worker completion is synchronized independently of
UI delivery. Use owned input/results, `std::jthread`, `std::stop_token`, and
stop-aware waits; no detached workers or implicit future lifetime is introduced.

### An owned child process

`process_config` uses filesystem paths, distinct UTF-8 arguments, an optional
working directory, explicit inherited/supplied environment and separate stream
policies. `process` caches configuration and explicitly `start()`s once without
a shell. An opaque platform peer owns the child, descriptors/handles and monitor.
Both output streams drain fairly; captured bytes are bounded per stream and
truncation is flagged while draining continues. This is not a lossless protocol
transport. Input is disconnected; descendant-tree ownership is outside the API.

Exit, signal, cancellation and launch failure have distinct results. A worker
controller calls `wait()` and delivers its owned result through a scope.
`request_stop()` is thread-safe/nonblocking. POSIX attempts SIGTERM, then SIGKILL
after the configured timeout. Windows has no universal cooperative termination;
applications needing it supply a protocol, followed by timed termination of the
owned child. RAII cancels, joins and reaps/releases resources. POSIX integration
lives in the platform layer and Windows integration uses private Win32 APIs.

## Infinity migration

`lib/nui` no longer includes SDL headers or calls SDL. The keyboard table uses
`native::key_code`; the window listens to Native key/reset signals and clears the
Spectrum held set under machine synchronization. It consumes only Spectrum
positions, preserving native handling of unmapped keys such as Alt. The SDL event watch and
window-ID/title focus helper are removed.

The beeper queues fixed 441-frame blocks into `native::audio_out(44100, 1, 22050)`.
The emulator explicitly drops rejected blocks or runs muted. The ticker is a
stop-aware `std::jthread` with a weak scoped latest sender; it starts only after
window creation. Display snapshots are converted under the machine lock, then
painted outside it. Full client bounds determine layout; damage only clips paint.

Normal and exceptional cleanup close UI delivery, stop/join the ticker, detach
the audio observer under the machine lock, shut down/join emulation, then close
audio. Menus, file dialogs, images, nearest scaling and titles still use Native.

Infinity now has a CMake GUI dependency closure and a `nemu-gui` target using
Native's `program()` launcher. Native can be consumed through `add_subdirectory()`
without also building Vision, tests or Docker orchestration. The Make-built
console/MCP/DAP `nemu` retains its separate entry point and does not link a GUI.
There is no manually maintained SDL/X11/image/font link list in the GUI target.
The existing host serial bridge has private Windows socket adaptation and BSD
SIGPIPE handling so the GUI dependency closure also cross-compiles. Other Infinity
tools remain outside this GUI port.

See [Infinity's README](../../../infinity/README.md) for its consumer build and
GUI arguments; normal Native build instructions remain in the manuals.

## Verification and tomorrow's acceptance

The same public-API diagnostic UI is available as **`vision --infinity-test`**.
It displays physical presses/releases/repeats/reset counts and held positions,
provides an ordinary text editor and modal dialog, plays a one-second stereo tone
(440 Hz left, 660 Hz right), and runs cancellable progress and a captured child
helper. Close during each operation to exercise teardown. No separate example
program or backend-specific application code was added.

Local, untracked acceptance folders were prepared for this review under
`build/infinity-acceptance/windows/` and `build/infinity-acceptance/haiku/`,
containing Vision, the three service tests, nemu-gui and the Spectrum mapping
test; the Windows folder also has
the required MinGW DLLs. They are ready to copy to the target systems; no
remote deployment was performed. Linux binaries remain in their ordinary
backend build trees.

The [manual acceptance checklist](../manuals/programming-native/19-BACKGROUND-WORK-AND-SERVICES.md#manual-acceptance-tomorrow)
covers layout changes, sided modifiers, focus/menus/modality, real sound,
unavailable devices, cancellation and shutdown. Run the binary from each normal
backend build tree; on macOS pass the argument to the executable inside the
Vision bundle. Cross-built Windows/Haiku service tests run directly on their target OS;
the generated CTest files contain host absolute paths. Use CTest with a
proper target-side configuration for native builds.

| Test | Scope |
| --- | --- |
| `native_infinity_tests` | Physical pairing/consumption/reset, bounded/coalesced delivery, lifetime invalidation |
| `native_audio_queue_tests` | Deterministic private consumer: copying, stereo arithmetic, order, capacity, submitted occupancy, silence, failure, producer/consumer concurrency, reopen |
| `native_process_tests` | Real child arguments/environment/directory, both full pipes, truncation, exit, failed launch, cancellation/reaping |
| `native_keyboard_runtime_tests` (SDL2) | Actual SDL event dispatch, editor isolation, focus reset, audio surviving video teardown |
| `infinity_native_keyboard_tests` (Infinity) | Native-key Spectrum mapping and sided shift/chord rules |

All nine Native Docker selections compile, including Windows and Haiku test
binaries and both GEM transports. SDL2's complete 15-test suite and both GEM
10-test suites pass. X11 and Window Maker also pass; Motif and OPEN LOOK pass their behavior
suites with external toolkit leak detection disabled. Their leak-enabled
runs report Xt/XView allocations at shutdown. Detailed results and adapter coverage
are recorded in the [feature matrix](../manuals/book-of-native/FEATURE-MATRIX.md).
Infinity's GUI and mapping test cross-compile for Windows and Haiku and build
for all six Linux toolkits. Mapping tests pass on all seven Linux selections, including both GEM
transports, and the SDL dummy-driver
screenshot smoke check pass. Its existing emulator/host tests and console
MCP/DAP end-to-end checks also pass.

A fake PCM consumer or dummy driver does not prove audible output. New physical
input adapters outside SDL have not been exercised with real held-key hardware
in this pass. Windows/Haiku device runtime acceptance remains for target systems.
The remote macOS build was rejected by automatic approval because its script
exports the repository to `leia`, a destination not explicitly authorized in this
session. Its new adapters remain uncompiled until that sync is approved. The
GEMix SDK prerequisite remains required before claiming full cross-platform
emulator keyboard parity.

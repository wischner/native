# Background Delivery, Audio, and Child Ownership

This chapter expands Architecture Section 20. All public declarations are
pure C++20; internal service peers own resource state independently of windows.

## Scoped UI delivery

`ui_dispatch_scope(created_window, capacity)` owns a shared core state with
one borrowed receiver, a weak window-generation token, a bounded FIFO and a
latest-progress slot. A copyable `ui_sender` holds only a weak reference.
Posting threads never read window state. The mutex protects admission and
queue ownership; callbacks and discarded payload destructors run outside it.

One pending `app::post()` wake drains at most sixteen callbacks per turn.
Latest progress is applied before FIFO completion. Remaining work rearms a
future turn, allowing native input/paint dispatch in between. Closing the
scope on the UI thread clears its receiver and queue. Native destruction
invalidates the weak generation even if the scope is still alive; recreation
cannot revive an old sender. Admission does not guarantee execution after
loop termination. The receiver must stay valid throughout a callback, or the
callback must stop accessing it after triggering destruction.

## PCM peer and device boundary

The shared peer validates formats and owns preallocated interleaved sample
storage and conservative frame accounting. Queueing never grows storage or
waits for playback space. Device adapters submit a bounded buffer and release
its real frames at a completed buffer boundary; silence does not occupy
application capacity. This deliberately overestimates partial-buffer latency.

SDL, AudioQueue and BSoundPlayer call a private consumer; ALSA and WASAPI use
owned cancellable workers. The ALSA adapter loads the stable runtime ABI and
requests exact-rate PCM without changing application speed. WASAPI owns COM
on its worker. All callbacks/threads stop before peer storage is freed.
No audio callback invokes application code. GUI teardown releases SDL video
references without shutting down an independently owned audio subsystem.

## Child peer

The core owns portable configuration, result synchronization, cancellation
and monitor lifetime in one shared peer. Platform code supplies launch and
monitor operations, keeping native handles inside the monitor's private
resource ownership. POSIX prepares argv and
environment before fork, uses only async-signal-safe operations before exec,
and reports exec failure through a close-on-exec pipe. Windows converts UTF-8
arguments, quotes for the C runtime parser, and limits inherited handles to
explicit standard streams. Neither calls a shell.

The monitor drains both pipes fairly into bounded per-stream byte captures;
excess bytes are drained and explicitly marked truncated. Output is not
implicitly decoded. Cancellation uses an independent stop source, so requests
can arrive during start without racing assignment of the monitor thread.
The destructor requests cancellation and joins: keep the controller outside
UI teardown when a child can take time. Only the direct child is owned;
descendants and interactive/lossless protocol transports are outside this API.

## Evidence

The deterministic core/PCM tests do not establish real device sound or desktop
focus. The process test launches a real helper. Adapter build and runtime
coverage are recorded separately in the feature matrix. GEMix lacks physical
input until its SDK and viewer supply a complete raw stream; the UI explicitly
reports that instead of generating false releases.

OpenMotif and Window Maker now wake blocked production loops through bounded,
nonblocking descriptor pipes. Workers write only the pipe; toolkit/Xlib calls
remain on the UI thread. Motif drains callbacks after Xt dispatch returns;
WINGs converts the descriptor notification into a local wake event, then drains
after ordinary/deferred dispatch. Pipe registration and the wake target end
before toolkit teardown. The CRT terminal smoke test exercises real child
output followed by a posted close without supplying input events or test timers.

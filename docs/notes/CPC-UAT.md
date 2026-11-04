# CPC integration acceptance — 2026-11-04

This review used `~/repos/retro-vault/charlie-papa-charlie` against the local
Native checkout based on `de30bc240bacc27ace4edb4ae03f507d8b1faaf4`, with the
working-tree fixes described below. The consumer already contained substantial
uncommitted platform/build/debugger work. It was preserved. These results do
not mean the consumer's pinned remote Native revision contains these fixes.

## Library repairs

- Fetch XKB physical key names explicitly. The old keyboard request did not
  populate the map, causing unknown physical keys on X11-family backends.
- Forward physical press/repeat/release from Athena, Motif, OPEN LOOK and WINGs
  collection/canvas hosts before their editor/navigation handlers. WINGs root
  clicks also establish toolkit logical focus.
- Forward AppKit canvas keys/modifiers and focus; route monitored repeats and
  releases to the focused child. Restore the key window's menu on activation,
  with owner fallback. Reserve AppKit's application submenu so File/Debug are
  not renamed or lost; provide Cmd+Q and function-key equivalents.
- Reuse Haiku physical translation in collection views and route modifier
  changes to the focused child.
- Accept Windows accelerator-origin `WM_COMMAND` (notification code 1) as
  well as menu clicks (0). Resolve accelerators from the message's top-level
  window, including modeless windows. Use Unicode menu APIs.
- Publish ICCCM text and EWMH UTF-8 titles on X11-family windows. Initialize
  character locale before Xt/WINGs title conversion. Automated ICCCM
  round-trips cover an em dash and accented text.
- Skip unchanged window bounds, titles and menu checks. A refresh loop no longer causes
  repeated native geometry operations or clears shader history for the same
  rectangle. A regression checks that cached shader output stays clean.
- Detach Motif menus before recursively destroying their Xt shell.
- Discover SDL2 Fontconfig through CMake as well as pkg-config, and ask for
  generic monospace before potentially missing named fixed fonts.

## Consumer and destination repairs

CPC now clears its effect and CRT request/checkmark after a rendering error,
preventing every new source frame from retrying a disabled effect. Its GTK
payload includes the otherwise missing `libgtksourceview-5.so.0`, a relocatable
loader path and the distribution copyright notice. This fixed the elementary
VM's loader failure without adding platform code to the application's UI.
Its screenshot mode waits for shader readiness instead of quitting during
initialization. UI tests no longer inject a host-downloaded Xvfb library path
into the SDK, and wait for a viewable window before assigning X focus.

Windows initially failed the native context probe with “OpenGL 3.3 is
unavailable on this Windows desktop.” Its display adapter is Microsoft Basic
Display Adapter. The application-local
[Mesa llvmpipe x64 26.2.6 build](https://github.com/mmozeiko/build-mesa/releases/tag/26.2.6)
passes the same context-preservation test and renders CPC's CRT on the actual
Windows desktop. The [build project](https://github.com/mmozeiko/build-mesa)
provides statically linked WGL DLLs. No Windows system driver or DLL was replaced.

The archive SHA-256 is
`1910245962e3d854589ed11a9a488e84841a01559142fba8b81395271981700b`.
The deployed `opengl32.dll` SHA-256 is
`e833d30337077e5890bf6a7147b7f2d27526b6f4647b32d8c8151fc57860c7f2`.
CPC's optional `CPC_WINDOWS_OPENGL_RUNTIME` staging accepts the DLL together
with a `notices/` directory. The tested directory contains Mesa source license
texts, LLVM 23.1.4's license and provenance. See the consumer's
`docs/manuals/BUILD-PLATFORMS.md` for the reproducible command. Software OpenGL
is not a promise of GPU performance.

## Evidence and scope

Native's Debug suites use the documented Docker SDKs and the remote Mac SDK.
Consumer builds use their existing SDK driver with the local Native checkout.
All eleven Docker selections build successfully; the macOS Debug bundle builds
with AppleClang on `mike`. CPC's six build/deployment Python tests pass.
Logs, screenshots and the disposable XTest/QMP harness are under
`build/cpc-uat/` in this checkout. The VM screenshots were inspected; saved
CPC `--screenshot` images alone show the source raster, not CRT composition.

| Selection | Evidence from this review |
| --- | --- |
| SDL2 | Native 28/28 CTests; CPC firmware/audio-queue/CRT smoke, full modeless debugger interaction, and repeated mouse capture/key-repeat/focus-loss tests pass |
| X11/Athena | Native 22/22; focused canvas press/repeat/release/reset and Unicode title checks pass; actual CPC F10 and clean close |
| OpenMotif | Native 26/26; focused input/title checks pass; actual CPC F10 and close pass with MWM in Xvfb |
| OPEN LOOK | Native 26/26; actual CPC F10 after canvas click and clean close |
| Window Maker | Native functional checks pass; new canvas/title/menu regression passes; classic-control process retains external Fontconfig/Pango LeakSanitizer exit allocations; actual CPC F10 and close pass |
| Generic GTK Wayland | Native 21/21 on final headless Weston run; CPC firmware startup, CRT active and clean close pass; an earlier splitter timing failure is recorded below |
| elementary | Native 21/21 under Weston; deployed CPC starts on Pantheon Wayland, Unicode title and F10 verified; CRT visibly rendered |
| macOS | Native 20/20 remote CTests; real CPC typing, modeless Debug menu, click back to main restoring File/View/Machine/Debug/Speed/Display/Mouse verified |
| Windows | Native menu/window API checks and Mesa context-preservation check pass; real CPC letters, F10 pause and CRT verified on Windows 11 |
| Haiku | Menu, dispatch, shader lifecycle, OSMesa context and GLSL renderer checks pass; real CPC F10 and CRT verified with `GALLIUM_DRIVER=softpipe` |
| GEMix direct and gemd | CPC firmware startup, `CRT status=2` (active), screenshot publication and zero-status shutdown pass in isolated Rasta sessions |

Representative verification entry points:

```sh
python3 scripts/docs/check.py
bash scripts/macos/remote/build.sh
# The existing GTK SDK command runs this inside its container:
bash scripts/linux/wayland/headless-session.sh \
  ctest --test-dir build/linux-wayland --output-on-failure
```

The focused `native_menu_runtime_tests` now exercises real canvas backend
callbacks on AppKit, Athena, Motif and WINGs, Unicode title round-trips on
Athena/Motif/WINGs, AppKit menu activation, and Windows accelerator-origin
commands. The other platform branches do not acquire canvas-key coverage
merely by compiling that executable. Real VM input was sent through the
virtual keyboard/mouse; Mac menu ownership was also inspected through
System Events. Inactive AppKit test processes exercise the actual window
activation delegate directly when the desktop does not grant key status.

## Remaining limits and observations

- The published GEM SDK does not supply the physical release contract. The
  optional local SDK extension described below enables it; it is not present
  merely because the backend builds or displays a firmware screen.
- The generic GTK suite initially failed its native splitter width assertion,
  then passed the focused rerun and the complete uninstrumented 21-test run.
  No splitter production change or suppression was made. This remains a
  timing-sensitive observation, not a claimed repair.
- One elementary paused capture remained blank until subsequent interaction;
  later paused CRT and raw frames rendered correctly, including eight subsequent
  pause/resume cycles with unchanged text pixels. This intermittent
  observation is not yet isolated to Native versus the emulator.
- WINGs' known Fontconfig/Pango exit allocations remain. No blanket sanitizer
  suppression was added to make the suite green.
- The CPC VM tests were predominantly muted. Audio-device playback, sustained
  game sessions, every chooser, clipboard path and every host keyboard layout
  are not qualified by this pass. Software-renderer throughput and maximum
  emulation speed have not been benchmarked against hardware rendering.
- The consumer retains its pre-existing build-copy compatibility adapters for
  older Native overrides. The Motif and fixed-font replacements do not activate
  against the repaired source. Its dependency catalog is advanced to the repair
  commit when publishing these changes.

Final payloads were refreshed in the existing Haiku/Windows deployment folders
and the separate `charlie-papa-charlie-uat` elementary folder. The Windows
Debug build remembers the prepared Mesa directory for F5. The temporary
`CPC-UAT` scheduled task was removed; the normal `CPC-Debug` task remains.
The Mac bundle was rebuilt in its existing remote project.

## CPC follow-up (2026-11-04)

The consumer pins upstream `f25ef150e862d61c32303f46e302c7bc55df92de`
with local repairs in the Native sibling checkout. Motif queues repaint
Expose events instead of dispatching from zero-delay timers: the actual CDE
Xephyr session now displays its menu and accepts F10 pause/resume.

GEM menu ownership now follows active modeless/main windows. Its optional
physical-key adapter requires the sibling GEM `gem/input.h` extension and
matching libgem/gemd. CPC can build a content-addressed SDK layer from that
checkout; published SDK images remain unchanged. The extension reuses the
standard event RPC layout, is explicitly opt-in, and leaves MU_KEYBD callers
on the original route. It preserves physical releases rather than fabricating
timers. Native's adapter uses the existing peer input state and pure C++ API.

`code_edit` exposes the inherited physical-key overload with a using declaration,
removing the hidden-virtual warning without disabling that diagnostic.
Native menu runtime checks cover main/child/main menu restoration and physical
press/repeat/release. The broader GEM input regression also covers focused
editor clipboard shortcuts; its result must accompany SDK qualification.

Follow-up evidence: all eleven CPC backend builds complete without compiler
warnings. All seven local Linux firmware render/close checks pass. The real
CDE session passes F10 pause/resume and clean window closure. Native's GEM
menu and editor-input regressions pass with ASan/UBSan. Actual Rasta viewer
input types `PRINT 123`, receives `123`, pauses with F10, opens the disassembly
window with an ASCII title, and restores the main menu on returning to it.
Windows GDB reaches main and closes its owned tunnel; elementary GDB reaches
main, renders in the VM Wayland session and exits normally.

CPC's debug optimization improves unpaced core throughput from 308% to 1492%
on Window Maker, 321% to 1284% on Haiku, and 508% to 1393% on macOS. These are
core benchmarks, not UI rates. Measured Window Maker display cadence is
53–59 paints/s. Haiku's full software CRT was below 1 paint/s and macOS around
6 paints/s. The consumer now defaults CRT off and keeps the original full
shader when enabled, without a lightweight replacement. Native's renderer
contract is unchanged.

The sibling GEM's complete RelWithDebInfo suite remains at 96/101, matching
the five previously recorded failures (demo34 direct/proxy reference images,
gemread direct/proxy harness, and gemscape browser metrics). A full Debug run
also exposes existing duplicate `global` symbol sanitizer failures in direct
applications. These are not claimed repaired. Five focused GEM API/transport
checks pass under ASan/UBSan, including standard MU_KEYBD compatibility.

### Default CRT and window activation follow-up

CPC now starts with CRT disabled on every backend. F11, the CRT menu or `--crt`
requests the original full shader, without the earlier slow-render fallback.
Both Haiku softpipe and macOS software OpenGL reach shader status `active`
and retain the full effect through capture and shutdown. Mac's initial content
is exactly 768 by 544; Haiku required the Native geometry repair described below.

Haiku activation focuses the plain client if no view is focused, preserving an
existing control's focus. Its geometry setters add the BeAPI menu extent so
the requested dimensions describe content. The extended dispatch regression
passes on `hotel`, covering repeated sizing, default activation focus, control
focus retention and two-looper callback serialization. Actual VM keyboard
events reach CPC after clicking only the title tab.

### Haiku window-movement flash

Haiku's collection host no longer clears the live view before portable
painting. It suppresses app_server erasure and reuses a view-owned bitmap to
compose the background and content, then copies completed damage to the
screen. The Native API and original CRT shader remain unchanged.

The Haiku dispatch, collection and menu runtime checks pass with this backend.
The dispatch check additionally samples the previously presented screen pixels
during a subsequent repaint. A 100-capture CPC title-drag run retained visible
content and exited normally; ordinary repaint checks alone did not reproduce
the original reported flash. CPC's updated binary is deployed to `hotel`, and
its CRT default stays off. Consumer evidence is under `build/uat3/`.

# elementary OS VM runtime

The configured SSH host `echo` runs elementary OS 8.1 (Ubuntu 24.04 base),
x86-64, with the `pantheon-wayland` login session, Gala and
`/run/user/1000/wayland-0`. Verified on 2026-10-20: GTK 4.14.5 and Granite
7.8.1. Build with the Docker targets described in the Build System chapter;
copy those binaries to the VM rather than compiling against host packages.

## VS Code F5

Select **Elementary OS, VM**. Its deployment task builds `docker-elementary`
and runs `scripts/linux/elementary-vm/deploy.sh`. Vision, its complete
retro-terminal assets and a private GtkSourceView library are copied to
`/home/tomaz/Projects/native/run`. This F5 deployment uses `LD_LIBRARY_PATH`
in the SSH debugger environment and does not patch executables. The
`debug-pipe.sh` script runs GDB in the existing `wayland-0` session, selects
Cairo rendering, clears `DISPLAY`, and leaves the native desktop theme in
control. `ELEMENTARY_VM_HOST` can override `echo`; paths still target user
`tomaz`. Required remote tools are GDB, rsync and SSH. Deployment diagnostics
stay on stderr so GDB/MI stdout remains clean.

Verified deployment and GDB/MI startup on `echo`; the deployed Vision reaches
a source breakpoint in `program()`. This startup check precedes GTK display
initialization and does not resolve the desktop timeouts described below.

## Regression-suite deployment

The test executables and generated `tests/CTestTestfile.cmake` retain absolute
build paths. On this VM they are deployed below
`/home/tomaz/repos/wischner/native/build/linux-elementary/` and
`build/linux-wayland/`. Copy each build's `tests/native_*` executables,
`tests/glsl-fixture/`, `tests/CTestTestfile.cmake`, `src/vision` and complete
`src/retro-terminal/` directory. Also copy repository `tests/data/` and
`src/assets/` at their original absolute paths for embedded fixture references.
Do not copy build object files or replace an existing checkout.

The VM has no GtkSourceView 5 runtime and passwordless sudo is unavailable.
The test deployment therefore supplies the Docker SDK's
`libgtksourceview-5.so.0` in `/home/tomaz/native-elementary-runtime/`.
Only deployed executable copies receive that directory as a RUNPATH using
`patchelf`; the repository build artifacts remain unchanged. This also allows
the child-process test to clear its environment. Copy the same library into
each `tests/glsl-fixture/`, whose test overrides `LD_LIBRARY_PATH`.
`patchelf` was extracted from its downloaded Ubuntu package in the private
runtime directory without installing system packages. Normal distribution
should declare `libgtksourceview-5-0` as a runtime dependency instead.

## Execution

Run as the logged-in desktop user, without X environment or forwarding:

```bash
ssh echo
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-0
export GDK_BACKEND=wayland
export GSK_RENDERER=cairo
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
export ASAN_OPTIONS=detect_leaks=0
unset DISPLAY
ctest --test-dir /home/tomaz/repos/wischner/native/build/linux-elementary/tests --output-on-failure --timeout 30
ctest --test-dir /home/tomaz/repos/wischner/native/build/linux-wayland/tests --output-on-failure --timeout 30
```

Run GUI suites sequentially. Cairo avoids dependence on the VM's accelerated
GTK renderer; the EGL renderer regression independently exercises Mesa.
Vision is at `build/linux-elementary/src/vision`, with its assets beside it.

## Verification and limits

On 2026-10-20, both rebuilt toolkit suites pass all 18 tests under Docker
Weston and on the actual Pantheon Wayland session on `echo`, with ASan/UBSan
and exit-time leak scanning disabled. The VM runs completed in 17.17 seconds
(elementary) and 16.18 seconds (generic GTK Wayland). The shared interaction
repair preserves native pointer targets, accounts for
header/menu height, and translates mnemonic labels to GTK underscore syntax.
The regression checks full requested client height, bottom-button visibility
and GTK hit testing through nested panels, native action delivery and mnemonic
labels with literal ampersands/underscores. The VM-rendered control snapshot
was inspected and shows the complete bottom row.

Earlier runs on this VM timed out in four GUI fixtures; GDB located the widget
fixture in `gtk_init_check()` waiting in `wl_display_roundtrip_queue()`. Gala
logged recursive main-context warnings then. These remain historical evidence;
the current full reruns pass and no session restart was performed by the
scripts. The optional control snapshot also succeeds in the current session.

Automated hit testing and native activation do not simulate physical mouse
input. Physical input, clipboard exchange with other applications, interactive
file portals and accessibility remain manual acceptance limits. The screenshot
checks the control fixture, not every Vision dialog or desktop configuration.

### GTK close lifetime follow-up

A child input-reset callback closing its parent reproduced an ASan
heap-use-after-free in the shared GTK destroy path. Container ownership alone
allowed the child widget to be freed before its pending teardown. Peers now
hold an explicit GTK reference, transfer it locally during removal, and retain
the native parent across unparenting callbacks. Regression coverage also
closes the parent from native unmapping, verifies balanced GTK reference
release, and sends a GTK close request with a native editor focused.

The lifetime repair passes all 18 tests in each Docker-backed toolkit and 25
repeated elementary close regressions under ASan/UBSan. The deployed elementary
VM fixture passes 10 repeated runs with `G_DEBUG=fatal-criticals`. Vision and
its complete assets are redeployed through the normal VM deployment script.
The 18-test VM suite results above are from the preceding interaction repair;
this follow-up reruns the focused close fixture on the VM.

### Native GTK/Granite theme rendering

The theme adapter now uses actual GTK widgets and native CSS-part functions.
Complete controls include Granite entries through the same factory used by the
live elementary controls. Window targets draw GTK render nodes into the native
Cairo frame with the original device transform, clip and offset. Image targets
rasterize those same nodes; no separate emulated skin remains. The gallery's
button primitive uses measured native height.

Both full Docker suites pass 18/18 after this change. The latest elementary VM
suite passes 18/18 in 19.73 seconds. Its focused theme fixture passes 96 exact
pixel comparisons against live native controls: checked/unchecked indicators,
button/entry/list presentation, six interaction states, light/dark styles and
controlled 2x window-frame rendering with offsets and clips. A side-by-side
VM-rendered image was inspected; native and primitive columns match.

The subsequent generic GTK VM suite passed 15/18; close, widget and collection
fixtures timed out. GDB located the wait in `gtk_init_check()` →
`wl_display_roundtrip_queue()` before any theme rendering. Gala again logged
recursive main-context prepare/check warnings. This identifies the client's
blocked stage, not the cause of the compositor condition. No script restarted the session. After the compositor recovered, all three
affected generic fixtures passed focused reruns. Both final deployed toolkit
fixtures pass all 96 pixel comparisons. The HiDPI reference captures the live
control directly at the same target offset, scale and clip as the primitive;
relocating an already-rasterized reference introduced small Cairo edge-rounding
differences on the VM.

The VM subsequently ran out of disk during deployment. Removing only staged
qualification executables from the remote `build/linux-elementary` and
`build/linux-wayland` trees recovered space; their source, test metadata and
logs remain. Those full-suite executable copies must be redeployed before
running CTest there again. Final Vision deployment succeeded. Earlier generic
VM runs passed all 18 tests.

To capture the focused comparison with the deployed elementary fixture:

```bash
scripts/linux/elementary-vm/debug-pipe.sh /usr/bin/env -u GTK_THEME \
  G_DEBUG=fatal-criticals \
  NATIVE_GTK_THEME_SNAPSHOT=/home/tomaz/Projects/native/run/theme-parity.png \
  /home/tomaz/Projects/native/run/native_wayland_runtime_tests
```

Deploy that test executable separately from `build/linux-elementary/tests/`;
the normal F5 deployment copies Vision and its assets. `GTK_THEME` is cleared
for this check so the light/dark preference exercises two distinct installed
variants. The test restores its process-local preferences and animation setting.
Physical input, interactive portals, accessibility and actual scaled-monitor
acceptance remain separate from controlled pixel comparisons.

### Modeless stacking follow-up (2026-10-20)

GTK modeless windows now omit a native transient parent so the main and
modeless windows can stack independently. Portable ownership remains intact;
modal dialogs retain GTK transience and modality. The updated elementary VM
fixture passes assertions for both relationships, and fixed Vision is deployed
to the normal F5 location. These assertions verify the GTK configuration;
physical window switching remains desktop acceptance.


### Settings sidebar spacing (2026-10-21)

The GraniteSettingsSidebar adapter now opts into the installed theme's
`sidebar` class. Native CSS supplies row padding, background, border and
selection colors. A six-logical-pixel column gap separates images and
labels when Granite's grid has no spacing; native nonzero spacing is kept.
This fixes flush-to-edge icons without replacing Granite's row renderer.

The Docker elementary suite passes 20/20 with the elementary stylesheet.
The normal deployment script updated Vision on `echo`. A debugger-owned
Vision process rendered the actual settings window under the VM's Granite
7.8.1 and installed desktop theme; the captured navigation has clear row
insets and icon/title separation. The verification used native GTK snapshots,
not a forced Docker theme or a replacement drawing path. It does not claim
a physical-input walkthrough.

The date and time fields remain GraniteDatePicker and GraniteTimePicker.
The optional seconds editor is a GtkSpinButton composed beside the latter;
Granite's own popup edits hours and minutes. No emulated picker was added.


### Shared GTK resize and row-image correction (2026-10-21)

The fixed child host now stays out of overlay measurement and is clipped to
the current viewport. Its current allocation no longer raises the window's
minimum size after enlargement. Native GTK tree rows now display optional
image textures, which also makes the generic GTK settings navigator show its
supplied images. The full GTK and elementary Docker suites each pass 20/20;
the enhanced settings fixture verifies real texture pixels and compositor
allocation after growing and shrinking both dimensions. The existing
bottom-row geometry/picking checks still pass.

The normal deployment script updated the elementary Vision executable after
this shared correction. The production generic GTK gallery was inspected
under headless Weston and returned to its initial size after enlargement;
that capture is separate from a complete interactive Pantheon walkthrough.


## Clipboard and native control review (2026-10-21)

Both the generic GTK and elementary Docker suites pass 20/20 after the control
review. The deployed elementary build also passes the focused GTK runtime,
settings runtime and collection runtime checks on `echo`, using its installed
desktop theme. The GTK runtime reports 96 live-control pixel comparisons.
These checks exercise local clipboard copy/paste without blocking its provider,
Ctrl/Shift table selection, Tab completion, splitter movement, editable native
combos, date entry initialization/editing and native sidebar image sizing.

Ordinary buttons retain their original native GtkButton hierarchy. The later
2026-10-22 review replaces the clipped-overlay approach for partial borders
with scoped CSS on that original button. Selected edges use the desktop
border color; restoring all edges removes the override and restores the
complete installed theme.

The desktop locked during the final visible picker walkthrough, so no final
unlocked-desktop date-popup screenshot is claimed. The native snapshot and
callback checks above completed successfully; no desktop lock setting was
changed. Vision and its private runtime were deployed through the normal
script, and only the test-owned demo process was stopped afterward.

## Table, splitter and border review (2026-10-22)

Both GTK selections pass all 20 CTests under headless Weston; the elementary
run explicitly uses `io.elementary.stylesheet.blueberry`. The native GTK
fixture covers complete pointer press/release sequences with Ctrl/Shift,
collapsed groups remaining collapsed, ordinary typing and middle insertion
in editable combos, fifty split allocations/orientation changes and pending
callback destruction. Native snapshots check all sixteen button edge masks.

The deployed gallery on `echo` confirms physical Ctrl multiselection, Shift
range selection, group collapse after release and repeated native splitter
drags. Live typing preserves `hello` in order; native tab labels,
individual preview edges, the fully borderless preview and the property
grid's right frame were also inspected. The separator retains native theme painting with a usable allocated
strip; portable ratio notifications run after GTK allocation. Tabs remain
GtkNotebook, with padded native labels; Granite 7 supplies no separate tab
widget in the pinned SDK. The property grid reserves its right frame outside
the scrolling strip. Screenshots use `build/elementary-control-review-*.png`.

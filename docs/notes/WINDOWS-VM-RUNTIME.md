# Windows VM Runtime and Debugging

The **Windows 11, VM** F5 profile targets the Windows 11 desktop reachable
through the SSH alias `whiskey`. It does not invoke Wine. Builds
remain Docker-backed through `docker-win`; the guest receives the resulting
MinGW `vision.exe`, three compiler runtime DLLs, the Windows App SDK
bootstrap DLL and the SDK's native Win64
`gdbserver.exe`. No source code or Linux executable is run in the guest.

## Connection and guest preparation

The scripts use `ssh whiskey`, including the account, key and host settings in
SSH configuration. The current account is `WHISKEY\tomaz`; an interactive
desktop must be logged in. Override `WINDOWS_VM_HOST` or `WINDOWS_VM_USER`
for another setup. Setting `WINDOWS_VM_DOMAIN` additionally enables optional
libvirt startup; the default does not depend on libvirt or lease discovery.

Run `scripts/windows/vm/setup-ssh.ps1` once in an elevated guest PowerShell,
passing the host's **public** ed25519 key as `-AuthorizedKey`. It installs
Windows OpenSSH Server, enables automatic startup and public-key-only login,
and restricts its inbound SSH rule to the libvirt host (`192.168.122.1`,
override with `-HostAddress`). It preserves an initial `sshd_config.native-backup`.
Administrator keys use `C:\ProgramData\ssh\administrators_authorized_keys`
with access restricted to Administrators and SYSTEM, following Microsoft's
[Windows OpenSSH key guidance](https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_keymanagement).
No private key is copied into Windows.

The scripts require strict host-key checking and non-interactive key
authentication using normal SSH configuration and known-hosts files. They do
not accept unknown or changed server keys automatically. The existing
`whiskey` SSH connection supplies these settings.

Install x64 Windows App Runtime 1.8 for the genuine WinUI 3 NavigationView.
`prepare.sh` checks the current user's installed
`Microsoft.WindowsAppRuntime.1.8` package before deployment. The runtime was
already installed on `whiskey` during the 2026-10-22 review. The build image
is `native-windows-winui-sdk:24.04`, built by `docker-win-sdk`; it supplies
matching GCC 13 runtime DLLs and `Microsoft.WindowsAppRuntime.Bootstrap.dll`.
Do not mix compiler runtime DLLs from the older cross-build image.

## F5 workflow

Select **Windows 11, VM**:

1. The existing Docker Debug build produces `build/windows-mingw-w64/`.
2. `scripts/windows/vm/prepare.sh` waits for SSH (optionally starting an explicitly
   configured libvirt domain),
   synchronizes changed executable/runtime/debugger files to
   `C:\NativeDebug\native`, and registers the `Native-Vision-Debug` task.
   It refuses deployment while that directory's Vision/debugger is running.
3. `scripts/windows/vm/debug-pipe.sh` starts the task with the logged-in user's
   interactive token, then connects Docker's MinGW-aware GDB through an SSH
   tunnel. A normal SSH process runs in session 0, so it is not used as the
   GUI program's parent. The task runs at limited privilege on the desktop,
   using `wscript.exe` to start PowerShell hidden before its console can be
   displayed, plus a console-less debug server.
   VS Code uses this wrapper as `miDebuggerPath`, with a remote-server
   address. An identity source map retains full breakpoint paths, avoiding
   ambiguity between the application's and backend's `main.cpp` files.
4. Native GDBserver uses guest port 2345; the host tunnel listens only on
   `127.0.0.1:31337`. A guest firewall rule blocks non-loopback access to 2345.
   This is necessary because [GDBserver ignores the host portion of its listen
   address](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Server.html).
5. Tunnel setup uses a workspace-specific SSH control socket and waits for
   forwarding to succeed before starting the guest debugger. An occupied port
   fails startup immediately. When GDB exits, the wrapper stops this directory's
   debug processes and closes its SSH master. `Stop Vision Debug (Windows VM)` is also a
   `postDebugTask`, covering adapter failures that terminate the wrapper.
   The post-debug task also closes this workspace's orphaned SSH master; it never
   kills an arbitrary process holding port 31337. Cleanup leaves the VM and
   other applications running. The scheduled task
   and deployed files remain available for subsequent launches.

Session diagnostics use stderr, preserving GDB/MI stdout. The scripts do not
store a Windows password, change autologon, reboot the guest, or alter Windows
Update. The full local executable retains debug information; the staging executable
uses `objcopy --strip-debug` before deployment. DLL and asset contents remain
unchanged. The DLLs are `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and
`libwinpthread-1.dll`, resolved from the same Docker cross-compiler that builds
Vision. GDBserver is supplied by that image, not downloaded from an unrelated
binary provider.

The host needs OpenSSH, Docker, `flock`, `iconv` and `base64`. Libvirt tools
are needed only when `WINDOWS_VM_DOMAIN` is explicitly configured.
The VM must retain its SSH host keys and have its interactive user logged in.
An occupied host or guest debug port is an error, not a reason to kill an
unrelated debugger.

## Incremental deployment and DLL cache

`prepare.sh` extracts runtime tools from the Docker SDK and produces
`build/windows-vm/vision.exe` without debug sections. The full
`build/windows-mingw-w64/src/vision.exe` remains the debugger's symbol source.
`sync.py` compares SHA256 hashes against the remote deployment, archives only
changed files and installs the scheduled task only when its launcher changes
or the task is absent. Unchanged assets, runtimes and executables are skipped.
The host requires Python 3.11 or newer; Windows uses its built-in `tar.exe`,
PowerShell and Windows Script Host, with no additional software installation.

The desktop launcher collects the exact DLL versions loaded by Vision,
including its manifest-selected WinSxS libraries, in
`C:\NativeDebug\native\gdb-libraries`. Preparation synchronizes changed
DLLs into `build/windows-vm/libraries`, verifies their hashes and supplies
case aliases for Windows names. GDB uses a local sysroot, DOS path semantics
and local library search paths when this cache is ready. The launch profile
explicitly includes that search path so the C++ adapter does not replace it;
current SDK runtime DLLs take precedence over cached copies. The first uncached
launch reads DLLs remotely; subsequent preparations import its cache.
Windows build/revision changes invalidate it and trigger another cold run.
Newly loaded modules are captured for the next preparation. Cache collection
errors are recorded in `gdb-libraries/error.log` and do not abort debugging.

Windows F5 configure/build/deploy/stop tasks use quiet presentation without
focusing the Linux terminal panel. Output remains available in the task log.

## Runtime checks (2026-09-05)

The Docker-built gallery was launched on the Windows 11 desktop and its
Activate button updated the activation counter. The native Windows binaries
`native_core_tests`, `native_window_api_tests`, `native_table_model_tests` and
`native_code_document_tests` all returned zero in the guest. Run these from a
writable working directory: the window API test exercises image file I/O.
These checks did not use Wine.

The native-control correction adds `native_windows_runtime_tests`. Run it
as the logged-in interactive user (for example, with an interactive scheduled
task), not directly in SSH session 0: it tests the desktop visual-style service
and real window painting. It covers native control styles, derived painting
overrides, native list row metrics, text/clipboard and combo notifications,
splitter capture, parent/child paint clipping, GDI resource stability, and
modal sibling exclusion. The expanded suite also checks grouped grid-line
pixels and toggling, native classic tabs on all four edges, status-strip
clipping and hit-testing after several resizes, and independent modeless
stacking. Owner-data grid checks cover both axes independently, disabled
grids and newly requested rows after scrolling. Vertical tab checks compare
short and long native label extents with actual item dimensions, guarding
against the native minimum-width gap. It returned zero in the Windows 11
desktop session.
The subsequent walkthrough checked collections, native report tables, layout
maximize/restore, splitter and pane input, combo and tab selection, native
message/folder dialogs, and multiline typing. Screenshots are development
artifacts under `build/windows-native-*.png`.

The grid/tab/chrome follow-up visually checked grid lines in materialized
groups, matching classic tabs on all four edges, a shrunk window whose tab
children extend behind the intact status strip, and activation that brings
the main window in front of its modeless window. These screenshots use the
`build/windows-grid-*`, `windows-tabs-*`, `windows-status-*`, and
`windows-modeless-*` prefixes.

The VS Code C++ debug adapter launches the native target and hits
`src/main.cpp:16` with the correct full source path. Disconnecting while
paused at that breakpoint also passed with the image's GDBserver 10.2.
A direct GDB session
also displayed the gallery and exited normally when its window was closed.
Both Vision and GDBserver were verified in interactive session 1. The guest
executable's SHA256 matched the Docker build. Direct network access to 2345
was blocked while the loopback SSH tunnel worked.

**Open limitation:** Pause/Stop while the GUI is running can hang waiting for
a remote interrupt. The adapter sends `-exec-interrupt` and the RSP connection
carries Ctrl-C, but the target does not stop. This was reproduced with both
the image's GDBserver 10.2 and an isolated MSYS2 GDBserver 17.2; the latter is
not part of the maintained deployment. Close Vision's window to end a
running session cleanly. Do not describe running-target interruption as
verified. Breakpoints and native execution are independently verified.

The 2026-09-07 inspector regression was deployed separately under
`C:\NativeDebug\inspector-fix` and passed in the interactive Windows 11
desktop session. It verifies native checkbox/text/number input, presented
toolbar-icon and property-text pixels, repeated toolbar commands with status
updates, sticky selection, bottom-edge input, scrolling, and owner recreation.
Property combos also pass the unified value-background pixel check and a
native popup selection check. The follow-up covers post-creation grid
resizing, live toolbar growth/shrinkage and neighboring-bar hits, and custom
dropdown values, including Cancel and callbacks retained from a previous
popup session. Posted assertions and popup commits use the production
message-loop wakeup rather than a test timer.
The temporary regression task was removed after validation; the normal
Vision deployment and debug task were retained.

## Removing the development setup

Close debugging first. The guest task is `Native-Vision-Debug`, the additional
GDB firewall rule is `Native-GDB-Local-Only`, and deployed files are confined
to `C:\NativeDebug\native`. Remove only those resources if uninstalling this
workflow. SSH is a separate installed Windows feature: restore its saved
configuration and key/firewall policy only if no other workflow needs it.

The deployment also copies Vision's external `retro-terminal/` effect/font
assets and license notices. Run `vision.exe --retro-terminal --terminal-smoke`
on the interactive desktop for the shell/output/CRT acceptance check. The
portable pixel test can take an explicit asset directory:
`native_terminal_screen_tests.exe C:\NativeDebug\native\retro-terminal`.

## Non-Amiga repair verification (2026-10-15)

An interactive desktop task ran 18 cross-built regression executables: 16 passed,
and the desktop-context and real-renderer tests returned skip code 77 because
this VM has no suitable OpenGL 3.3 context. Native controls, callback/creation
lifecycle, close cancellation/acceptance, service/process/audio-queue models and
independent worker shutdown passed. This does not prove WGL shader execution or
real-device sound. The final rebuilt suite was deployed with user approval and passed with the
same two skips; all deployed test SHA256 hashes match the Docker outputs. The
temporary Native-Adjustment-Tests task was removed; Native-Vision-Debug remains.

Copy tests/data beside the tests and pass --data data to native_mouse_shader_tests;
pass retro-terminal to native_terminal_screen_tests. Preserve the MinGW runtime
DLLs and attributed asset directory. Run GUI regressions in the interactive user
session, not the SSH service desktop. Temporary acceptance tasks should be removed
after verification; retain Native-Vision-Debug for the maintained debugger flow.

## SSH alias and F5 verification (2026-10-20)

The current **Windows 11, VM** profile uses `ssh whiskey` (account
`WHISKEY\tomaz`, Windows 11 Home build 26300). The Docker MinGW Debug build
completed, and `prepare.sh` deployed Vision, its complete retro-terminal
assets, matching runtime DLLs and GDBserver. The existing interactive task and
local-only debug-port protection were installed through the same script.

The installed VS Code C++ adapter (`OpenDebugAD7`, cpptools 1.34.4) launched
this profile and hit `src/main.cpp:24` and the Windows application loop at
`lib/native/platforms/windows/app.cpp:50`, with correct full source paths.
Both Vision and GDBserver ran in session 1. A temporary interactive desktop
probe confirmed a visible main-window handle and the title
`Vision Native Feature Gallery`; it closed that window normally, and the
adapter reported exit code 0 and successful disconnect. The temporary probe
task was removed. A separate paused-breakpoint disconnect also passed.
These checks validate build/deployment and F5 startup, breakpoints and normal
shutdown; they do not repeat the historical full backend acceptance suite.

## Editor I-beam repair (2026-10-20)

The native-peer fixture reproduced an arrow over a focused Win32 text editor.
Root keyboard-focus loss to a stock child was marking the application pointer
state inactive and suspending child hover; stock editors consume their own
mouse-move messages. The Windows backend now tracks top-level activation,
recognizes focus transfer inside the same native root, and resumes hover from
forwarded client cursor messages with a window-lifetime guard.

The final Docker build and interactive `native_windows_runtime_tests.exe` on
`whiskey` pass. Cursor assertions cover both editor modes, root-to-editor focus
transfer, explicit crosshair overrides, inactive-root arrow selection and
reactivation. Existing native-peer, modeless/modal, grid, tab and chrome
regressions also pass in that executable. The normal deployment script copied
the corrected Vision binary and assets to the F5 directory.

## Tunnel recovery (2026-10-20)

An orphaned workspace SSH process held host port 31337 after the launcher
exited. The old asynchronous startup started the Windows server before
noticing a failed tunnel. Startup now waits for SSH authentication and port
forwarding to succeed, using a dedicated control socket at
`build/windows-vm-ssh.sock`. The SSH master does not inherit the launcher lock.
Normal exit, the next locked startup and the post-debug task can close that
workspace master; an unreachable stale socket is removed. Other processes
holding the port are left intact and produce an immediate startup failure.

Validation reproduced an occupied port and confirmed that no desktop server
was started. The actual C++ adapter then hit application and event-loop source
breakpoints, displayed Vision in desktop session 1 and reported normal exit
code 0 after window closure. A simulated leftover control master was removed
by `stop.sh`, and a fresh bind confirmed port 31337 was released. The original
verified orphan was stopped during this repair.

## Faster F5 verification (2026-10-20)

The staged executable is about 17 MiB versus 69 MiB with full local debug
information. Unchanged preparation transferred zero deployment files and,
after the initial cache fill, zero DLLs. Forty loaded DLLs were cached. A
cold adapter launch reached the first application breakpoint in 14.57 seconds;
the completed cache configuration reached it in 8.91 seconds, then 8.89
seconds after verifying SDK runtime search precedence. These are launch-to-breakpoint observations, excluding build and
preparation, rather than a guarantee for all VM loads.

The actual C++ adapter hit application and Windows event-loop breakpoints,
loaded cached system DLL symbols without remote DLL reads or missing-library
messages, displayed the gallery and reported exit code 0 after window
closure. The interactive probe found no visible window handles for WScript,
the launch PowerShell process or GDBserver. The Windows-specific task
presentation uses `reveal: never`; compiler problems may still reveal the
Problems panel, and task output remains accessible manually. DLL case aliases
cover the reported Windows spellings while preserving verified bytes.

## Quiet SSH and deployment output (2026-10-20)

VM scripts suppress only OpenSSH's non-post-quantum key-exchange advisory
using `WarnWeakCrypto=no-pq-kex`; they retain strict host-key verification and
other SSH diagnostics. `IgnoreUnknown=WarnWeakCrypto` supports older host
clients without that option. This changes warning presentation, not the
server's negotiated cryptography. Successful deployment/cache summaries are
silent by default; set `WINDOWS_VM_VERBOSE=1` to display them. Failures still
report normally and retain their nonzero exit status.


On 2026-10-21 the Docker-built settings regression passed model and native
runtime modes in the logged-in `whiskey` desktop. The temporary limited-user
task used an isolated `C:\NativeDebug\settings-check` deployment and existing
MinGW runtime DLLs. It asserts DATETIMEPICK_CLASS widgets, native WM_NOTIFY
handling, unavailable-date rollback, time changes and page recreation.
Programmatic DTM_SETSYSTEMTIME need not notify, so the fixture also delivers
the corresponding user-edit notification explicitly. This is callback
coverage, not a physical mouse/calendar walkthrough.


## Native control review (2026-10-21)

Four focused runs pass in the logged-in `whiskey` desktop: native Windows
controls, inspector runtime, settings model and settings runtime. The isolated
`C:\NativeDebug\bugs-review` deployment uses the completed Docker build and
its MinGW runtime DLLs. The temporary interactive task was removed afterward.

Checks include transparent iconless tree rows and native child indentation,
all 16 stock-button border masks without changing button class, centered modal
windows, preferred-size date/time fields, real inspector hit testing, native
flat toolbar commands/toggles and property editor scrolling/recreation. The
inspector screenshot confirms the native toolbar, visible values and light
dotted grid. Native toolbar activation preserves the editor's focus, including
a combo's ordinary system selection highlight. Common file dialogs and message
boxes use a thread-scoped activation hook to center on their native owner.

## Standard input and footer checks (2026-10-22)

The final Docker MinGW build passes four isolated interactive checks on
`whiskey`: native Win32 controls, inspector, settings model and settings
runtime. Single-line EDIT retains its native border while WM_NCCALCSIZE
centers the font-height client and caret; native control-color brushes fill
the resulting inner padding. The settings fixture checks that client geometry
and repeated size requests, alongside equal entry/combo/picker heights.
The review task is removed afterward. The tested Vision is deployed through
`prepare.sh` to the normal F5 location.

## WinUI navigation and control review (2026-10-22)

The pinned GNU/WinUI Docker SDK builds the actual WinUI 3 NavigationView host.
The existing x64 Windows App Runtime 1.8 in `whiskey` resolves it through the
adjacent bootstrap DLL. Native sections, ImageIcon rows, selected-row styling
and AutoSuggestBox search were inspected on the interactive Windows desktop.
Physical mouse selection switches Dates/Time pages, and typed search filters
rows while retaining the selected page. Closing the gallery through its title
bar returns status zero and leaves no Vision process.

The four focused interactive runs all return zero: Windows controls,
inspector, settings model and settings runtime. The settings fixture checks
a visible DesktopChildSiteBridge and native date/time text padding in addition
to page recreation and native callbacks. Screenshots use the
`build/windows-winui-navigation-*.png` prefix. Runtime execution uses the
Windows desktop; Wine is only a build-time SDK generator dependency.

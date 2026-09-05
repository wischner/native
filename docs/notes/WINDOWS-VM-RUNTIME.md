# Windows VM Runtime and Debugging

The Windows F5 profile targets the native Windows 11 desktop in the libvirt
guest **Windows** (formerly `Windows10`). It does not invoke Wine. Builds
remain Docker-backed through `docker-win`; the guest receives the resulting
MinGW `vision.exe`, three runtime DLLs and the SDK's native Win64
`gdbserver.exe`. No source code or Linux executable is run in the guest.

## Connection and guest preparation

The reference VM UUID is `95319e95-537b-4479-9b79-53419fe70fe9`; using the
UUID keeps the workflow independent of the VM's display name. The scripts
discover its IPv4 address from the libvirt lease. The reference SSH account
is `tomaz stih` and the autologon desktop must be available. Override
`WINDOWS_VM_DOMAIN`, `WINDOWS_VM_HOST` or `WINDOWS_VM_USER` for another setup.

Run `scripts/windows/vm/setup-ssh.ps1` once in an elevated guest PowerShell,
passing the host's **public** ed25519 key as `-AuthorizedKey`. It installs
Windows OpenSSH Server, enables automatic startup and public-key-only login,
and restricts its inbound SSH rule to the libvirt host (`192.168.122.1`,
override with `-HostAddress`). It preserves an initial `sshd_config.native-backup`.
Administrator keys use `C:\ProgramData\ssh\administrators_authorized_keys`
with access restricted to Administrators and SYSTEM, following Microsoft's
[Windows OpenSSH key guidance](https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_keymanagement).
No private key is copied into Windows.

Verify the displayed guest ed25519 host-key fingerprint independently against
the key collected from that guest. Store the verified public host key under
the alias `native-windows-vm` in `~/.ssh/native_windows_known_hosts`. The scripts
require strict host-key checking and non-interactive key authentication;
they do not accept unknown or changed server keys automatically.

## F5 workflow

Select **Debug Vision (Windows 11 VM)**:

1. The existing Docker Debug build produces `build/windows-mingw-w64/`.
2. `scripts/windows/vm/prepare.sh` starts the VM if needed, waits for SSH,
   copies the exact executable/runtime/debugger files to
   `C:\NativeDebug\native`, and registers the `Native-Vision-Debug` task.
   It refuses deployment while that directory's Vision/debugger is running.
3. `scripts/windows/vm/debug-pipe.sh` starts the task with the logged-in user's
   interactive token, then connects Docker's MinGW-aware GDB through an SSH
   tunnel. A normal SSH process runs in session 0, so it is not used as the
   GUI program's parent. The task runs at limited privilege on the desktop,
   using a hidden PowerShell launcher and a console-less debug server.
   VS Code uses this wrapper as `miDebuggerPath`, with a remote-server
   address. An identity source map retains full breakpoint paths, avoiding
   ambiguity between the application's and backend's `main.cpp` files.
4. Native GDBserver uses guest port 2345; the host tunnel listens only on
   `127.0.0.1:31337`. A guest firewall rule blocks non-loopback access to 2345.
   This is necessary because [GDBserver ignores the host portion of its listen
   address](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Server.html).
5. When GDB exits, the wrapper stops this directory's debug processes and
   removes its tunnel. `Stop Vision Debug (Windows VM)` is also a
   `postDebugTask`, covering adapter failures that terminate the wrapper.
   Cleanup leaves the VM and other applications running. The scheduled task
   and deployed files remain available for subsequent launches.

Session diagnostics use stderr, preserving GDB/MI stdout. The scripts do not
store a Windows password, change autologon, reboot the guest, or alter Windows
Update. The DLLs are `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and
`libwinpthread-1.dll`, resolved from the same Docker cross-compiler that builds
Vision. GDBserver is supplied by that image, not downloaded from an unrelated
binary provider.

The host needs libvirt tools, OpenSSH, Docker, `flock`, `iconv` and `base64`.
The VM must retain its SSH host keys and have its interactive user logged in.
An occupied host or guest debug port is an error, not a reason to kill an
unrelated debugger.

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

## Removing the development setup

Close debugging first. The guest task is `Native-Vision-Debug`, the additional
GDB firewall rule is `Native-GDB-Local-Only`, and deployed files are confined
to `C:\NativeDebug\native`. Remove only those resources if uninstalling this
workflow. SSH is a separate installed Windows feature: restore its saved
configuration and key/firewall policy only if no other workflow needs it.

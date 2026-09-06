# macOS Remote Runtime (leia)

This note records the current remote macOS workflow used from Linux.

## Target

- Host: `leia`
- User: `tomaz`
- Remote project path: `/Users/tomaz/Projects/native`

The scripts are parameterized through environment variables:

- `MAC_REMOTE_HOST` (default `leia`)
- `MAC_REMOTE_USER` (default `tomaz`)
- `MAC_REMOTE_BASE` (default `/Users/<user>/Projects`)
- `MAC_REMOTE_PROJECT` (default local repo basename)

## Scripts

- `scripts/macos/remote/sync.sh`
  - rsync source tree to macOS target
- `scripts/macos/remote/build.sh`
  - sync + configure + build the `Debug` application bundle on macOS
    (`build/macos-debug/src/vision.app`)
- `scripts/macos/remote/run.sh`
  - run Vision on macOS over SSH
- `scripts/macos/remote/debug.sh`
  - verify Developer Tools authorization and run Vision under remote `lldb`
- `scripts/macos/remote/smoke-test.sh`
  - build on macOS, verify binaries, clear/check quarantine xattr,
    ad-hoc sign, and launch/kill Vision for a short smoke test

## Why this avoids Gatekeeper "internet download" rejection

The application bundle is compiled on the Mac itself from synced source. It is
not copied as a prebuilt internet artifact. The smoke test also verifies that
no `com.apple.quarantine` xattr remains on the bundle.

## VS Code tasks and launch

- Tasks now use the script-backed labels:
  - `Build Vision Debug (macOS leia)`
  - `Smoke Test Vision Debug (macOS leia)`
- The launch entry builds and starts the remote LLDB session through SSH.

## Native-control regression checks

The 2026-09-05 AppKit audit passes all six CTests on `leia`. The dedicated
`native_mac_runtime_tests` renders controls into AppKit bitmap targets and
verifies that stock controls do not invoke the portable painter. It checks
native text/image cells, group disclosure, icon selection, accordion scrolling
hosts, and the retained derived-button drawing extension. Follow-up checks
cover upright image text with straight-alpha blending, single-open accordion
scrollbar ownership after section changes and resizing, and table grid/stripe
contrast in both light and dark appearances. The general collection
lifecycle test is also registered on macOS. All six tests also
pass in a separate AddressSanitizer/UndefinedBehaviorSanitizer build (leak
detection disabled for process-lifetime AppKit resources).

### Reproduce the automated checks

From the Linux checkout, sync and build using the existing remote script,
then run CTest on the Mac (these commands use the default target above):

```bash
bash scripts/macos/remote/build.sh
ssh tomaz@leia 'export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH; cd /Users/tomaz/Projects/native && ctest --test-dir build/macos-debug --output-on-failure'
```

For the separate sanitizer audit, SSH into `tomaz@leia` and run:

```bash
export PATH=/opt/homebrew/bin:/usr/local/bin:$PATH
cd /Users/tomaz/Projects/native
cmake -S . -B build/macos-native-audit -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -pedantic -fsanitize=address,undefined' \
  -DCMAKE_OBJCXX_FLAGS='-Wall -Wextra -pedantic -fsanitize=address,undefined' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build/macos-native-audit -j8
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/macos-native-audit --output-on-failure
```

The explicit flags matter: the top-level automatic sanitizer configuration
targets GCC, not AppleClang. This audit tree is separate from the F5 Debug
bundle and does not change the launch configuration.

### Desktop permissions and visual coverage

The SSH login can build and run GUI tests in the logged-in desktop session.
After enabling Screen Recording for the SSH session, `screencapture` can
capture the desktop; the updated gallery has been inspected there. macOS may
also present an explicit `com.apple.sshd-session` permission prompt that the
console user must approve. Accessibility is a separate permission: remote
System Events input is now permitted after enabling the SSH session under
Privacy & Security → Accessibility (`/usr/libexec/sshd-session` on this Mac).
The console must remain logged in and unlocked: a locked session returns
black captures and hides application windows from Accessibility. The live
walkthrough found empty splitter panes and oversized tab content; those now
have dedicated geometry regressions, alongside native virtual-grid scrolling
and grid-style checks. The session locked before that complete-gallery
walkthrough finished. After unlocking, targeted desktop checks verified the
image-text, duplicate accordion scrollbar and table-contrast fixes; this does
not constitute a completed mouse-driven walkthrough of every gallery feature.
`NATIVE_MAC_TEST_SNAPSHOT` optionally names a PNG output path for
the regression window's own offscreen rendering, without desktop capture.

The targeted desktop checks confirmed upright text in the gallery image,
one scrollbar in the expanded single-open accordion, and clearer grids and
alternating rows in both table examples. Light/dark color contrast is covered
by automated AppKit color checks; this is not a claim of a full visual
walkthrough in both appearances. Screen Recording and Accessibility are
needed for remote desktop inspection and input, not for the test's own
offscreen bitmap rendering.

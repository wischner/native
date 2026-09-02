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

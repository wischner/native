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

- `tools/macos-remote/sync.sh`
  - rsync source tree to macOS target
- `tools/macos-remote/build.sh`
  - sync + configure + build `Debug` on macOS (`build/macos-debug`)
- `tools/macos-remote/run.sh [app|painter]`
  - run chosen example on macOS over SSH
- `tools/macos-remote/debug.sh [app|painter]`
  - open remote `lldb` session for the chosen example
- `tools/macos-remote/smoke-test.sh`
  - build on macOS, verify binaries, clear/check quarantine xattr,
    ad-hoc sign, and launch/kill both binaries for a short smoke test

## Why this avoids Gatekeeper "internet download" rejection

Binaries are compiled on the Mac itself from synced source. They are not
copied as prebuilt internet artifacts. The smoke test also verifies no
`com.apple.quarantine` xattr remains on built binaries.

## VS Code tasks and launch

- Tasks now use the script-backed labels:
  - `Build (macOS leia)`
  - `Run app-example (macOS leia)`
  - `Run painter (macOS leia)`
  - `Debug app-example (macOS leia, LLDB)`
  - `Debug painter (macOS leia, LLDB)`
  - `Smoke Test (macOS leia, no quarantine)`
- Launch entries mirror these run/debug flows through SSH.

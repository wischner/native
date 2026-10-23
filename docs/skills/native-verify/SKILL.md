---
name: native-verify
description: Verify a C++ application built with Native using its documented SDK builds, lifecycle checks, and control interactions. Use for acceptance or review of application changes, with distinct compile, runtime, and visual evidence.
---

# Verify a Native application

Read the consumer's build configuration and the Native checkout's
[build chapter](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md).
Use the dependency revision under test, not remembered API names or old suite
counts. Relative links here resolve in the Native checkout.

1. Identify which controls and behaviors changed. Inspect lifetime order,
   parent/create/show sequencing, borrowed models/pages, and signal argument
   types against current public headers and the corresponding book example.
2. Build with the existing Docker-backed backend target or the consumer's
   build inside the same SDK. Keep build artifacts in the root `build/`
   tree and backend trees separate. Check shared C++ with GCC/Docker first;
   macOS compilation uses the remote Apple SDK. Do not silently substitute a
   host compiler or claim a cross-build was executed on the target.
3. Run checks that cover the change. For application UI, exercise input,
   selection and programmatic restoration, resize larger then smaller, switch
   pages, reopen popups, cancel dialogs, and close the window with its active
   resources. For dates, test an allowed date and an excluded one through
   both text and calendar. For tables, test range/toggle selection when used.
4. Match desktop to toolkit: GTK uses Wayland; Motif/CDE, OPEN LOOK and Window
   Maker have separate nested desktop launchers. Use existing remote VM
   configuration when the task requires it; do not assume aliases or desktop
   access are available in another checkout. Consult
   [exceptional runtime notes](../../notes/README.md) only for that target.
5. Preserve real diagnostics. Separate application failures from documented
   SDK exceptions using the current
   [feature matrix](../../manuals/book-of-native/FEATURE-MATRIX.md).
   Do not add blanket sanitizer suppressions to obtain a passing report.
6. Report exact commands, results, and untested scope using the
   [handoff template](../../ai/REQUEST-TEMPLATE.md). Update affected usage docs.
   A syntax check proves declarations/types, a link proves dependency closure,
   and a runtime check proves only the interactions it actually exercises.

Do not broaden a small application edit into a backend rewrite or repeat all
backend suites once relevant checks pass. Missing target access limits the
verification claim; it is not evidence that the UI works there.

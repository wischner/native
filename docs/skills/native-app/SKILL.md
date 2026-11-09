---
name: native-app
description: Create or extend C++ applications using the Native UI library, including control lifetime, program entry, layouts, and CMake integration. Use for application code rather than implementing toolkit adapters.
---

# Build a Native application

Locate the Native source checkout used by the consumer. Read its `AGENTS.md`
and the consuming project's instructions before changing files. Links below
resolve from this skill in the checkout; if installed elsewhere, resolve the
same `docs/` paths against that checkout.

1. Identify the requested behavior, existing application structure, and target
   backends. Reuse current build and deployment settings. Ask only for missing
   information that prevents a correct implementation.
2. Read [the first application](../../manuals/programming-native/01-FIRST-APPLICATION.md)
   and [control lifecycle](../../manuals/programming-native/04-BUTTONS.md)
   when creating a new app. Native supplies the process entry point;
   implement `int program(int, char **)` and keep a named `app_wnd` alive
   through `app::run(window)`.
3. Choose concrete controls using the
   [control index](../../manuals/PROGRAMMING-NATIVE.md#find-every-control).
   Read the relevant complete example and current `include/native/*.h`
   declarations before composing it. Prefer public controls to drawing a
   replacement widget. Backend native/hybrid choices belong inside Native.
4. Keep application state and controls in an owning C++ window/controller.
   Ordinary child controls need `set_parent`, `create`, and `show` after
   their parent is created. Containers such as tabs, splits, accordions,
   and settings sidebars create their borrowed content; do not create that
   content twice. Declare borrowed pages, panes, models, and images before
   their borrowers. A panel's own children still need explicit creation.
5. Configure silent setters before creation where convenient. Connect user
   signals to application state; update dependent UI explicitly after
   programmatic changes. Resize against `get_client_bounds()` or install a
   layout so non-client toolbars, rulers, and status bars retain their space.
6. Link the CMake target `native`; include `<native.h>` and standard C++
   headers, with no toolkit types or private `lib/native` headers. Follow
   [consumer integration](../../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#native-as-a-cmake-dependency)
   and use [native-build](../native-build/SKILL.md) for the selected target's
   SDK, dependency checks and distribution payload. SDL2 stock fonts need
   SDL2_ttf and Fontconfig detected at build time plus runtime font data.
   macOS uses the documented remote AppleClang/Apple SDK workflow.
   A matching tag-release SDK also provides `Native::Native` through
   `find_package(Native CONFIG REQUIRED)`; pass its root in `CMAKE_PREFIX_PATH`.
7. Exercise the requested interactions and resizing/close lifecycle. Report
   build, runtime, and visual evidence separately. Refresh affected project
   docs and use the [handoff template](../../ai/REQUEST-TEMPLATE.md).

Keep generated software in the user's application. Within the Native
repository, Vision remains the single demonstration application; educational
programs belong in the Programming Native book, not a new examples source tree.
No step grants permission to publish, deploy to unrelated machines, or send
messages outside the requested task.

## Input and continuously refreshed views

For emulators and other held-key clients, check
`app::get_physical_keyboard_supported()` before promising keyboard operation.
GEM's current AES transport does not provide the physical press/release
contract; do not synthesize releases or pretend text input supplies it.
Connect key/reset handlers to the actual focused canvas as appropriate, and
release application-held input on focus loss. Keep menus and their commands
owned by the corresponding main/modeless window.

Use `shader_status::active` after painting to report an active effect. Keep
checkmarks/settings consistent with `on_shader_error` and explicit fallback.
A package that validates can still fail on the destination graphics driver.

Fix reproducible adapter defects in Native and regression-test them there.
Do not copy and patch Native implementation files in consumer CMake or add
private dependency defines. Record the dependency revision and whether local
changes were used; a clean upstream revision alone does not identify a dirty
checkout's build.

If the application disables CRT after an error, also clear the effect. Native
permits retry after a new source/property update, so leaving a failed effect
installed in a continuously updated emulator can retry it on every frame.

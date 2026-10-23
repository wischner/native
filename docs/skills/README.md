# Skills for writing Native applications

These are reusable skills for consuming the library through `<native.h>`.
They describe tasks, not backend implementations. Start from the
[AI generation guide](../ai/README.md) to choose context and an agent role.

| Skill | Activate for |
| --- | --- |
| [native-app](native-app/SKILL.md) | Creating or extending a C++ Native application, lifecycle and build integration |
| [native-controls](native-controls/SKILL.md) | Choosing and composing controls, settings, pickers, commands, tables, and editors |
| [native-verify](native-verify/SKILL.md) | Building, reviewing, and checking a generated application's observable behavior |

Each folder contains a `SKILL.md` with name/description frontmatter and a
focused workflow. Load it through your agent runner or read it directly.
These files do not install themselves or change the runner's configuration.
The book links resolve relative to this checkout. If your runner copies a
skill into its own discovery directory, supply the Native checkout path as
context and resolve `docs/` and `include/` there. Keep that checkout/version
consistent with the library being compiled. A copied skill is not a substitute
for the referenced headers and examples.

All three skills use the same [control index](../manuals/PROGRAMMING-NATIVE.md#find-every-control).
They deliberately avoid copied method catalogs that would drift from the API.

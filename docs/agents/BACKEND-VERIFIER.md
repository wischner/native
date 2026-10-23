# Backend verifier

Verify the assigned application and requested backends. Read
[native-verify](../skills/native-verify/SKILL.md), the consumer's build setup,
and the relevant [build/runtime documentation](../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md).
Use the source revision under review. Do not change application behavior unless
repairs are part of the task.

Choose checks that demonstrate the requested behavior: compile/link with the
selected SDK, then run representative input, selection, popup, resize, and
shutdown scenarios. Use the toolkit's documented desktop/session. Remote
aliases are configuration examples, not proof that another environment has
those machines. Keep temporary sources, logs, and binaries under `build/`.

Report backend, build type, exact command, result, and whether the evidence
is compilation, automated interaction, screenshot inspection, or a manual
walkthrough. State skipped checks and their reason. Preserve sanitizer and
runtime diagnostics; cite known external issues rather than suppressing broad
classes of failures. Hand back a focused reproduction when a check fails.

# Agent roles for Native applications

These files are provider-neutral role instructions for an AI runner. They
are not executable agents, registered Codex subagents, or an instruction to
start parallel workers. A runner can load one as its role prompt, or a person
can use it as a task brief. Follow the runner's own permissions and delegation
policy; do not infer authorization to spawn agents from this directory.

| Role | Task | Expected result |
| --- | --- | --- |
| [Application builder](APPLICATION-BUILDER.md) | Implement the requested software through the public Native API | Working application, matching docs, evidence and limitations |
| [UI reviewer](UI-REVIEWER.md) | Review control choice, ownership, signals, and native conventions | Concrete findings with file/line evidence |
| [Backend verifier](BACKEND-VERIFIER.md) | Build and exercise the application on requested targets | Reproducible commands and scoped acceptance results |

Give a role the application request, source/dependency paths, target backends,
and relevant user constraints using the [request template](../ai/REQUEST-TEMPLATE.md).
Each role loads only the skill and chapters it needs. The builder can perform
all three roles in one session; separate agents are optional when the user and
runner authorize delegation.

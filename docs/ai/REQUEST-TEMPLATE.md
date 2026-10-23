# Application request and handoff

Copy the relevant fields into a task brief. Leave optional fields out when
irrelevant; do not invent requirements to complete a form.

## Request

```text
Application/source path:
Native checkout and revision:
User workflow and visible result:
Target backends and desktops:
Existing build/deploy command:
Controls or existing window to extend:
Data model, persistence, and validation rules:
Assets and licensing constraints:
Required interactions and acceptance examples:
Scope allowed for changes (application/library/build/docs):
Environment limits or unavailable targets:
```

For example: “Add a booking preferences window to the existing application.
Use searchable General and Appointment pages, exclude the supplied holiday
list and weekends, allow 15-minute appointments between 08:00 and 18:00, and
retain values while switching pages. Target the existing GTK Wayland build.
Use Chapter 21; do not add a toolkit dependency.” The holiday list and
persistence policy are application inputs, not facts the generator should guess.

## Implementation handoff

```text
Implemented behavior and entry point:
Changed application files and documentation:
Controls/examples reused:
Ownership and lifetime choices that matter:
Build commands and results:
Automated interactions actually exercised:
Visual/manual checks actually performed:
Skipped checks and remaining limitations:
How to reproduce any failure:
```

Keep compile, link, runtime, and visual results separate. A screenshot can show
alignment; it cannot prove keyboard behavior or shutdown. A cross-build can
prove compilation; it cannot prove execution on Windows, Haiku, or macOS.

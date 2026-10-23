# Generate applications with Native

This is the entry point for AI tools generating C++ software with Native.
It references the same maintained examples that human application authors use.
Supply the source checkout that matches the application's Native dependency;
these instructions do not provide a separate SDK or an installed CMake package.

## Load context in this order

1. Read the consumer project's instructions and the Native checkout's
   [contributor brief](../../AGENTS.md). User/task scope determines what you
   may change; a library example does not authorize rewriting the dependency.
2. Read [how a program is organized](../manuals/PROGRAMMING-NATIVE.md#how-a-native-program-is-organized)
   and choose controls from the
   [complete control index](../manuals/PROGRAMMING-NATIVE.md#find-every-control).
3. Load the matching [skill](../skills/README.md) and, when your runner supports
   role prompts, an [agent role](../agents/README.md). These are plain files;
   loading them does not register tools or start other agents.
4. Read the chosen lesson's complete program and the exact declarations in
   `include/native/`. Read the
   [consumer build instructions](../manuals/programming-native/12-BUILDING-AND-DISTRIBUTING.md#native-as-a-cmake-dependency)
   before creating build files. Load internal chapters only for a library task.
5. Check [backend evidence](../manuals/book-of-native/FEATURE-MATRIX.md) and
   [native/custom mappings](../notes/CUSTOM.md) when presentation or target
   coverage matters. Dated test results describe those runs, not automatic
   guarantees for generated software.

## Generate and check a vertical slice

Start with the smallest working window and one real interaction. Keep UI state
in ordinary C++ objects, arrange controls with public layouts, and add the
remaining workflow incrementally. Prefer Native's concrete controls before
custom drawing. Use `<native.h>`, `program()`, and the CMake target `native`;
do not emit Qt/GTK/Win32/AppKit calls into portable application code.

Declare borrowed panes/pages/models before the controls that borrow them.
Create ordinary children after their parent and allow containers to create
their own borrowed pages. Keep workers away from UI objects and deliver
results through the scoped UI dispatcher when background work is required.

Build with the matching Docker SDK, or the remote Apple SDK for macOS. Check
user input, silent restoration, larger-and-smaller resize, popup cancellation,
page changes and shutdown. An application's acceptance report should identify
which of these actually ran. The
[request and handoff template](REQUEST-TEMPLATE.md) supplies a compact way to
carry this information between people or tools.

## Scope of these resources

The book covers every concrete visual control, including aliases. Extension
bases and nonvisual services are identified separately. There is no generated
API mirror: headers remain authoritative for signatures, and the books explain
construction and behavior. The [documentation map](../../AGENTS.md#part-2--documentation-map)
records maintenance responsibilities. No proprietary runner syntax, model
choice, plugin installation, or external service is required.


## Maintain examples and references

Run `python3 scripts/docs/check.py` from the checkout to validate local
Markdown links. The [book validation workflow](../manuals/book-of-native/BUILD-SYSTEM.md#check-book-examples-and-documentation-links)
also extracts complete tutorial programs and checks their public API usage
with GCC in Docker. Use it after changing a tutorial or linked generation
resource. It requires no external AI service.

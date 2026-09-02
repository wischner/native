# Windows Wine Runtime Note

This note records the current runtime constraint for Windows binaries in the
Linux development workflow.

## Current behavior

Windows binaries built through `docker-win` are MinGW binaries.

When those binaries are run through Wine, the following MinGW runtime DLLs are
currently needed next to the `.exe`:

- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

These DLLs are not part of the standard Wine-provided Windows DLL set.
They come from the MinGW runtime.

## Current workflow

The VS Code tasks prepare the runtime by copying those DLLs from the
`wischner/gcc-x86_64-windows-mingw-w64` Docker image into the example output
directories before launching Wine.

## Open issue

The current workflow works, but it is operational rather than elegant.
Possible future improvements include:

- static linking of the MinGW C++ runtime where appropriate
- a shared runtime staging directory with `WINEPATH`
- packaging Windows example outputs with their required MinGW runtime files

These are not active user workflow features yet, so they stay in notes rather
than in the book.

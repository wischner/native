# Patterns: Docking Workspaces

Docking is layered over existing portable contracts. It does not introduce a
backend widget family.

## Ownership boundary

`dock_host` borrows an `app_wnd` owner and a `wnd` surface. It installs an
owned `dock_layout_manager` on the surface and borrows each registered pane's
content control. The application retains all C++ window objects. Internal
floating shells are `modeless_wnd` objects owned by the host and associated
with the supplied application-window owner.

This division is important. The layout manager owns only the split/tab state
and calculates child rectangles. It does not interpret pointer events, create
windows, paint tabs, or serialize backend handles. The host performs those
coordination tasks and rejects use after an application replaces its installed
layout.

## Native control transitions

There is no portable hidden-child operation on `wnd`. The host therefore keeps
one native resource for the active pane in every docked tab node. An inactive
or hidden pane retains its complete portable control model but has no native
resource. Activation recreates from that cache.

An auto-hidden pane follows the same transition rule. Its stable pane/edge
pair remains in portable state while collapsed. Revealing it creates the
content on the host in a temporary overlay; collapsing destroys that resource,
and pinning inserts the pane back into the split/tab tree.

Moving between top-level parents follows a strict order:

1. Destroy the content's native resource.
2. Change its non-owning portable parent.
3. Apply the resolved content bounds.
4. Create and show the resource under the new created parent.

That path works for native widgets and themed emulations alike and avoids
depending on whether a toolkit can reparent an already-live widget.

## Rendering and input

Tabs and splitters occupy exposed parts of the surface, outside the active
child rectangles. A singleton tab node still reserves and paints its titled
strip: every docked pane therefore has a visible drag handle. The host listens
to the surface's portable paint and mouse
signals. It composes existing semantic theme primitives—header/content
surfaces, selection, focus, separators, palette, stock control font, and
backend metrics—so OPEN LOOK, WINGs, Motif, Windows, Haiku, macOS, SDL2,
Athena, and GEM all retain their own appearance resources.

The splitter has its own theme extent and is a continuous resize control, not
just a painted separator. Its hover/pressed grip follows the active theme;
dragging clamps against both recursive branch minimums and commits one user
event on release. Collapsed left and right auto-hide tabs rasterize the native
control-font title and rotate it 90 degrees, inward-facing on each edge. Top
and bottom titles stay horizontal, and every orientation ellipsizes to fit.
Revealed auto-hide content lives in a raised child surface rather than in the
host's backing pixels. This preserves stacking over native controls; collapse
reparents the content through its ordinary lifecycle and leaves the saved
dock tree and control model untouched. Pane-child pointer events also dismiss
the reveal when the pointer crosses directly into another native control.

A drag hit-tests the latest geometry produced by the layout manager. The
preview contains only stable pane/node identity and Native rectangles.
Floating drags follow the backend-reported screen pointer position, so moving
the shell cannot feed stale client coordinates back into the next motion and
an undocked pane can always return to its host. Once a
drag crosses its movement threshold, one five-part docking compass appears at
the center of the host. Its left, right, top, and bottom arrows select splits;
its center target selects tab docking. Each guide occupies a transient raised
native child surface that remains allocated for the drag, so the compass stays
above native pane widgets without repainting them as the pointer moves. A
second compact raised surface states the operation and target pane in words.
A peerless/custom surface retains the host-painted preview fallback. Every guide
uses the active backend's button painter and palette. A release mutates the
portable tree, reconciles native parents, then emits one user event.
Programmatic calls use the same mutation and reconciliation path without
emission.

Pin and close marks use semantic caption-button theme primitives. The host's
protected virtual stages own the actual default painting for tabs, splitters,
surfaces, edge tabs, revealed captions, previews, destination labels, and
individual docking-guide targets. Derived hosts can replace a stage or call
base and extend it; painting
is not performed before a stage is dispatched.

An OPEN LOOK floating pane suppresses the redundant outer frame label and
uses one centered, compact rounded OPEN LOOK button caption as both its title and
drag handle.
OPEN LOOK pin/unpin states use the stock OLGX menu pushpin rather than a
portable approximation, and custom overlay state changes copy their backing
Pixmap without clearing the live XView Panel first.

## Persistence boundary

`dock_layout_state` is the authoritative typed snapshot. The Native Dock v2
text codec is deterministic and intentionally stores only structural state,
including auto-hidden pane/edge pairs. The parser retains version 1
compatibility and supplies an empty auto-hide collection for that legacy form.
Registration supplies current titles, capability flags, minimum dimensions,
and content pointers after every application start.

Restore filters unknown IDs for compatibility with removed panes, docks IDs
missing from an older snapshot for forward compatibility, rejects duplicate
known IDs, and normalizes empty branches. Parsing and validation complete
before the live manager adopts the candidate state.

This design expands Architecture Section 16 and reuses the window, ownership,
painting, theme, and signal rules from Sections 3 through 7.

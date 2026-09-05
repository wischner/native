# Patterns: split views and tabs

GEMix keeps drag capture in the root peer until release, including movement
outside the window, and clears capture when its borrowed splitter is destroyed.
Its tabs use white paper with black labels in every placement. Selected tabs
overlap the framed page, while a list used as page content does not add another
enclosure across that join. Rotated labels use the GEM stock bitmap font and
reserve enough padding to avoid ellipsizing otherwise fitting labels.
List panes repaint their selection independently of divider movement. The
flat left-tab page edge remains visible beside inactive tabs; the selected
tab alone keeps an open join to its content.

Use a split view for two simultaneously visible work areas whose boundary the
user may resize. Use a tab view when only one page should be live at a time.
They compose: either split pane may itself contain tabs, and a tab page may
contain a split view.

The portable model is deliberately small. A split has two borrowed panes, an
orientation, a ratio, minimum extents, and one change signal. A tab view has
borrowed pages, labels, enabled state, four-edge placement, selection,
an optional page frame, and one selection signal.
Neither control changes top-level ownership or creates floating windows.

Tabs may be appended through either `add_item(title, page)` or the common
builder syntax `tabs << native::tab_page(title, page)`. Both paths borrow the
same uncreated page object; `operator<<` is append-only sugar and never changes
placement or selection.

Top placement is the compatibility default. Bottom placement keeps the page
above the labels and gives the tabs a genuinely downward-facing free edge.
Left and right placement reserve a vertical edge and rotate labels in the
corresponding reading direction. Placement changes are silent and preserve
the selected borrowed page across creation and resizing.

The framed page is the compatibility default. Strip-only mode removes that
box, makes the page flush with the cross-axis edges, and retains one
full-span separator between the page and tabs. Switching the frame at run
time is silent and retains the item model and selected borrowed page.
The page edge or strip-only separator paints before the tabs, and the selected
tab overlaps it by one device pixel. Top, bottom, left, and right placements
therefore join the page without a one-pixel gap. Rasterized bottom and right
tabs retain one adjoining page line so the selected join matches native top
and painted left tabs. Window Maker inactive bottom/right tabs replace the page
colored closure beneath their joining edge with the surrounding surface,
leaving the page highlight alone and using a shadowed free edge rather than
two light lines.

Native adapters retain toolkit behavior and accessibility wherever the
toolkit provides the widget: Haiku `BSplitView`/`BTabView`, Motif
`XmPanedWindow`/`XmNotebook`, Window Maker `WMSplitView`/`WMTabView`, and
AppKit `NSSplitView`/`NSTabView`.

Haiku, AppKit, Windows, and Motif use native placement where the toolkit also
meets Native's directional-label contract. Themed hosts use the shared
portable geometry and renderer. WINGs exposes only top tabs, so Window Maker
keeps `WMTabView` for framed top placement and uses a WINGs-matched Native
renderer for bottom, left, right, and strip-only placement.

Haiku uses `BTabView` on all four edges. Its own rotated-label renderer draws
side-tab text directly, and label-sized widths keep short vertical strips
usable. Split pane hosts and painted lists request full repaint on resize so
the previous trailing borders are erased as the divider moves.

Window Maker also keeps `WMSplitView` fully native. Size notifications from
its pane views update the portable ratio after the current WINGs dispatch,
then resize both borrowed controls to the exact native dimensions of their
respective pane frames. Using the granted sizes rather than independently
recomputing them keeps every child border inside its pane.
Deferring one captured notification batch at a time prevents native constraint
updates and portable child layout from feeding back inside one event turn.

The portable split renderer dispatches `draw_splitter_background()` and then
`draw_splitter_grip()`. Each protected virtual has a complete default for only
its stage, so an application can replace the divider face or grip without
reimplementing the other.
The default background is the borderless panel/window surface, so exposed
divider space belongs visually to the host; only the compact center grip marks
the draggable axis.

On an emulated backend such as SDL, the split host is painted before its pane
children. Divider input is translated from root coordinates, captures the
active split until button release, and updates the panes continuously during
motion. Horizontal and vertical orientations select their matching system
resize cursor automatically.

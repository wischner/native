# Patterns: split views and tabs

Use a split view for two simultaneously visible work areas whose boundary the
user may resize. Use a tab view when only one page should be live at a time.
They compose: either split pane may itself contain tabs, and a tab page may
contain a split view.

The portable model is deliberately small. A split has two borrowed panes, an
orientation, a ratio, minimum extents, and one change signal. A tab view has
borrowed pages, labels, enabled state, four-edge placement, selection,
and one selection signal.
Neither control changes top-level ownership or creates floating windows.

Top placement is the compatibility default. Bottom placement keeps the page
above the labels and gives the tabs a genuinely downward-facing free edge.
Left and right placement reserve a vertical edge and rotate labels in the
corresponding reading direction. Placement changes are silent and preserve
the selected borrowed page across creation and resizing.

Native adapters retain toolkit behavior and accessibility wherever the
toolkit provides the widget: Haiku `BSplitView`/`BTabView`, Motif
`XmPanedWindow`/`XmNotebook`, Window Maker `WMSplitView`/`WMTabView`, and
AppKit `NSSplitView`/`NSTabView`.

Haiku, AppKit, Windows, and Motif use native placement where the toolkit also
meets Native's directional-label contract. Themed hosts use the shared
portable geometry and renderer. WINGs exposes only top tabs, so Window Maker
keeps `WMTabView` for top placement and uses a WINGs-matched Native renderer
for bottom, left, and right placement.

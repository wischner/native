# Patterns: split views and tabs

Use a split view for two simultaneously visible work areas whose boundary the
user may resize. Use a tab view when only one page should be live at a time.
They compose: either split pane may itself contain tabs, and a tab page may
contain a split view.

The portable model is deliberately small. A split has two borrowed panes, an
orientation, a ratio, minimum extents, and one change signal. A tab view has
borrowed pages, labels, enabled state, selection, and one selection signal.
Neither control changes top-level ownership or creates floating windows.

Native adapters retain toolkit behavior and accessibility wherever the
toolkit provides the widget: Haiku `BSplitView`/`BTabView`, Motif
`XmPanedWindow`/`XmNotebook`, Window Maker `WMSplitView`/`WMTabView`, and
AppKit `NSSplitView`/`NSTabView`.

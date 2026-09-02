# Chapter 16: Docking Workspaces

Docking combines two existing Native facilities. A
`dock_layout_manager` calculates split and tab geometry, while a `dock_host`
coordinates pointer input, theme painting, child-control lifecycle, floating
`modeless_wnd` shells, and persistence. Pane contents remain normal native or
emulated Native controls.

## Define the workspace

Declare pane controls before the host. C++ destroys members in reverse order,
so this makes the borrowing host detach its panes before their C++ objects are
destroyed.

```cpp
class workspace final : public native::app_wnd
{
public:
    workspace()
        : native::app_wnd("Workspace", 80, 60, 960, 640)
        , _project({{"Project", nullptr, 1}}, 0, 0, 200, 400)
        , _editor("int main() {}\n")
        , _output("Ready\n", native::text_edit_mode::multi_line)
        , _docking(*this) {
        _output.set_read_only(true);
        on_wnd_create.connect(this, &workspace::on_create);
        _docking.on_change.connect(this, &workspace::on_dock_change);
    }

private:
    native::tree_view _project;
    native::code_edit _editor;
    native::text_edit _output;
    native::dock_host _docking;

    bool on_create() {
        if (_docking.get_layout().get_pane(1)) {
            _docking.set_layout_state(_docking.get_layout_state());
            return true;
        }

        native::dock_pane editor(2, "Editor", _editor);
        editor.closable = false;
        editor.minimum_size = {320, 200};

        native::dock_pane project(1, "Project", _project);
        project.minimum_size = {160, 160};

        native::dock_pane output(3, "Output", _output);
        output.minimum_size = {260, 100};

        _docking.add_pane(editor)
            .add_pane(project, native::dock_position::left, 2)
            .add_pane(output, native::dock_position::bottom, 2);
        return true;
    }

    bool on_dock_change(native::dock_event event) {
        // This runs for accepted user actions, not calls made above.
        return event.pane != 0;
    }
};
```

Register panes after the surface has been created. `add_pane()` assigns the
content parent, creates active native controls, and applies the first layout.
The host owns the installed layout manager but borrows every pane and the
surface.

Use a docking surface as a dedicated workspace. Every docked region reserves
an exposed, draggable titled tab strip, including a region that contains only
one pane. Its tab strips, splitters, and drop previews are painted by the host;
pane controls paint and
handle input in their own native child regions.

## Split and tab placement

The position is relative to a docked stable pane ID:

```cpp
_docking.dock(1, native::dock_position::left, 2);
_docking.dock(3, native::dock_position::center, 2);
_docking.activate_pane(3);
_docking.move_tab(3, 2);
```

Left and right create a horizontal child arrangement. Top and bottom create a
vertical child arrangement. Center adds the pane to the target's tab node.
Passing `0` as the relative ID selects the default root target.

`set_split_ratio(node_id, ratio)` accepts a proportional value. The layout
clamps it to valid limits and then further respects the recursive minimum size
of both split branches.

## Floating, pinning, closing, and showing

Floating bounds use screen coordinates:

```cpp
_docking.float_pane(1, native::rect(120, 100, 360, 480));
_docking.auto_hide_pane(1, native::dock_position::left);
_docking.reveal_auto_hide(1);
_docking.collapse_auto_hide();
_docking.pin_pane(1, native::dock_position::left, 2);
_docking.close_pane(3);
_docking.show_pane(3, native::dock_position::bottom, 2);
```

Floating destroys the pane's current native child resource, reparents its
portable object to an owned `modeless_wnd`, recreates it, and preserves the
control's portable model. Docking performs the reverse transition. Closing a
pane moves it to hidden state; it does not unregister or delete its content.
`remove_pane()` is the permanent unregister operation.

A pane with `floatable == false` rejects floating. A pane with
`pinnable == false` has no pin mark and cannot enter auto-hide state. A pane
with `closable == false` has no client close mark, and closing its native
floating shell returns it to the dock.

Auto-hide is a fourth pane location, separate from hidden. It stores the left,
right, top, or bottom edge and presents a collapsed edge tab. Revealing that
tab temporarily creates the content in an overlay with a compact caption.
Left and right tabs show the complete title rotated 90 degrees, with letter
tops facing inward; top and bottom titles remain horizontal. Titles use the
backend's control font and ellipsize when the strip is short.
The temporary reveal is a raised native child surface, so its caption and
content stay above native dock siblings instead of being painted underneath
them. Leaving it through another native pane collapses it and restores that
pane's original dock geometry and portable state.
Pinning restores it to the dock tree; moving outside the overlay collapses it
without destroying the saved edge state. Programmatic reveal and collapse are
signal-silent like the other direct host operations.

## Pointer interaction

Users can:

- click a tab to activate it;
- drag a tab onto a compass arrow to create a split;
- drag onto the compass center to make or reorder tabs;
- drag outside the workspace to create a floating window;
- drag the floating window's single client caption back over the workspace;
- resize a split continuously from its wider themed splitter boundary;
- pin a docked pane to the nearest edge or unpin a revealed pane; and
- close a pane from its tab close mark.

The host emits one `on_change` event after each accepted user action. Direct
API calls are deliberately signal-silent.

Splitter width is a dedicated theme metric, not the decorative one-pixel
separator width. Hovering or pressing reveals its compact native grip. A drag
is clamped by both branches' recursive minimum sizes and emits one
`split_resized` event when released.

Floating movement and redocking use screen pointer coordinates supplied by
the native backend rather than coordinates relative to the shell while it is
moving. In OPEN LOOK the floating shell hides its redundant frame label: the
one centered, compact rounded OPEN LOOK button caption is the title and docking
drag handle. Its pin command uses the standard OPEN LOOK menu pushpin glyph.

One five-part docking compass appears at the host client center after pointer
movement starts a drag. Hovering a guide selects its destination. Raised
transient child surfaces keep the compass above native pane controls and stay
allocated for the drag, so pointer movement does not repaint the controls
underneath. A compact raised label also names the operation and target pane,
for example `LEFT OF: Editor` or `TAB WITH: Output`, so the selected compass
target is unambiguous. Peerless/custom surfaces retain the host-painted
preview fallback.
The arrows and center target are native-themed button parts
rather than application artwork, so their borders, interaction state, glyph
color, and pressed treatment follow the active backend.

## Save and restore

Use the typed state when the application already has its own settings format:

```cpp
native::dock_layout_state state = _docking.get_layout_state();
_docking.set_layout_state(state);
```

Use the versioned text form for direct storage:

```cpp
std::string saved = _docking.serialize_layout();
_docking.restore_layout(saved);
```

The Native Dock v2 value stores the tree, ratios, tab order, active tabs,
floating bounds, hidden pane IDs, and auto-hidden pane/edge pairs. Titles and
content are registered by the application and are not serialized. The parser
also accepts Native Dock v1 values, which have no auto-hidden entries. Unknown
saved IDs are ignored, while a newly registered ID absent from an older layout
is docked by default.

Malformed text or typed state throws `std::invalid_argument` before live state
is replaced. Store the text in the application's normal preferences service;
Native does not choose a settings file or registry location.

Open **Window -> Docking workspace** in Vision to exercise the complete
workspace. Its **Panes** menu also demonstrates programmatic float/dock,
auto-hide/pin, close/show, and in-memory save/restore.

## Deriving a docking host

`dock_host` is inheritable. User actions enter through
`on_native_change(const dock_event &)`, whose base implementation emits
`on_change`. Override it and call base when the normal signal should remain.
Protected virtual painting stages cover surfaces, tabs, splitters, collapsed
edge tabs, revealed compact captions, drop previews, the compact destination
label, and individual docking guide targets. Their base methods do the default
themed drawing, so an override
can call base and then add a badge or can replace the part completely. There is
no second owner-draw pass after the virtual method returns.

For automated desktop smoke tests, `vision --docking` opens that workspace
immediately after the main gallery enters its normal event loop.

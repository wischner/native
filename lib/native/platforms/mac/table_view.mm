//
// Implements table_view with a data-source-driven NSTableView inside
// NSScrollView, preserving AppKit headers, reuse, grids, and stripes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include <native.h>

#include "../../control_render_access.h"
#include "../../table_visible_rows.h"
#include "globals.h"
#include "native_cells.h"

namespace
{
    // Dynamic system-derived colors keep contrast in light and dark mode.
    // NSTableView/NSTableRowView still perform all grid/background painting.
    NSColor *table_tint(CGFloat amount) {
        return [NSColor colorWithName:nil dynamicProvider:^NSColor *(NSAppearance *appearance) {
            __block NSColor *color = nil;
            [appearance performAsCurrentDrawingAppearance:^{
                color = [[NSColor textBackgroundColor]
                    blendedColorWithFraction:amount ofColor:[NSColor labelColor]];
            }];
            return color;
        }];
    }

    NSString *native_string(const std::string &value) {
        NSString *result = [NSString stringWithUTF8String:value.c_str()];
        return result ? result : @"";
    }

    mac::mac_table_view &binding_for(native::table_view &table) {
        auto *binding = mac::table_view_bindings.object_from_handle(
            &table);
        if (!binding || !binding->table)
            throw std::runtime_error(
                "macOS: missing table_view binding.");
        return *binding;
    }

    std::optional<native::table_group> group_by_id(
        native::table_model &model,
        native::table_group_id id) {
        for (std::size_t index = 0;
             index < model.group_count(); ++index) {
            native::table_group group = model.group(index);
            if (group.id == id)
                return group;
        }
        return std::nullopt;
    }

    native::table_column_id column_id(NSTableColumn *column) {
        return static_cast<native::table_column_id>(
            [[column identifier] integerValue]);
    }
} // namespace

@interface native_table_widget : NSTableView {
@public
    void *_owner;
}
@end

@implementation native_table_widget
- (void)keyDown:(NSEvent *)event {
    auto *owner = static_cast<native::table_view *>(_owner);
    if (owner && ([event keyCode] == 36 || [event keyCode] == 76)) {
        const auto selected = owner->get_selected_rows();
        if (!selected.empty())
            owner->on_native_activate(selected.back());
        return;
    }
    [super keyDown:event];
}
@end

@interface native_table_cell_view : native_content_cell {
@public
    void *_owner;
    NSInteger _row;
    native::table_column_id _column;
}
@end

@implementation native_table_cell_view
- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::table_view *>(_owner);
    if (!owner || !owner->get_created() || !owner->get_model() ||
        typeid(*owner) == typeid(native::table_view) ||
        _row < 0 || static_cast<std::size_t>(_row) >=
                        owner->get_display_row_count()) {
        [super drawRect:dirty];
        return;
    }
    const native::table_display_row display = owner->get_display_row(
        static_cast<std::size_t>(_row));
    if (display.group)
        return;
    const native::table_column_id target_column = _column;
    const auto found = std::find_if(
        owner->get_columns().begin(),
        owner->get_columns().end(),
        [target_column](const native::table_column &column) {
            return column.id == target_column;
        });
    if (found == owner->get_columns().end())
        return;
    native::rect bounds(
        0,
        0,
        static_cast<native::dim>(
            std::max<CGFloat>(0, [self bounds].size.width)),
        static_cast<native::dim>(
            std::max<CGFloat>(0, [self bounds].size.height)));
    native::gpx &graphics = owner->get_gpx();
    graphics.set_clip(bounds);
    auto appearance = native::theme::create(graphics);
    native::theme::state state;
    native::table_model *model = owner->get_model();
    const native::table_row_id row = model->row_id(display.model_row);
    const auto &selection = owner->get_selected_rows();
    state.selected = std::find(selection.begin(), selection.end(), row) !=
                     selection.end();
    state.focused = [[self window] firstResponder] == [self superview];
    native::detail::control_render_access::draw_table_cell(
        *owner,
        graphics,
        *appearance,
        row,
        display.model_row,
        *found,
        model->cell(display.model_row, _column),
        bounds,
        state);
}
@end

@interface native_table_group_button : NSButton {
@public
    void *_owner;
    native::table_group_id _group;
}
@end

@implementation native_table_group_button
- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::table_view *>(_owner);
    native::table_model *model = owner ? owner->get_model() : nullptr;
    const auto group = model ? group_by_id(*model, _group)
                             : std::nullopt;
    if (!owner || !owner->get_created() || !group ||
        typeid(*owner) == typeid(native::table_view)) {
        [super drawRect:dirty];
        return;
    }
    native::gpx &graphics = owner->get_gpx();
    auto appearance = native::theme::create(graphics);
    const NSRect frame = [self bounds];
    const native::rect bounds(
        0,
        0,
        static_cast<native::dim>(
            std::max<CGFloat>(0, frame.size.width)),
        static_cast<native::dim>(
            std::max<CGFloat>(0, frame.size.height)));
    graphics.set_clip(native::rect(
        static_cast<native::coord>(dirty.origin.x),
        static_cast<native::coord>(dirty.origin.y),
        static_cast<native::dim>(
            std::max<CGFloat>(0, dirty.size.width)),
        static_cast<native::dim>(
            std::max<CGFloat>(0, dirty.size.height))));
    native::theme::state state;
    state.disabled = !group->collapsible;
    state.pressed = [self isHighlighted];
    native::detail::control_render_access::draw_table_group(
        *owner, graphics, *appearance, *group, bounds, state);
}
@end

@interface native_table_header_cell : NSTableHeaderCell {
@public
    void *_owner;
    native::table_column_id _column;
}
@end

@implementation native_table_header_cell
- (void)drawWithFrame:(NSRect)frame inView:(NSView *)view {
    auto *owner = static_cast<native::table_view *>(_owner);
    if (!owner || !owner->get_created() ||
        typeid(*owner) == typeid(native::table_view)) {
        [super drawWithFrame:frame inView:view];
        return;
    }
    const native::table_column_id target = _column;
    const auto column = std::find_if(
        owner->get_columns().begin(),
        owner->get_columns().end(),
        [target](const native::table_column &candidate) {
            return candidate.id == target;
        });
    if (column == owner->get_columns().end()) {
        [super drawWithFrame:frame inView:view];
        return;
    }
    native::gpx &graphics = owner->get_gpx();
    const native::rect bounds(
        static_cast<native::coord>(frame.origin.x),
        static_cast<native::coord>(frame.origin.y),
        static_cast<native::dim>(
            std::max<CGFloat>(0, frame.size.width)),
        static_cast<native::dim>(
            std::max<CGFloat>(0, frame.size.height)));
    graphics.set_clip(bounds);
    auto appearance = native::theme::create(graphics);
    native::theme::state state;
    state.pressed = [self isHighlighted];
    native::detail::control_render_access::draw_table_header(
        *owner, graphics, *appearance, *column, bounds, state);
}
@end

@interface native_table_adapter
    : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
@public
    void *_owner;
}
- (void)toggleGroup:(id)sender;
- (void)doubleClick:(id)sender;
@end

@implementation native_table_adapter
- (void)tableView:(NSTableView *)table
    didAddRowView:(NSTableRowView *)view forRow:(NSInteger)row {
    (void)table;
    auto *owner = static_cast<native::table_view *>(_owner);
    if (!owner || row < 0 ||
        static_cast<std::size_t>(row) >= owner->get_display_row_count()) return;
    const auto display = owner->get_display_row(static_cast<std::size_t>(row));
    if (!display.group) {
        [view setBackgroundColor:owner->get_alternating_rows() && (display.model_row % 2)
            ? table_tint(0.09) : [NSColor textBackgroundColor]];
    }
}
- (void)clipBoundsChanged:(NSNotification *)note {
    auto *owner = static_cast<native::table_view *>(_owner);
    auto *state = owner
        ? mac::table_view_bindings.object_from_handle(owner) : nullptr;
    if (!state || state->suppress || !owner->get_created()) return;
    const NSPoint origin = [[note object] bounds].origin;
    const NSInteger row = [state->table rowAtPoint:origin];
    if (row < 0) return;
    // Cache native scrolling without feeding rounded rows back to AppKit.
    state->suppress = true;
    owner->on_native_scroll(static_cast<std::size_t>(row),
        static_cast<int>(origin.x));
    state->suppress = false;
}
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table {
    (void)table;
    auto *owner = static_cast<native::table_view *>(_owner);
    return owner
        ? static_cast<NSInteger>(owner->get_display_row_count())
        : 0;
}

- (BOOL)tableView:(NSTableView *)table isGroupRow:(NSInteger)row {
    (void)table;
    auto *owner = static_cast<native::table_view *>(_owner);
    return owner && row >= 0 &&
           static_cast<std::size_t>(row) <
               owner->get_display_row_count() &&
           owner->get_display_row(static_cast<std::size_t>(row)).group;
}

- (BOOL)tableView:(NSTableView *)table
    shouldSelectRow:(NSInteger)row {
    return ![self tableView:table isGroupRow:row];
}

- (NSView *)tableView:(NSTableView *)table
    viewForTableColumn:(NSTableColumn *)column
                   row:(NSInteger)row {
    auto *owner = static_cast<native::table_view *>(_owner);
    if (!owner || !owner->get_model() || row < 0 ||
        static_cast<std::size_t>(row) >=
            owner->get_display_row_count()) {
        return nil;
    }
    const native::table_display_row display =
        owner->get_display_row(static_cast<std::size_t>(row));
    if (display.group) {
        if (column && column != [[table tableColumns] firstObject])
            return nil;
        NSString *identifier = @"native_table_group";
        NSTableCellView *cell = [table
            makeViewWithIdentifier:identifier owner:self];
        if (!cell) {
            cell = [[[NSTableCellView alloc]
                initWithFrame:NSMakeRect(0, 0, 200, 22)] autorelease];
            [cell setIdentifier:identifier];
            native_table_group_button *button =
                [[native_table_group_button alloc]
                initWithFrame:NSMakeRect(2, 1, 190, 20)];
            [button setButtonType:NSButtonTypeOnOff];
            [button setBezelStyle:NSBezelStyleDisclosure];
            [button setBordered:YES];
            [button setTarget:self];
            [button setAction:@selector(toggleGroup:)];
            [button setAlignment:NSTextAlignmentLeft];
            [cell addSubview:button];
            [button release];
            NSTextField *label = [[NSTextField alloc]
                initWithFrame:NSMakeRect(22, 1, 176, 20)];
            [label setBezeled:NO];
            [label setEditable:NO];
            [label setSelectable:NO];
            [label setDrawsBackground:NO];
            [label setFont:[NSFont boldSystemFontOfSize:0]];
            [[label cell] setLineBreakMode:NSLineBreakByTruncatingTail];
            [label setAutoresizingMask:NSViewWidthSizable];
            [cell addSubview:label];
            [cell setTextField:label];
            [label release];
        }
        native::table_model *model = owner->get_model();
        const auto group = group_by_id(*model, display.group_id);
        native_table_group_button *button =
            static_cast<native_table_group_button *>(
                [[cell subviews] firstObject]);
        button->_owner = owner;
        button->_group = display.group_id;
        const bool custom = typeid(*owner) != typeid(native::table_view);
        [button setTitle:custom && group ? native_string(group->title) : @""];
        [button setFrame:NSMakeRect(2, 1, custom ? 190 : 16, 20)];
        [[cell textField] setHidden:custom];
        [[cell textField] setStringValue:group ? native_string(group->title) : @""];
        [button setTag:static_cast<NSInteger>(display.group_id)];
        [button setEnabled:group && group->collapsible];
        [button setState:group && owner->get_group_expanded(group->id)
                             ? NSControlStateValueOn
                             : NSControlStateValueOff];
        return cell;
    }

    NSString *identifier = [NSString stringWithFormat:
        @"native_table_%u", column_id(column)];
    native_table_cell_view *cell = [table
        makeViewWithIdentifier:identifier owner:self];
    if (!cell) {
        cell = [[[native_table_cell_view alloc]
            initWithFrame:NSMakeRect(0, 0, [column width], 22)]
                autorelease];
        [cell setIdentifier:identifier];
    }
    cell->_owner = owner;
    cell->_row = row;
    cell->_column = column_id(column);
    const auto value = owner->get_model()->cell(display.model_row,
        cell->_column);
    const auto found = std::find_if(owner->get_columns().begin(),
        owner->get_columns().end(), [&](const native::table_column &candidate) {
            return candidate.id == cell->_column;
        });
    const auto alignment = found == owner->get_columns().end()
        ? native::table_alignment::start : found->alignment;
    const bool allow_image = found != owner->get_columns().end() &&
        found->allow_image;
    const bool custom = typeid(*owner) != typeid(native::table_view);
    mac::configure_cell(cell, native_string(value.text),
        allow_image ? mac::cell_image(value.image) : nil,
        alignment == native::table_alignment::end ? NSTextAlignmentRight :
        alignment == native::table_alignment::center ? NSTextAlignmentCenter :
        NSTextAlignmentLeft);
    [[cell textField] setHidden:custom];
    [[cell imageView] setHidden:custom];
    [cell setNeedsDisplay:YES];
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)note {
    auto *owner = static_cast<native::table_view *>(_owner);
    auto *binding = owner
        ? mac::table_view_bindings.object_from_handle(owner)
        : nullptr;
    if (!owner || !binding || binding->suppress ||
        !owner->get_model()) {
        return;
    }
    NSIndexSet *indexes = [[note object] selectedRowIndexes];
    std::vector<native::table_row_id> rows;
    for (NSUInteger index = [indexes firstIndex];
         index != NSNotFound;
         index = [indexes indexGreaterThanIndex:index]) {
        const native::table_display_row display =
            owner->get_display_row(index);
        if (!display.group)
            rows.push_back(owner->get_model()->row_id(
                display.model_row));
    }
    owner->on_native_selection(rows);
}

- (void)tableView:(NSTableView *)table
    sortDescriptorsDidChange:(NSArray<NSSortDescriptor *> *)old {
    (void)old;
    auto *owner = static_cast<native::table_view *>(_owner);
    auto *binding = owner
        ? mac::table_view_bindings.object_from_handle(owner)
        : nullptr;
    if (!binding || binding->suppress)
        return;
    NSSortDescriptor *sort = [[table sortDescriptors] firstObject];
    if (owner && sort)
        owner->on_native_sort_request(
            static_cast<native::table_column_id>(
                [[sort key] integerValue]));
}

- (void)tableViewColumnDidResize:(NSNotification *)note {
    auto *owner = static_cast<native::table_view *>(_owner);
    NSTableColumn *column = [[note userInfo]
        objectForKey:@"NSTableColumn"];
    if (owner && column)
        owner->on_native_column_resize(
            column_id(column),
            static_cast<native::dim>(std::clamp<CGFloat>(
                [column width], 0, 65535)));
}

- (void)tableViewColumnDidMove:(NSNotification *)note {
    auto *owner = static_cast<native::table_view *>(_owner);
    NSTableView *table = [note object];
    NSNumber *new_index = [[note userInfo]
        objectForKey:@"NSTableViewNewColumn"];
    if (!owner || !new_index)
        return;
    const NSInteger index = [new_index integerValue];
    if (index >= 0 && static_cast<NSUInteger>(index) < [[table tableColumns] count]) {
        owner->on_native_column_move(
            column_id([[table tableColumns] objectAtIndex:index]),
            static_cast<std::size_t>(index));
    }
}

- (void)toggleGroup:(id)sender {
    auto *owner = static_cast<native::table_view *>(_owner);
    if (!owner)
        return;
    const auto id = static_cast<native::table_group_id>([sender tag]);
    owner->on_native_group_expand(
        id, !owner->get_group_expanded(id));
}

- (void)doubleClick:(id)sender {
    (void)sender;
    auto *owner = static_cast<native::table_view *>(_owner);
    if (!owner)
        return;
    const auto selected = owner->get_selected_rows();
    if (!selected.empty())
        owner->on_native_activate(selected.back());
}
@end

namespace
{
    void clear_images(mac::mac_table_view &binding) {
        for (auto &entry : binding.images)
            [entry.second release];
        binding.images.clear();
    }

    void rebuild(native::table_view &owner) {
        auto &binding = binding_for(owner);
        binding.suppress = true;
        clear_images(binding);
        while ([[binding.table tableColumns] count] > 0) {
            [binding.table removeTableColumn:
                [[binding.table tableColumns] lastObject]];
        }
        for (const auto &column : owner.get_columns()) {
            if (!column.visible)
                continue;
            NSString *identifier = [NSString stringWithFormat:
                @"%u", column.id];
            NSTableColumn *native_column = [[NSTableColumn alloc]
                initWithIdentifier:identifier];
            native_table_header_cell *header =
                [[native_table_header_cell alloc]
                    initTextCell:native_string(column.title)];
            header->_owner = &owner;
            header->_column = column.id;
            [native_column setHeaderCell:header];
            [header release];
            [native_column setWidth:column.width];
            [native_column setMinWidth:column.min_width];
            [native_column setMaxWidth:column.max_width];
            [native_column setResizingMask:
                owner.get_columns_resizable() && column.resizable
                    ? NSTableColumnUserResizingMask |
                          NSTableColumnAutoresizingMask
                    : NSTableColumnNoResizing];
            if (column.sortable) {
                NSSortDescriptor *sort = [[NSSortDescriptor alloc]
                    initWithKey:identifier ascending:YES];
                [native_column setSortDescriptorPrototype:sort];
                [sort release];
            }
            [binding.table addTableColumn:native_column];
            [native_column release];
        }
        if (owner.get_sort()) {
            NSString *key = [NSString stringWithFormat:
                @"%u", owner.get_sort()->column];
            NSSortDescriptor *sort = [[NSSortDescriptor alloc]
                initWithKey:key
                  ascending:owner.get_sort()->direction ==
                            native::sort_direction::ascending];
            [binding.table setSortDescriptors:@[sort]];
            [sort release];
        } else {
            [binding.table setSortDescriptors:@[]];
        }
        [binding.table setHeaderView:
            owner.get_header_visible() ? binding.header : nil];
        [binding.table setAllowsColumnReordering:
            owner.get_columns_reorderable()];
        [binding.table setColumnAutoresizingStyle:
            owner.get_fill_last_column()
                ? NSTableViewLastColumnOnlyAutoresizingStyle
                : NSTableViewNoColumnAutoresizing];
        if (owner.get_fill_last_column() &&
            [[binding.table tableColumns] count] > 0) {
            CGFloat total = 0;
            for (NSTableColumn *column in [binding.table tableColumns])
                total += [column width];
            const CGFloat available = [binding.table bounds].size.width;
            if (available > total) {
                NSTableColumn *last =
                    [[binding.table tableColumns] lastObject];
                [last setWidth:[last width] + available - total];
            }
        }
        [binding.table setAllowsMultipleSelection:
            owner.get_selection_mode() ==
                native::table_selection_mode::multiple];
        [binding.table setUsesAlternatingRowBackgroundColors:
            owner.get_alternating_rows()];
        NSUInteger style = NSTableViewGridNone;
        if (owner.get_grid_lines() ==
                native::table_grid_lines::horizontal ||
            owner.get_grid_lines() == native::table_grid_lines::both) {
            style |= NSTableViewSolidHorizontalGridLineMask;
        }
        if (owner.get_grid_lines() ==
                native::table_grid_lines::vertical ||
            owner.get_grid_lines() == native::table_grid_lines::both) {
            style |= NSTableViewSolidVerticalGridLineMask;
        }
        [binding.table setGridStyleMask:
            static_cast<NSTableViewGridLineStyle>(style)];
        [binding.table setGridColor:table_tint(0.24)];
        [binding.table setRowHeight:owner.get_row_height()
            ? *owner.get_row_height() : binding.default_row_height];
        [binding.scroll setHasVerticalScroller:
            owner.get_vertical_scrollbar_policy() !=
                native::scrollbar_policy::never];
        [binding.scroll setHasHorizontalScroller:
            owner.get_horizontal_scrollbar_policy() !=
                native::scrollbar_policy::never];
        [binding.scroll setAutohidesScrollers:
            owner.get_vertical_scrollbar_policy() ==
                    native::scrollbar_policy::automatic &&
                owner.get_horizontal_scrollbar_policy() ==
                    native::scrollbar_policy::automatic];
        [binding.table reloadData];
        binding.suppress = false;
    }
} // namespace

namespace native
{
    void table_view::apply_table() {
        rebuild(*this);
        auto &state = binding_for(*this);
        _native_row_height = static_cast<int>([state.table rowHeight]);
    }

    void table_view::apply_selection() {
        auto &binding = binding_for(*this);
        binding.suppress = true;
        NSMutableIndexSet *indexes = [NSMutableIndexSet indexSet];
        for (table_row_id id : _selection) {
            const auto display =
                id == _focused_row && _focused_model_row
                    ? _visible_rows->display_index_for_model_row(
                          *_focused_model_row)
                    : display_row_for_id(id);
            if (display)
                [indexes addIndex:*display];
        }
        [binding.table selectRowIndexes:indexes
                   byExtendingSelection:NO];
        binding.suppress = false;
    }

    void table_view::apply_scroll() {
        auto &binding = binding_for(*this);
        if (binding.suppress) return;
        binding.suppress = true;
        const CGFloat y = _vertical_row < get_display_row_count()
            ? [binding.table rectOfRow:_vertical_row].origin.y : 0;
        [[binding.scroll contentView]
            scrollToPoint:NSMakePoint(_horizontal_offset, y)];
        [binding.scroll reflectScrolledClipView:
            [binding.scroll contentView]];
        binding.suppress = false;
    }

    void table_view::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: table_view requires a created parent.");
        NSScrollView *scroll = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [scroll setBorderType:NSBezelBorder];
        [scroll setClipsToBounds:YES];
        [[scroll contentView] setClipsToBounds:YES];
        native_table_widget *table = [[native_table_widget alloc]
            initWithFrame:NSMakeRect(0, 0, _bounds.d.w, _bounds.d.h)];
        table->_owner = self;
        if (@available(macOS 11.0, *))
            [table setStyle:NSTableViewStyleFullWidth];
        [table setIntercellSpacing:NSMakeSize(1, 0)];
        [table setColumnAutoresizingStyle:
            NSTableViewNoColumnAutoresizing];
        [table setSelectionHighlightStyle:
            NSTableViewSelectionHighlightStyleRegular];
        [table setFloatsGroupRows:YES];
        native_table_adapter *adapter =
            [[native_table_adapter alloc] init];
        adapter->_owner = self;
        [table setDataSource:adapter];
        [table setDelegate:adapter];
        [table setTarget:adapter];
        [table setDoubleAction:@selector(doubleClick:)];
        [scroll setDocumentView:table];
        [parent addSubview:scroll];
        auto *binding = new mac::mac_table_view();
        binding->scroll = scroll;
        binding->table = table;
        binding->header = [[table headerView] retain];
        binding->adapter = adapter;
        binding->default_row_height = [table rowHeight];
        mac::table_view_bindings.register_pair(self, binding);
        [[scroll contentView] setPostsBoundsChangedNotifications:YES];
        [[NSNotificationCenter defaultCenter] addObserver:adapter
            selector:@selector(clipBoundsChanged:)
            name:NSViewBoundsDidChangeNotification object:[scroll contentView]];
        self->synchronize_theme_metrics();
        self->apply_table();
        self->apply_selection();
        self->apply_scroll();
    }

    void table_view::show_native() {
        auto *binding = mac::table_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->scroll)
            throw std::runtime_error(
                "macOS: table_view is not created.");
        [binding->scroll setHidden:NO];
    }

    void table_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            mac::table_view_bindings.object_from_handle(self);
        if (binding) {
            [[NSNotificationCenter defaultCenter] removeObserver:binding->adapter];
            [binding->table setDataSource:nil];
            [binding->table setDelegate:nil];
            clear_images(*binding);
            [binding->scroll removeFromSuperview];
            [binding->scroll setDocumentView:nil];
            [binding->header release];
            [binding->scroll release];
            [binding->table release];
            [binding->adapter release];
            mac::table_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

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

#include <native.h>

#include "../../table_visible_rows.h"
#include "globals.h"

namespace
{
    NSString *native_string(const std::string &value) {
        NSString *result = [NSString stringWithUTF8String:value.c_str()];
        return result ? result : @"";
    }

    NSImage *native_image(const native::img &source) {
        NSBitmapImageRep *representation =
            [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr
                              pixelsWide:source.w()
                              pixelsHigh:source.h()
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSCalibratedRGBColorSpace
                             bitmapFormat:0
                              bytesPerRow:source.w() * 4
                             bitsPerPixel:32];
        if (!representation)
            return nil;
        std::uint8_t *target = [representation bitmapData];
        for (int y = 0; y < source.h(); ++y) {
            for (int x = 0; x < source.w(); ++x) {
                const native::rgba color =
                    source.pixels()[y * source.w() + x];
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * source.w() + x) * 4;
                target[offset] = color.r;
                target[offset + 1] = color.g;
                target[offset + 2] = color.b;
                target[offset + 3] = color.a;
            }
        }
        NSImage *image = [[NSImage alloc]
            initWithSize:NSMakeSize(source.w(), source.h())];
        [image addRepresentation:representation];
        [representation release];
        return image;
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

    NSImage *cached_image(native::table_view &table,
                          const native::img *image) {
        if (!image)
            return nil;
        auto &binding = binding_for(table);
        const auto found = binding.images.find(image);
        if (found != binding.images.end())
            return found->second;
        NSImage *converted = native_image(*image);
        if (converted)
            binding.images[image] = converted;
        return converted;
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

@interface native_table_adapter
    : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
@public
    void *_owner;
}
- (void)toggleGroup:(id)sender;
- (void)doubleClick:(id)sender;
@end

@implementation native_table_adapter
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
        if (column != [[table tableColumns] firstObject])
            return nil;
        NSString *identifier = @"native_table_group";
        NSTableCellView *cell = [table
            makeViewWithIdentifier:identifier owner:self];
        if (!cell) {
            cell = [[[NSTableCellView alloc]
                initWithFrame:NSMakeRect(0, 0, 200, 22)] autorelease];
            [cell setIdentifier:identifier];
            NSButton *button = [[NSButton alloc]
                initWithFrame:NSMakeRect(2, 1, 190, 20)];
            [button setButtonType:NSButtonTypeOnOff];
            [button setBezelStyle:NSBezelStyleDisclosure];
            [button setBordered:NO];
            [button setTarget:self];
            [button setAction:@selector(toggleGroup:)];
            [button setAlignment:NSTextAlignmentLeft];
            [cell addSubview:button];
            [button release];
        }
        native::table_model *model = owner->get_model();
        const auto group = group_by_id(*model, display.group_id);
        NSButton *button = (NSButton *)[[cell subviews] firstObject];
        [button setTitle:group ? native_string(group->title) : @""];
        [button setTag:static_cast<NSInteger>(display.group_id)];
        [button setEnabled:group && group->collapsible];
        [button setState:group && owner->get_group_expanded(group->id)
                             ? NSControlStateValueOn
                             : NSControlStateValueOff];
        return cell;
    }

    NSString *identifier = [NSString stringWithFormat:
        @"native_table_%u", column_id(column)];
    NSTableCellView *cell = [table
        makeViewWithIdentifier:identifier owner:self];
    if (!cell) {
        cell = [[[NSTableCellView alloc]
            initWithFrame:NSMakeRect(0, 0, [column width], 22)]
                autorelease];
        [cell setIdentifier:identifier];
        NSImageView *image = [[NSImageView alloc]
            initWithFrame:NSMakeRect(2, 3, 16, 16)];
        [image setImageScaling:NSImageScaleProportionallyDown];
        NSTextField *text = [[NSTextField alloc]
            initWithFrame:NSMakeRect(22, 1,
                                     std::max<CGFloat>(1,
                                         [column width] - 24),
                                     20)];
        [text setBezeled:NO];
        [text setEditable:NO];
        [text setSelectable:NO];
        [text setDrawsBackground:NO];
        [text setLineBreakMode:NSLineBreakByTruncatingTail];
        [cell addSubview:image];
        [cell addSubview:text];
        [cell setImageView:image];
        [cell setTextField:text];
        [image release];
        [text release];
    }
    const native::table_cell value = owner->get_model()->cell(
        display.model_row, column_id(column));
    [cell.textField setStringValue:native_string(value.text)];
    [cell.imageView setImage:cached_image(*owner, value.image)];
    const bool has_image = value.image != nullptr;
    const native::size icon = owner->get_icon_size()
        .value_or(native::size(16, 16));
    [cell.imageView setHidden:!has_image];
    [cell.imageView setFrame:NSMakeRect(
        2, std::max<CGFloat>(1, ([table rowHeight] - icon.h) / 2),
        icon.w, icon.h)];
    const CGFloat text_x = has_image ? icon.w + 8 : 2;
    [cell.textField setFrame:NSMakeRect(
        text_x, 1,
        std::max<CGFloat>(1, [column width] - text_x - 2),
        std::max<CGFloat>(1, [table rowHeight] - 2))];
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
    if (index >= 0 && index < [[table tableColumns] count]) {
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
            [[native_column headerCell]
                setStringValue:native_string(column.title)];
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
        if (owner.get_row_height())
            [binding.table setRowHeight:*owner.get_row_height()];
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
    void table_view::apply_table() { rebuild(*this); }

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
        if (_vertical_row < get_display_row_count())
            [binding.table scrollRowToVisible:_vertical_row];
        [[binding.scroll contentView]
            scrollToPoint:NSMakePoint(_horizontal_offset,
                                      [[binding.scroll contentView]
                                          bounds].origin.y)];
        [binding.scroll reflectScrolledClipView:
            [binding.scroll contentView]];
    }

    void table_view::create() const {
        if (_created)
            return;
        NSView *parent = mac::parent_view(get_parent());
        if (!parent)
            throw std::runtime_error(
                "macOS: table_view requires a created parent.");
        auto *self = const_cast<table_view *>(this);
        NSScrollView *scroll = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [scroll setBorderType:NSBezelBorder];
        native_table_widget *table = [[native_table_widget alloc]
            initWithFrame:NSMakeRect(0, 0, _bounds.d.w, _bounds.d.h)];
        table->_owner = self;
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
        mac::table_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        rebuild(*self);
        self->apply_selection();
        self->apply_scroll();
        self->on_wnd_create.emit();
    }

    void table_view::show() const {
        auto *binding = mac::table_view_bindings.object_from_handle(
            const_cast<table_view *>(this));
        if (!_created || !binding || !binding->scroll)
            throw std::runtime_error(
                "macOS: table_view is not created.");
        [binding->scroll setHidden:NO];
    }

    void table_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *binding =
            mac::table_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
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

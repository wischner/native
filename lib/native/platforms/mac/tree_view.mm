//
// Implements tree_view with AppKit NSOutlineView and NSScrollView.
// Retained NSNumber objects provide stable native outline identity while
// the public API continues to expose only portable 64-bit item IDs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <functional>
#include <stdexcept>

#include <native.h>

#include "../../control_render_access.h"
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

    native::tree_item_id item_id(id item) {
        return [item isKindOfClass:[NSNumber class]]
                   ? static_cast<native::tree_item_id>(
                         [static_cast<NSNumber *>(item)
                             unsignedLongLongValue])
                   : native::invalid_tree_item_id;
    }

    mac::mac_tree_view &binding_for(native::tree_view &tree) {
        auto *binding =
            mac::tree_view_bindings.object_from_handle(&tree);
        if (!binding || !binding->outline)
            throw std::runtime_error(
                "macOS: missing tree_view binding.");
        return *binding;
    }

    const std::vector<native::tree_view_item> &children_for(
        native::tree_view &tree,
        id item) {
        const native::tree_item_id id = item_id(item);
        return id == native::invalid_tree_item_id
                   ? tree.get_items()
                   : tree.get_item(id).children;
    }
} // namespace

@interface native_tree_outline_view : NSOutlineView {
@public
    void *_owner;
}
@end

@implementation native_tree_outline_view
- (void)keyDown:(NSEvent *)event {
    auto *owner = static_cast<native::tree_view *>(_owner);
    if (owner && ([event keyCode] == 36 || [event keyCode] == 76)) {
        owner->on_native_navigation(
            native::tree_view_navigation::activate);
        return;
    }
    if (owner && [[event charactersIgnoringModifiers] isEqualToString:@" "]) {
        owner->on_native_navigation(
            native::tree_view_navigation::toggle);
        return;
    }
    [super keyDown:event];
}
@end

@interface native_tree_cell_view : NSView {
@public
    void *_owner;
    native::tree_item_id _item;
}
@end

@implementation native_tree_cell_view
- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::tree_view *>(_owner);
    if (!owner || !owner->get_created() ||
        _item == native::invalid_tree_item_id) {
        [super drawRect:dirty];
        return;
    }
    std::size_t visible_index = 0;
    for (; visible_index < owner->get_visible_item_count();
         ++visible_index) {
        if (owner->get_visible_item(visible_index).id == _item)
            break;
    }
    if (visible_index >= owner->get_visible_item_count())
        return;
    const native::tree_view_visible_item visible =
        owner->get_visible_item(visible_index);
    native::gpx &graphics = owner->get_gpx();
    auto appearance = native::theme::create(graphics);
    const native::theme::metrics metrics = appearance->defaults();
    const native::rect portable_row =
        owner->get_row_bounds(visible_index);
    const native::rect portable_disclosure =
        owner->get_disclosure_bounds(visible_index);
    const int native_indent =
        portable_disclosure.x2() - portable_row.p.x +
        metrics.header_gap;
    native::rect bounds(
        static_cast<native::coord>(-native_indent),
        0,
        static_cast<native::dim>(std::max<CGFloat>(
            0, [self bounds].size.width + native_indent)),
        static_cast<native::dim>(std::max<CGFloat>(
            0, [self bounds].size.height)));
    graphics.set_clip(native::rect(
        0,
        0,
        static_cast<native::dim>(std::max<CGFloat>(
            0, [self bounds].size.width)),
        bounds.d.h));
    native::theme::state state;
    state.selected = owner->get_selected_item() == _item;
    state.disabled = !owner->get_item(_item).enabled;
    state.focused = [[self window] firstResponder] != nil;
    native::detail::control_render_access::draw_tree_row(
        *owner,
        graphics,
        *appearance,
        visible_index,
        owner->get_item(_item),
        visible.depth,
        bounds,
        state);
}
@end

@interface native_tree_adapter
    : NSObject <NSOutlineViewDataSource, NSOutlineViewDelegate> {
@public
    void *_owner;
}
@end

@implementation native_tree_adapter
- (NSInteger)outlineView:(NSOutlineView *)outline
    numberOfChildrenOfItem:(id)item {
    (void)outline;
    auto *owner = static_cast<native::tree_view *>(_owner);
    return owner ? static_cast<NSInteger>(
                       children_for(*owner, item).size())
                 : 0;
}

- (id)outlineView:(NSOutlineView *)outline
            child:(NSInteger)index
           ofItem:(id)item {
    (void)outline;
    auto *owner = static_cast<native::tree_view *>(_owner);
    auto *binding = owner
                        ? mac::tree_view_bindings
                              .object_from_handle(owner)
                        : nullptr;
    if (!owner || !binding || index < 0)
        return nil;
    const auto &children = children_for(*owner, item);
    if (index >= static_cast<NSInteger>(children.size()))
        return nil;
    const auto found = binding->items.find(children[index].id);
    return found == binding->items.end() ? nil : found->second;
}

- (BOOL)outlineView:(NSOutlineView *)outline
   isItemExpandable:(id)item {
    (void)outline;
    auto *owner = static_cast<native::tree_view *>(_owner);
    const native::tree_item_id id = item_id(item);
    return owner && id != native::invalid_tree_item_id &&
           !owner->get_item(id).children.empty();
}

- (NSView *)outlineView:(NSOutlineView *)outline
    viewForTableColumn:(NSTableColumn *)column
                  item:(id)item {
    (void)column;
    auto *owner = static_cast<native::tree_view *>(_owner);
    const native::tree_item_id id = item_id(item);
    if (!owner || id == native::invalid_tree_item_id)
        return nil;
    NSString *identifier = @"native_tree_cell";
    native_tree_cell_view *cell = [outline
        makeViewWithIdentifier:identifier owner:self];
    if (!cell) {
        cell = [[[native_tree_cell_view alloc]
            initWithFrame:NSMakeRect(0, 0, 180, 20)] autorelease];
        [cell setIdentifier:identifier];
    }
    cell->_owner = owner;
    cell->_item = id;
    [cell setNeedsDisplay:YES];
    return cell;
}

- (id)outlineView:(NSOutlineView *)outline
    objectValueForTableColumn:(NSTableColumn *)column
                       byItem:(id)item {
    (void)outline;
    (void)column;
    auto *owner = static_cast<native::tree_view *>(_owner);
    const native::tree_item_id id = item_id(item);
    return owner && id != native::invalid_tree_item_id
               ? native_string(owner->get_item(id).text)
               : @"";
}

- (BOOL)outlineView:(NSOutlineView *)outline
    shouldSelectItem:(id)item {
    (void)outline;
    auto *owner = static_cast<native::tree_view *>(_owner);
    const native::tree_item_id id = item_id(item);
    return owner && id != native::invalid_tree_item_id &&
           owner->get_item(id).enabled;
}

- (void)outlineView:(NSOutlineView *)outline
    willDisplayCell:(id)cell
      forTableColumn:(NSTableColumn *)column
                item:(id)item {
    (void)outline;
    (void)column;
    auto *owner = static_cast<native::tree_view *>(_owner);
    auto *binding = owner
                        ? mac::tree_view_bindings
                              .object_from_handle(owner)
                        : nullptr;
    const native::tree_item_id id = item_id(item);
    if (!owner || !binding || id == native::invalid_tree_item_id)
        return;
    const auto image = binding->images.find(id);
    if ([cell respondsToSelector:@selector(setImage:)])
        [cell setImage:image == binding->images.end()
                           ? nil
                           : image->second];
    if ([cell respondsToSelector:@selector(setTextColor:)]) {
        [cell setTextColor:owner->get_item(id).enabled
                               ? [NSColor controlTextColor]
                               : [NSColor disabledControlTextColor]];
    }
}

- (void)outlineViewSelectionDidChange:(NSNotification *)note {
    auto *owner = static_cast<native::tree_view *>(_owner);
    auto *binding = owner
                        ? mac::tree_view_bindings
                              .object_from_handle(owner)
                        : nullptr;
    if (!owner || !binding || binding->suppress)
        return;
    NSOutlineView *outline = [note object];
    const NSInteger row = [outline selectedRow];
    owner->on_native_selection(
        row < 0 ? native::invalid_tree_item_id
                : item_id([outline itemAtRow:row]));
}

- (void)outlineViewItemDidExpand:(NSNotification *)note {
    auto *owner = static_cast<native::tree_view *>(_owner);
    auto *binding = owner
                        ? mac::tree_view_bindings
                              .object_from_handle(owner)
                        : nullptr;
    if (!owner || !binding || binding->suppress)
        return;
    const native::tree_item_id id = item_id(
        [[note userInfo] objectForKey:@"NSObject"]);
    if (id != native::invalid_tree_item_id) {
        binding->suppress = true;
        if (owner->get_item(id).enabled)
            owner->on_native_expansion(id, true);
        else
            [binding->outline collapseItem:
                                  binding->items[id]];
        binding->suppress = false;
    }
}

- (void)outlineViewItemDidCollapse:(NSNotification *)note {
    auto *owner = static_cast<native::tree_view *>(_owner);
    auto *binding = owner
                        ? mac::tree_view_bindings
                              .object_from_handle(owner)
                        : nullptr;
    if (!owner || !binding || binding->suppress)
        return;
    const native::tree_item_id id = item_id(
        [[note userInfo] objectForKey:@"NSObject"]);
    if (id != native::invalid_tree_item_id) {
        binding->suppress = true;
        if (owner->get_item(id).enabled)
            owner->on_native_expansion(id, false);
        else
            [binding->outline expandItem:
                                  binding->items[id]];
        binding->suppress = false;
    }
}

- (void)activate:(id)sender {
    NSOutlineView *outline = sender;
    auto *owner = static_cast<native::tree_view *>(_owner);
    const NSInteger row = [outline selectedRow];
    if (owner && row >= 0) {
        owner->on_native_double_click(
            item_id([outline itemAtRow:row]));
    }
}
@end

namespace
{
    void release_cached_objects(mac::mac_tree_view &binding) {
        for (auto &entry : binding.items)
            [entry.second release];
        binding.items.clear();
        for (auto &entry : binding.images)
            [entry.second release];
        binding.images.clear();
    }

    void rebuild(native::tree_view &tree) {
        auto &binding = binding_for(tree);
        binding.suppress = true;
        release_cached_objects(binding);
        std::function<void(const std::vector<native::tree_view_item> &)>
            cache;
        cache = [&binding, &cache](
                    const std::vector<native::tree_view_item> &items) {
            for (const native::tree_view_item &item : items) {
                binding.items[item.id] = [[NSNumber alloc]
                    initWithUnsignedLongLong:item.id];
                if (item.image)
                    binding.images[item.id] =
                        native_image(*item.image);
                cache(item.children);
            }
        };
        cache(tree.get_items());
        [binding.outline reloadData];
        std::function<void(const std::vector<native::tree_view_item> &)>
            expand;
        expand = [&binding, &expand](
                     const std::vector<native::tree_view_item> &items) {
            for (const native::tree_view_item &item : items) {
                if (item.expanded) {
                    const auto native_item = binding.items.find(item.id);
                    if (native_item != binding.items.end())
                        [binding.outline expandItem:native_item->second];
                }
                expand(item.children);
            }
        };
        expand(tree.get_items());
        binding.suppress = false;
    }
} // namespace

namespace native
{
    void tree_view::apply_items() {
        auto &binding = binding_for(*this);
        [binding.scroll setBorderType:get_border_visible()
                                          ? NSBezelBorder
                                          : NSNoBorder];
        [binding.outline setIndentationPerLevel:
                             std::max<int>(16, _indent_width)];
        rebuild(*this);
    }

    void tree_view::apply_selection() {
        auto &binding = binding_for(*this);
        binding.suppress = true;
        const auto found = binding.items.find(_selected_item);
        if (found == binding.items.end()) {
            [binding.outline deselectAll:nil];
        } else {
            const NSInteger row =
                [binding.outline rowForItem:found->second];
            if (row >= 0) {
                [binding.outline selectRowIndexes:
                                     [NSIndexSet indexSetWithIndex:row]
                                 byExtendingSelection:NO];
                [binding.outline scrollRowToVisible:row];
            }
        }
        binding.suppress = false;
    }

    void tree_view::apply_expansion(tree_item_id id, bool expanded) {
        auto &binding = binding_for(*this);
        if (binding.suppress)
            return;
        const auto found = binding.items.find(id);
        if (found == binding.items.end())
            return;
        binding.suppress = true;
        if (expanded)
            [binding.outline expandItem:found->second];
        else
            [binding.outline collapseItem:found->second];
        binding.suppress = false;
    }

    void tree_view::apply_scroll_offset() {
        auto &binding = binding_for(*this);
        [[binding.scroll contentView]
            scrollToPoint:NSMakePoint(0, _scroll_offset)];
        [binding.scroll reflectScrolledClipView:
                            [binding.scroll contentView]];
    }

    void tree_view::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: tree_view requires a created parent.");
        NSScrollView *scroll = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [scroll setHasVerticalScroller:YES];
        [scroll setAutohidesScrollers:YES];
        [scroll setBorderType:get_border_visible()
                                  ? NSBezelBorder
                                  : NSNoBorder];
        native_tree_outline_view *outline =
            [[native_tree_outline_view alloc]
                initWithFrame:NSMakeRect(0,
                                         0,
                                         _bounds.d.w,
                                         _bounds.d.h)];
        outline->_owner = self;
        NSTableColumn *column = [[NSTableColumn alloc]
            initWithIdentifier:@"native_tree_item"];
        [column setWidth:std::max<int>(1, _bounds.d.w - 2)];
        [column setResizingMask:NSTableColumnAutoresizingMask];
        [outline addTableColumn:column];
        [outline setOutlineTableColumn:column];
        [column release];
        [outline setHeaderView:nil];
        [outline setAllowsMultipleSelection:NO];
        [outline setAllowsEmptySelection:YES];
        [outline setSelectionHighlightStyle:
                     NSTableViewSelectionHighlightStyleRegular];
        [outline setIntercellSpacing:NSZeroSize];
        [outline setColumnAutoresizingStyle:
                     NSTableViewLastColumnOnlyAutoresizingStyle];
        [outline setBackgroundColor:[NSColor textBackgroundColor]];
        native_tree_adapter *adapter =
            [[native_tree_adapter alloc] init];
        adapter->_owner = self;
        [outline setDataSource:adapter];
        [outline setDelegate:adapter];
        [outline setTarget:adapter];
        [outline setDoubleAction:@selector(activate:)];
        [scroll setDocumentView:outline];
        [parent addSubview:scroll];
        auto *binding = new mac::mac_tree_view();
        binding->scroll = scroll;
        binding->outline = outline;
        binding->adapter = adapter;
        mac::tree_view_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        [outline setRowHeight:std::max<int>(
                                  _row_height, _icon_size.h + 2)];
        self->apply_items();
        self->apply_selection();
    }

    void tree_view::show_native() {
        auto *binding = mac::tree_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->scroll)
            throw std::runtime_error(
                "macOS: tree_view is not created.");
        [binding->scroll setHidden:NO];
    }

    void tree_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            mac::tree_view_bindings.object_from_handle(self);
        if (binding) {
            [binding->outline setDataSource:nil];
            [binding->outline setDelegate:nil];
            [binding->outline setTarget:nil];
            release_cached_objects(*binding);
            [binding->scroll removeFromSuperview];
            [binding->scroll setDocumentView:nil];
            [binding->scroll release];
            [binding->outline release];
            [binding->adapter release];
            mac::tree_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

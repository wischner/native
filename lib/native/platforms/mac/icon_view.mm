//
// Implements icon_view with AppKit NSCollectionView and flow layout.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>
#include <typeinfo>

#include <native.h>

#include "../../control_render_access.h"
#include "globals.h"
#include "native_cells.h"

namespace
{
    NSString *const item_identifier = @"native_icon_item";

    NSString *native_string(const std::string &value) {
        NSString *result = [NSString stringWithUTF8String:value.c_str()];
        return result ? result : @"";
    }

    NSImage *native_image(const native::img &source) {
        return [mac::cell_image(&source) retain];
    }
}

@interface native_icon_item_view : NSView {
@public
    void *_owner;
    NSInteger _index;
    BOOL _selected;
    NSBox *_selection;
}
@end

@implementation native_icon_item_view
- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::icon_view *>(_owner);
    if (!owner || !owner->get_created() || _index < 0 ||
        typeid(*owner) == typeid(native::icon_view) ||
        _index >= static_cast<NSInteger>(owner->get_items().size())) {
        [super drawRect:dirty];
        return;
    }
    native::gpx &graphics = owner->get_gpx();
    auto appearance = native::theme::create(graphics);
    const NSRect frame = [self bounds];
    const native::rect bounds(
        0,
        0,
        static_cast<native::dim>(std::max<CGFloat>(0, frame.size.width)),
        static_cast<native::dim>(std::max<CGFloat>(0, frame.size.height)));
    graphics.set_clip(native::rect(
        static_cast<native::coord>(dirty.origin.x),
        static_cast<native::coord>(dirty.origin.y),
        static_cast<native::dim>(std::max<CGFloat>(0, dirty.size.width)),
        static_cast<native::dim>(std::max<CGFloat>(0, dirty.size.height))));
    native::theme::state state;
    state.selected = _selected == YES;
    state.disabled = !owner->get_items()[_index].enabled;
    state.focused = state.selected &&
        [[self window] firstResponder] != nil;
    native::detail::control_render_access::draw_icon_item(
        *owner,
        graphics,
        *appearance,
        static_cast<std::size_t>(_index),
        owner->get_items()[_index],
        bounds,
        state);
}
@end

@interface native_icon_collection_item : NSCollectionViewItem
@end

@implementation native_icon_collection_item
- (void)loadView {
    native_icon_item_view *container = [[native_icon_item_view alloc]
        initWithFrame:NSMakeRect(0, 0, 96, 92)];
    NSBox *selection = [[NSBox alloc] initWithFrame:[container bounds]];
    [selection setBoxType:NSBoxCustom];
    [selection setBorderWidth:0];
    [selection setTitlePosition:NSNoTitle];
    [selection setCornerRadius:5];
    [selection setFillColor:[NSColor selectedContentBackgroundColor]];
    [selection setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [selection setHidden:YES];
    [container addSubview:selection];
    container->_selection = selection;
    [selection release];
    NSImageView *image = [[NSImageView alloc] initWithFrame:NSZeroRect];
    [image setImageScaling:NSImageScaleProportionallyDown];
    [container addSubview:image];
    [self setImageView:image];
    [image release];
    NSTextField *label = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [label setBezeled:NO];
    [label setEditable:NO];
    [label setSelectable:NO];
    [label setDrawsBackground:NO];
    [label setFont:[NSFont systemFontOfSize:0]];
    [[label cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [container addSubview:label];
    [self setTextField:label];
    [label release];
    [self setView:container];
    [container release];
}
- (void)setSelected:(BOOL)selected {
    [super setSelected:selected];
    native_icon_item_view *view =
        static_cast<native_icon_item_view *>([self view]);
    view->_selected = selected;
    const auto *owner = static_cast<native::icon_view *>(view->_owner);
    const bool custom = owner && typeid(*owner) != typeid(native::icon_view);
    [view->_selection setHidden:!selected || custom];
    const bool enabled = owner && view->_index >= 0 &&
        view->_index < static_cast<NSInteger>(owner->get_items().size()) &&
        owner->get_items()[view->_index].enabled;
    [[self textField] setTextColor:!enabled ? [NSColor disabledControlTextColor] :
        selected ? [NSColor alternateSelectedControlTextColor] : [NSColor controlTextColor]];
    [view setNeedsDisplay:YES];
}
@end

@interface native_icon_collection_view : NSCollectionView {
@public
    void *_owner;
}
@end

@implementation native_icon_collection_view
- (void)drawRect:(NSRect)dirty {
    auto *owner = static_cast<native::icon_view *>(_owner);
    if (!owner || !owner->get_created() ||
        typeid(*owner) == typeid(native::icon_view)) {
        [super drawRect:dirty];
        return;
    }
    native::gpx &graphics = owner->get_gpx();
    auto appearance = native::theme::create(graphics);
    const NSRect frame = [self bounds];
    const native::rect bounds(
        0,
        0,
        static_cast<native::dim>(std::max<CGFloat>(0, frame.size.width)),
        static_cast<native::dim>(std::max<CGFloat>(0, frame.size.height)));
    graphics.set_clip(native::rect(
        static_cast<native::coord>(dirty.origin.x),
        static_cast<native::coord>(dirty.origin.y),
        static_cast<native::dim>(std::max<CGFloat>(0, dirty.size.width)),
        static_cast<native::dim>(std::max<CGFloat>(0, dirty.size.height))));
    native::theme::state state;
    state.focused = [[self window] firstResponder] == self;
    native::detail::control_render_access::draw_icon_background(
        *owner, graphics, *appearance, bounds, state);
}
- (void)mouseDown:(NSEvent *)event {
    [super mouseDown:event];
    if ([event clickCount] < 2)
        return;
    auto *owner = static_cast<native::icon_view *>(_owner);
    if (owner)
        owner->on_native_activate(owner->get_selected_index());
}
- (void)keyDown:(NSEvent *)event {
    auto *owner = static_cast<native::icon_view *>(_owner);
    if (owner && ([event keyCode] == 36 || [event keyCode] == 76)) {
        owner->on_native_activate(owner->get_selected_index());
        return;
    }
    [super keyDown:event];
}
@end

@interface native_icon_adapter
    : NSObject <NSCollectionViewDataSource, NSCollectionViewDelegate> {
@public
    void *_owner;
}
@end

@implementation native_icon_adapter
- (NSSet<NSIndexPath *> *)collectionView:(NSCollectionView *)view
    shouldSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)paths {
    (void)view;
    auto *owner = static_cast<native::icon_view *>(_owner);
    NSMutableSet<NSIndexPath *> *enabled = [NSMutableSet set];
    for (NSIndexPath *path in paths) {
        if (owner && [path item] >= 0 &&
            [path item] < static_cast<NSInteger>(owner->get_items().size()) &&
            owner->get_items()[[path item]].enabled)
            [enabled addObject:path];
    }
    return enabled;
}
- (void)collectionView:(NSCollectionView *)view
    didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)paths {
    (void)paths;
    [self collectionViewSelectionDidChange:
        [NSNotification notificationWithName:@"native_selection" object:view]];
}
- (void)collectionView:(NSCollectionView *)view
    didDeselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)paths {
    (void)paths;
    [self collectionViewSelectionDidChange:
        [NSNotification notificationWithName:@"native_selection" object:view]];
}
- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)view {
    (void)view;
    return 1;
}
- (NSInteger)collectionView:(NSCollectionView *)view
    numberOfItemsInSection:(NSInteger)section {
    (void)view;
    (void)section;
    auto *owner = static_cast<native::icon_view *>(_owner);
    return owner ? static_cast<NSInteger>(owner->get_items().size()) : 0;
}
- (NSCollectionViewItem *)collectionView:(NSCollectionView *)view
    itemForRepresentedObjectAtIndexPath:(NSIndexPath *)path {
    auto *owner = static_cast<native::icon_view *>(_owner);
    auto *binding = owner
                        ? mac::icon_view_bindings.object_from_handle(owner)
                        : nullptr;
    auto *item = static_cast<native_icon_collection_item *>(
        [view makeItemWithIdentifier:item_identifier forIndexPath:path]);
    const NSInteger index = [path item];
    if (!owner || !binding || index < 0 ||
        index >= static_cast<NSInteger>(owner->get_items().size())) {
        return item;
    }
    const native::icon_view_item &value = owner->get_items()[index];
    native_icon_item_view *item_view =
        static_cast<native_icon_item_view *>([item view]);
    item_view->_owner = owner;
    item_view->_index = index;
    item_view->_selected = [item isSelected];
    const bool custom = typeid(*owner) != typeid(native::icon_view);
    [item.imageView setHidden:custom];
    [item.textField setHidden:custom];
    [item setSelected:[item isSelected]];
    [item_view setNeedsDisplay:YES];
    [item.imageView setImage:
                        index < static_cast<NSInteger>(binding->images.size())
                            ? binding->images[index]
                            : nil];
    [item.textField setStringValue:
                            owner->get_label_mode() ==
                                    native::icon_view_label_mode::hidden
                                ? @""
                                : native_string(value.text)];
    [item.textField setTextColor:!value.enabled ? [NSColor disabledControlTextColor] :
        [item isSelected] ? [NSColor alternateSelectedControlTextColor] :
        [NSColor controlTextColor]];
    const native::size icon = owner->get_icon_size();
    const NSRect frame = [[item view] bounds];
    if (owner->get_label_mode() ==
        native::icon_view_label_mode::beside) {
        [item.imageView setFrame:NSMakeRect(4,
                                            (frame.size.height - icon.h) / 2,
                                            icon.w,
                                            icon.h)];
        [item.textField setAlignment:NSTextAlignmentLeft];
        [item.textField setFrame:NSMakeRect(
                                     icon.w + 10,
                                     2,
                                     std::max<CGFloat>(
                                         1,
                                         frame.size.width - icon.w - 12),
                                     frame.size.height - 4)];
    } else {
        [item.imageView setFrame:NSMakeRect(
                                     (frame.size.width - icon.w) / 2,
                                     frame.size.height - icon.h - 6,
                                     icon.w,
                                     icon.h)];
        [item.textField setAlignment:NSTextAlignmentCenter];
        [item.textField setFrame:NSMakeRect(
                                     2,
                                     2,
                                     frame.size.width - 4,
                                     std::max<CGFloat>(
                                         1,
                                         frame.size.height - icon.h - 10))];
    }
    return item;
}
- (void)collectionViewSelectionDidChange:(NSNotification *)note {
    auto *owner = static_cast<native::icon_view *>(_owner);
    auto *binding = owner
                        ? mac::icon_view_bindings.object_from_handle(owner)
                        : nullptr;
    if (!owner || !binding || binding->suppress)
        return;
    NSCollectionView *view = [note object];
    NSIndexPath *path = [[view selectionIndexPaths] anyObject];
    const int selected = path ? static_cast<int>([path item]) : -1;
    if (selected >= 0 && !owner->get_items()[selected].enabled) {
        binding->suppress = true;
        if (owner->get_selected_index() < 0) {
            [view setSelectionIndexPaths:[NSSet set]];
        } else {
            NSIndexPath *previous = [NSIndexPath
                indexPathForItem:owner->get_selected_index()
                       inSection:0];
            [view setSelectionIndexPaths:[NSSet setWithObject:previous]];
        }
        binding->suppress = false;
        return;
    }
    owner->on_native_selection(selected);
}
@end

namespace
{
    mac::mac_icon_view &binding_for(native::icon_view &control) {
        auto *binding =
            mac::icon_view_bindings.object_from_handle(&control);
        if (!binding || !binding->collection)
            throw std::runtime_error(
                "macOS: missing icon_view binding.");
        return *binding;
    }

    void rebuild_images(native::icon_view &control) {
        auto &binding = binding_for(control);
        for (NSImage *image : binding.images)
            [image release];
        binding.images.clear();
        for (const auto &item : control.get_items())
            binding.images.push_back(item.image
                                         ? native_image(*item.image)
                                         : nil);
        [binding.collection reloadData];
    }

    void update_layout(native::icon_view &control) {
        auto &binding = binding_for(control);
        const native::size icon = control.get_icon_size();
        NSSize item_size;
        if (control.get_label_mode() ==
            native::icon_view_label_mode::beside) {
            item_size = NSMakeSize(
                std::max<int>(160, icon.w + 80),
                std::max<int>(icon.h + 12, 52));
        } else {
            item_size = NSMakeSize(
                std::max<int>(80, icon.w + 12),
                icon.h +
                    (control.get_label_mode() ==
                             native::icon_view_label_mode::hidden
                         ? 12
                         : 40));
        }
        [binding.layout setItemSize:item_size];
        [binding.layout invalidateLayout];
        [binding.collection reloadData];
    }
} // namespace

namespace native
{
    void icon_view::apply_items() { rebuild_images(*this); }
    void icon_view::apply_icon_size() { update_layout(*this); }
    void icon_view::apply_label_mode() { update_layout(*this); }

    void icon_view::apply_selected_index() {
        auto &binding = binding_for(*this);
        binding.suppress = true;
        if (_selected_index < 0) {
            [binding.collection setSelectionIndexPaths:[NSSet set]];
        } else {
            NSIndexPath *path = [NSIndexPath
                indexPathForItem:_selected_index inSection:0];
            [binding.collection
                setSelectionIndexPaths:[NSSet setWithObject:path]];
            const NSCollectionViewScrollPosition position =
                NSCollectionViewScrollPositionNearestVerticalEdge;
            [binding.collection scrollToItemsAtIndexPaths:
                                    [NSSet setWithObject:path]
                                           scrollPosition:
                                               position];
        }
        binding.suppress = false;
    }

    void icon_view::apply_scroll_offset() {
        auto &binding = binding_for(*this);
        [[binding.scroll contentView]
            scrollToPoint:NSMakePoint(0, _scroll_offset)];
        [binding.scroll reflectScrolledClipView:
                            [binding.scroll contentView]];
    }

    void icon_view::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: icon_view requires a created parent.");
        NSScrollView *scroll = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [scroll setHasVerticalScroller:YES];
        [scroll setAutohidesScrollers:YES];
        [scroll setBorderType:NSBezelBorder];
        NSCollectionViewFlowLayout *layout =
            [[NSCollectionViewFlowLayout alloc] init];
        [layout setScrollDirection:NSCollectionViewScrollDirectionVertical];
        [layout setMinimumInteritemSpacing:4];
        [layout setMinimumLineSpacing:4];
        [layout setSectionInset:NSEdgeInsetsMake(6, 6, 6, 6)];
        native_icon_collection_view *collection =
            [[native_icon_collection_view alloc]
                initWithFrame:NSMakeRect(0,
                                         0,
                                         _bounds.d.w,
                                         _bounds.d.h)];
        collection->_owner = self;
        [collection setCollectionViewLayout:layout];
        [collection setSelectable:YES];
        [collection setAllowsMultipleSelection:NO];
        [collection setBackgroundColors:
                         @[[NSColor controlBackgroundColor]]];
        [collection registerClass:[native_icon_collection_item class]
            forItemWithIdentifier:item_identifier];
        native_icon_adapter *adapter =
            [[native_icon_adapter alloc] init];
        adapter->_owner = self;
        [collection setDataSource:adapter];
        [collection setDelegate:adapter];
        [scroll setDocumentView:collection];
        [parent addSubview:scroll];
        auto *binding = new mac::mac_icon_view();
        binding->scroll = scroll;
        binding->collection = collection;
        binding->layout = layout;
        binding->adapter = adapter;
        mac::icon_view_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        rebuild_images(*self);
        update_layout(*self);
        self->apply_selected_index();
    }

    void icon_view::show_native() {
        auto *binding = mac::icon_view_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->scroll)
            throw std::runtime_error(
                "macOS: icon_view is not created.");
        [binding->scroll setHidden:NO];
    }

    void icon_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            mac::icon_view_bindings.object_from_handle(self);
        if (binding) {
            [binding->collection setDataSource:nil];
            [binding->collection setDelegate:nil];
            for (NSImage *image : binding->images)
                [image release];
            [binding->scroll removeFromSuperview];
            [binding->scroll setDocumentView:nil];
            [binding->scroll release];
            [binding->collection release];
            [binding->layout release];
            [binding->adapter release];
            mac::icon_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

//
// Implements tab_view with NSTabView and NSTabViewItem.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>

#include <native.h>

#include "globals.h"

@interface native_tab_delegate : NSObject <NSTabViewDelegate> {
@public
    void *_owner;
}
@end

@interface native_tab_page_host : NSView {
@public
    CGFloat _tabHeight;
}
@end

@implementation native_tab_page_host
- (BOOL)isFlipped { return YES; }
- (NSView *)hitTest:(NSPoint)point {
    if (point.y < _tabHeight)
        return nil;
    return [super hitTest:point];
}
@end

@implementation native_tab_delegate
- (BOOL)tabView:(NSTabView *)view
    shouldSelectTabViewItem:(NSTabViewItem *)item {
    auto *owner = static_cast<native::tab_view *>(_owner);
    const NSInteger index = [view indexOfTabViewItem:item];
    return owner && index >= 0 &&
        owner->get_item(static_cast<std::size_t>(index)).get_enabled();
}

- (void)tabView:(NSTabView *)view
    didSelectTabViewItem:(NSTabViewItem *)item {
    auto *owner = static_cast<native::tab_view *>(_owner);
    auto *binding = owner
        ? mac::tab_view_bindings.object_from_handle(owner)
        : nullptr;
    if (!owner || !binding || binding->suppress)
        return;
    const NSInteger index = [view indexOfTabViewItem:item];
    if (index >= 0)
        owner->on_native_selection(static_cast<int>(index));
}
@end

namespace
{
    NSString *native_string(const std::string &value) {
        NSString *text = [NSString stringWithUTF8String:value.c_str()];
        return text ? text : @"";
    }

    mac::mac_tab_view *binding(native::tab_view &owner) {
        return mac::tab_view_bindings.object_from_handle(&owner);
    }

    NSTabViewType native_placement(native::tab_placement placement) {
        switch (placement) {
        case native::tab_placement::top:
            return NSTopTabsBezelBorder;
        case native::tab_placement::bottom:
            return NSBottomTabsBezelBorder;
        case native::tab_placement::left:
            return NSLeftTabsBezelBorder;
        case native::tab_placement::right:
            return NSRightTabsBezelBorder;
        }
        return NSTopTabsBezelBorder;
    }

    void apply_placement(native::tab_view &owner,
                         mac::mac_tab_view &state) {
        [state.view setTabViewType:
            native_placement(owner.get_tab_placement())];
        if (state.page_host) {
            [state.page_host setFrame:[state.view contentRect]];
            [state.page_host setNeedsDisplay:YES];
        }
        [state.view setNeedsDisplay:YES];
    }
}

namespace native
{
    void tab_view::apply_items() {
        auto *state = binding(*this);
        if (!state || !state->view)
            throw std::runtime_error("macOS: missing tab-view binding.");
        apply_placement(*this, *state);
        const NSRect content = [state->view contentRect];
        _tab_height = std::max(
            1,
            static_cast<int>(
                get_tab_placement() == tab_placement::left ||
                        get_tab_placement() == tab_placement::right
                    ? _bounds.d.w - content.size.width
                    : _bounds.d.h - content.size.height));
        state->suppress = true;
        while ([state->view numberOfTabViewItems] > 0)
            [state->view removeTabViewItem:
                [state->view tabViewItemAtIndex:0]];
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            NSTabViewItem *item = [[NSTabViewItem alloc]
                initWithIdentifier:[NSNumber numberWithUnsignedLong:index]];
            [item setLabel:native_string(get_item(index).get_title())];
            NSView *placeholder = [[NSView alloc]
                initWithFrame:[state->view contentRect]];
            [item setView:placeholder];
            [placeholder release];
            [state->view addTabViewItem:item];
            [item release];
        }
        state->suppress = false;
        [state->view setNeedsDisplay:YES];
    }

    void tab_view::apply_selected_index() {
        auto *state = binding(*this);
        if (!state || !state->view)
            throw std::runtime_error("macOS: missing tab-view binding.");
        if (get_selected_index() < 0)
            return;
        state->suppress = true;
        [state->view selectTabViewItemAtIndex:get_selected_index()];
        state->suppress = false;
    }

    void tab_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: tab_view requires a created parent.");
        NSTabView *view = [[NSTabView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [view setTabViewType:native_placement(get_tab_placement())];
        native_tab_delegate *delegate = [[native_tab_delegate alloc] init];
        delegate->_owner = self;
        [view setDelegate:delegate];
        [parent addSubview:view];
        native_tab_page_host *page_host = [[native_tab_page_host alloc]
            initWithFrame:[view contentRect]];
        page_host->_tabHeight = 0;
        [page_host setAutoresizingMask:
            NSViewWidthSizable | NSViewHeightSizable];
        [view addSubview:page_host positioned:NSWindowAbove relativeTo:nil];
        [page_host release];
        auto *state = new mac::mac_tab_view();
        state->view = view;
        state->page_host = page_host;
        state->delegate = delegate;
        mac::tab_view_bindings.register_pair(self, state);
        apply_placement(*self, *state);
        _created = true;
        self->synchronize_theme_metrics();
        self->configure_page_host(true, false);
        const NSRect content = [view contentRect];
        self->_tab_height = std::max(
            1,
            static_cast<int>(
                get_tab_placement() == tab_placement::left ||
                        get_tab_placement() == tab_placement::right
                    ? _bounds.d.w - content.size.width
                    : _bounds.d.h - content.size.height));
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        auto *state = binding(*const_cast<tab_view *>(this));
        if (!_created || !state || !state->view)
            throw std::runtime_error("macOS: tab_view is not created.");
        [state->view setHidden:NO];
        const int selected = get_selected_index();
        if (selected >= 0) {
            wnd &content = get_item(
                static_cast<std::size_t>(selected)).get_content();
            if (content.get_created())
                content.show();
        }
    }

    void tab_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state) {
            [state->view setDelegate:nil];
            [state->view removeFromSuperview];
            [state->view release];
            [state->delegate release];
            mac::tab_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

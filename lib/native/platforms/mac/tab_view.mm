//
// Implements tabs using NSTabView and each NSTabViewItem's own page view.
// AppKit owns tab painting, hit testing, placement and page geometry.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include "globals.h"

@interface native_tab_page_host : NSView {
@public
    native::wnd *_content;
}
- (void)fitContent;
@end

@implementation native_tab_page_host
- (BOOL)isFlipped { return YES; }
- (void)fitContent {
    if (_content && _content->get_created()) {
        const NSSize size = [self bounds].size;
        _content->set_bounds(native::rect(0, 0,
            static_cast<native::dim>(std::clamp<CGFloat>(size.width, 0, 65535)),
            static_cast<native::dim>(std::clamp<CGFloat>(size.height, 0, 65535))));
    }
}
- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
    [super resizeSubviewsWithOldSize:oldSize];
    [self fitContent];
}
@end

@interface native_tab_delegate : NSObject <NSTabViewDelegate> {
@public
    native::tab_view *_owner;
}
@end

@implementation native_tab_delegate
- (BOOL)tabView:(NSTabView *)view
    shouldSelectTabViewItem:(NSTabViewItem *)item {
    const NSInteger index = [view indexOfTabViewItem:item];
    return _owner && index >= 0 &&
        _owner->get_item(static_cast<std::size_t>(index)).get_enabled();
}
- (void)tabView:(NSTabView *)view
    didSelectTabViewItem:(NSTabViewItem *)item {
    auto *state = _owner
        ? mac::tab_view_bindings.object_from_handle(_owner) : nullptr;
    if (!_owner || !state || state->suppress) return;
    const NSInteger index = [view indexOfTabViewItem:item];
    if (index >= 0)
        _owner->on_native_selection(static_cast<int>(index));
}
@end

namespace
{
    NSTabViewType native_placement(native::tab_placement placement) {
        switch (placement) {
        case native::tab_placement::bottom: return NSBottomTabsBezelBorder;
        case native::tab_placement::left: return NSLeftTabsBezelBorder;
        case native::tab_placement::right: return NSRightTabsBezelBorder;
        default: return NSTopTabsBezelBorder;
        }
    }

    mac::mac_tab_view &binding(native::tab_view &owner) {
        auto *state = mac::tab_view_bindings.object_from_handle(&owner);
        if (!state || !state->view)
            throw std::runtime_error("macOS: missing tab-view binding.");
        return *state;
    }
}

namespace native
{
    void tab_view::apply_items() {
        auto &state = binding(*this);
        state.suppress = true;
        [state.view setTabViewType:native_placement(get_tab_placement())];
        [state.view setDrawsBackground:get_page_frame_visible()];
        // Keep old hosts alive until existing portable pages are moved.
        NSArray *previous = [[state.view tabViewItems] copy];
        for (NSTabViewItem *item in previous)
            [state.view removeTabViewItem:item];
        for (std::size_t index = 0; index < get_item_count(); ++index) {
            auto &content = get_item(index).get_content();
            NSTabViewItem *item = [[NSTabViewItem alloc]
                initWithIdentifier:[NSNumber numberWithUnsignedLong:index]];
            NSString *title = [NSString stringWithUTF8String:
                get_item(index).get_title().c_str()];
            [item setLabel:title ? title : @""];
            native_tab_page_host *host = [[native_tab_page_host alloc]
                initWithFrame:[state.view contentRect]];
            host->_content = &content;
            [host setAutoresizesSubviews:YES];
            [host setClipsToBounds:YES];
            [item setView:host];
            [state.view addTabViewItem:item];
            if (NSView *child = mac::view_from_control(&content))
                [host addSubview:child];
            [host fitContent];
            [host release];
            [item release];
        }
        [previous release];
        state.suppress = false;
    }

    void tab_view::apply_selected_index() {
        auto &state = binding(*this);
        if (get_selected_index() < 0) return;
        state.suppress = true;
        [state.view selectTabViewItemAtIndex:get_selected_index()];
        auto *host = static_cast<native_tab_page_host *>(
            [[state.view selectedTabViewItem] view]);
        [host fitContent];
        state.suppress = false;
    }

    void tab_view::create_native() {
        NSView *parent = mac::parent_view(get_parent(), this);
        if (!parent)
            throw std::runtime_error("macOS: tabs require a created parent.");
        auto *state = new mac::mac_tab_view();
        state->view = [[NSTabView alloc] initWithFrame:NSMakeRect(
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h)];
        native_tab_delegate *delegate = [[native_tab_delegate alloc] init];
        delegate->_owner = this;
        state->delegate = delegate;
        [state->view setDelegate:delegate];
        [parent addSubview:state->view];
        mac::tab_view_bindings.register_pair(this, state);
        configure_page_host(true, false);
        apply_items();
        refresh_contents();
        apply_selected_index();
    }

    void tab_view::show_native() {
        auto &state = binding(*this);
        [state.view setHidden:NO];
        refresh_contents();
        apply_selected_index();
        if (get_selected_index() >= 0)
            get_item(get_selected_index()).get_content().show();
    }

    void tab_view::destroy_native() {
        auto *state = mac::tab_view_bindings.object_from_handle(this);
        if (!state) return;
        [state->view setDelegate:nil];
        [state->view removeFromSuperview];
        [state->view release];
        [state->delegate release];
        mac::tab_view_bindings.unregister_by_handle(this);
        delete state;
    }
}

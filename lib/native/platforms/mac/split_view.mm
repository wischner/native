//
// Implements split_view with AppKit's native NSSplitView. AppKit owns
// divider drawing and tracking; borrowed controls fill its actual panes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>
#include <native.h>

#include "globals.h"

@interface native_split_pane : NSView {
@public
    native::wnd *_content;
}
@end
@implementation native_split_pane
- (BOOL)isFlipped { return YES; }
- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
    [super resizeSubviewsWithOldSize:oldSize];
    if (_content && _content->get_created()) {
        const NSSize size = [self bounds].size;
        _content->set_bounds(native::rect(0, 0,
            static_cast<native::dim>(std::clamp<CGFloat>(size.width, 0, 65535)),
            static_cast<native::dim>(std::clamp<CGFloat>(size.height, 0, 65535))));
    }
}
@end

@interface native_split_delegate : NSObject <NSSplitViewDelegate> {
@public
    void *_owner;
}
@end

@implementation native_split_delegate
- (void)splitViewDidResizeSubviews:(NSNotification *)notification {
    auto *owner = static_cast<native::split_view *>(_owner);
    auto *state = owner
        ? mac::split_view_bindings.object_from_handle(owner)
        : nullptr;
    (void)notification;
    if (!owner || !state || state->suppress || !owner->get_created()) return;
    NSRect first = [state->first frame];
    NSRect whole = [state->view bounds];
    const CGFloat available = std::max<CGFloat>(
        1, (owner->get_orientation() == native::split_orientation::horizontal
                ? whole.size.width : whole.size.height) -
               [state->view dividerThickness]);
    const CGFloat extent = owner->get_orientation() ==
                                   native::split_orientation::horizontal
                               ? first.size.width : first.size.height;
    owner->on_native_ratio(static_cast<float>(extent / available));
}

- (CGFloat)splitView:(NSSplitView *)split
    constrainMinCoordinate:(CGFloat)proposed
    ofSubviewAt:(NSInteger)index {
    auto *owner = static_cast<native::split_view *>(_owner);
    if (index != 0 || !owner) return proposed;
    const NSSize size = [split bounds].size;
    const CGFloat available = std::max<CGFloat>(0,
        ([split isVertical] ? size.width : size.height) - [split dividerThickness]);
    const CGFloat minimum = std::min<CGFloat>(available, owner->get_first_minimum());
    return std::clamp(proposed, minimum, available);
}

- (CGFloat)splitView:(NSSplitView *)split
    constrainMaxCoordinate:(CGFloat)proposed
    ofSubviewAt:(NSInteger)index {
    auto *owner = static_cast<native::split_view *>(_owner);
    if (index != 0 || !owner) return proposed;
    const NSRect bounds = [split bounds];
    const CGFloat total = owner->get_orientation() ==
                                  native::split_orientation::horizontal
                              ? bounds.size.width : bounds.size.height;
    const CGFloat available = std::max<CGFloat>(0, total - [split dividerThickness]);
    const CGFloat first = std::min<CGFloat>(available, owner->get_first_minimum());
    const CGFloat second = std::min<CGFloat>(available - first, owner->get_second_minimum());
    return std::clamp(proposed, first, available - second);
}
@end

namespace
{
    mac::mac_split_view *binding(native::split_view &owner) {
        return mac::split_view_bindings.object_from_handle(&owner);
    }
}

namespace native
{
    void split_view::apply_orientation() {
        auto *state = binding(*this);
        if (!state || !state->view) return;
        [state->view setVertical:
            get_orientation() == split_orientation::horizontal];
        apply_ratio();
    }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state || !state->view) return;
        const int extent = get_orientation() == split_orientation::horizontal
                               ? get_first_bounds().d.w
                               : get_first_bounds().d.h;
        state->suppress = true;
        [state->view adjustSubviews];
        [state->view setPosition:extent ofDividerAtIndex:0];
        state->suppress = false;
    }

    void split_view::apply_minimums() { apply_ratio(); }
    void split_view::apply_splitter_size() {
        auto *state = binding(*this);
        if (!state) return;
        [state->view setDividerStyle:get_splitter_size() <= 1
            ? NSSplitViewDividerStyleThin : NSSplitViewDividerStyleThick];
        _splitter_size = static_cast<dim>([state->view dividerThickness]);
        apply_ratio();
    }

    void split_view::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: split_view requires a created parent.");
        auto *state = new mac::mac_split_view();
        state->view = [[NSSplitView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x, _bounds.p.y,
                                     _bounds.d.w, _bounds.d.h)];
        [state->view setVertical:
            get_orientation() == split_orientation::horizontal];
        [state->view setDividerStyle:get_splitter_size() <= 1
            ? NSSplitViewDividerStyleThin : NSSplitViewDividerStyleThick];
        self->_splitter_size = static_cast<dim>([state->view dividerThickness]);
        state->first = [[native_split_pane alloc]
            initWithFrame:NSMakeRect(0, 0, _bounds.d.w / 2, _bounds.d.h)];
        state->second = [[native_split_pane alloc]
            initWithFrame:NSMakeRect(0, 0, _bounds.d.w / 2, _bounds.d.h)];
        static_cast<native_split_pane *>(state->first)->_content = &get_first();
        static_cast<native_split_pane *>(state->second)->_content = &get_second();
        [state->first setClipsToBounds:YES];
        [state->second setClipsToBounds:YES];
        [state->view addSubview:state->first];
        [state->view addSubview:state->second];
        native_split_delegate *delegate = [[native_split_delegate alloc] init];
        delegate->_owner = self;
        state->delegate = delegate;
        [state->view setDelegate:delegate];
        [parent addSubview:state->view];
        mac::split_view_bindings.register_pair(self, state);
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_ratio();
    }

    void split_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state || !state->view)
            throw std::runtime_error("macOS: split_view is not created.");
        [state->view setHidden:NO];
        get_first().show();
        get_second().show();
    }

    void split_view::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = binding(*self);
        if (state) {
            [state->view setDelegate:nil];
            [state->view removeFromSuperview];
            [state->view release];
            [state->first release];
            [state->second release];
            [state->delegate release];
            mac::split_view_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

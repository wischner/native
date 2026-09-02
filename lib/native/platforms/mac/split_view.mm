// Implements split_view with AppKit's native NSSplitView.

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>
#include <native.h>

#include "globals.h"

@interface native_split_pane : NSView
@end
@implementation native_split_pane
- (BOOL)isFlipped { return YES; }
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
    if (!owner || !state || state->suppress) return;
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
    return index == 0 && owner
        ? std::max(proposed, static_cast<CGFloat>(owner->get_first_minimum()))
        : proposed;
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
    return std::min(proposed,
                    total - [split dividerThickness] -
                        owner->get_second_minimum());
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
        [state->view setPosition:extent ofDividerAtIndex:0];
        state->suppress = false;
    }

    void split_view::apply_minimums() { apply_ratio(); }
    void split_view::apply_splitter_size() { apply_ratio(); }

    void split_view::create() const {
        if (_created) return;
        auto *self = const_cast<split_view *>(this);
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
        [state->view setDividerStyle:NSSplitViewDividerStyleThin];
        state->first = [[native_split_pane alloc]
            initWithFrame:NSMakeRect(0, 0, 1, 1)];
        state->second = [[native_split_pane alloc]
            initWithFrame:NSMakeRect(0, 0, 1, 1)];
        [state->view addSubview:state->first];
        [state->view addSubview:state->second];
        native_split_delegate *delegate = [[native_split_delegate alloc] init];
        delegate->_owner = self;
        state->delegate = delegate;
        [state->view setDelegate:delegate];
        [parent addSubview:state->view];
        mac::split_view_bindings.register_pair(self, state);
        _created = true;
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_ratio();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = binding(*const_cast<split_view *>(this));
        if (!_created || !state || !state->view)
            throw std::runtime_error("macOS: split_view is not created.");
        [state->view setHidden:NO];
        get_first().show();
        get_second().show();
    }

    void split_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<split_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
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

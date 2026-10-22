//
// Uses flexible-height native flat buttons for portable edge toolbars. NSToolbar
// owns a window's titlebar; these bars can occupy any edge, several at once.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "native_cells.h"
#include "../../toolbar_peer.h"
#include <typeinfo>
#include <algorithm>

@interface native_tool_target : NSObject {
@public
    native::toolbar *_owner;
}
- (void)activate:(NSButton *)sender;
@end
@implementation native_tool_target
- (void)activate:(NSButton *)sender {
    if (!_owner) return;
    const auto id = std::string([[sender identifier] UTF8String]);
    // A selected exclusive tool rejects a second activation. AppKit has
    // already toggled its push-on/off cell, so restore the model's state.
    if (!_owner->on_native_command(id))
        [sender setState:_owner->get_checked(id) ? NSControlStateValueOn : NSControlStateValueOff];
}
@end

namespace
{
    struct toolbar_state
    {
        native_tool_target *target = nil;
        std::vector<NSView *> controls;
        std::vector<native::tool_item> items;
        native::size icon_size;
        ~toolbar_state() {
            if (target) target->_owner = nullptr;
            clear(); [target release];
        }
        void clear() {
            for (auto *view : controls) { [view removeFromSuperview]; [view release]; }
            controls.clear();
        }
    };
    bool same_items(const toolbar_state &peer, const native::toolbar &owner) {
        const auto &items = owner.get_items();
        if (peer.items.size() != items.size() || (peer.icon_size.w != owner.get_icon_size().w || peer.icon_size.h != owner.get_icon_size().h)) return false;
        for (std::size_t index = 0; index < items.size(); ++index)
            if (peer.items[index].id != items[index].id || peer.items[index].text != items[index].text ||
                peer.items[index].kind != items[index].kind || peer.items[index].icon != items[index].icon)
                return false;
        return true;
    }
}
namespace native::detail
{
    bool toolbar_peer_active(const toolbar &owner, const canvas &surface) {
        return typeid(owner) == typeid(toolbar) && peer_state<toolbar_state>(surface);
    }
    bool refresh_toolbar_peer(toolbar &owner, canvas &surface, const std::vector<rect> &bounds) {
        if (typeid(owner) != typeid(toolbar) || !surface.get_created()) return false;
        auto *peer = peer_state<toolbar_state>(surface);
        if (!peer) {
            peer = new toolbar_state;
            peer->target = [[native_tool_target alloc] init];
            peer->target->_owner = &owner;
            assign_peer_state(surface, peer);
        }
        const auto &items = owner.get_items();
        if (!same_items(*peer, owner)) {
            peer->clear(); peer->items = items; peer->icon_size = owner.get_icon_size();
            auto *host = mac::canvas_bindings.object_from_handle(&surface)->view;
            for (const auto &item : items) {
                NSView *view;
                if (item.kind == tool_kind::separator) {
                    auto *separator = [[NSBox alloc] initWithFrame:NSZeroRect];
                    [separator setBoxType:NSBoxSeparator]; view = separator;
                } else {
                    auto *button = [[NSButton alloc] initWithFrame:NSZeroRect];
                    [button setTitle:[NSString stringWithUTF8String:item.text.c_str()]];
                    [button setIdentifier:[NSString stringWithUTF8String:item.id.c_str()]];
                    [button setToolTip:[button title]];
                    [button setBezelStyle:NSBezelStyleFlexiblePush];
                    [button setShowsBorderOnlyWhileMouseInside:YES];
                    [button setButtonType:item.kind == tool_kind::button ? NSButtonTypeMomentaryPushIn : NSButtonTypePushOnPushOff];
                    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSControlSizeRegular]]];
                    [button setImagePosition:item.text.empty() ? NSImageOnly : NSImageLeft];
                    auto *image = mac::cell_image(item.icon.get());
                    [image setSize:NSMakeSize(peer->icon_size.w, peer->icon_size.h)];
                    if (item.icon) {
                        const auto *pixels = item.icon->pixels();
                        const auto count = std::size_t(item.icon->w()) * item.icon->h();
                        [image setTemplate:std::all_of(pixels, pixels + count,
                            [](rgba p) { return !p.a || (!p.r && !p.g && !p.b); })];
                    }
                    [button setImage:image];
                    [button setTarget:peer->target]; [button setAction:@selector(activate:)];
                    view = button;
                }
                [host addSubview:view]; peer->controls.push_back(view);
            }
        }
        const bool vertical = owner.get_edge() == window_edge::left || owner.get_edge() == window_edge::right;
        CGFloat offset = 2;
        const auto dimensions = surface.get_dimensions();
        for (std::size_t index = 0; index < items.size(); ++index) {
            const auto &r = bounds[index];
            NSRect frame = NSMakeRect(r.x1(), r.y1(), r.w(), r.h());
            CGFloat length = vertical ? r.h() : r.w();
            if (items[index].kind != tool_kind::separator) {
                const NSSize preferred = [[(NSButton *)peer->controls[index] cell] cellSize];
                length = std::max<CGFloat>(length, vertical ? preferred.height : preferred.width);
            }
            const CGFloat available = std::max<CGFloat>(0, (vertical ? dimensions.h : dimensions.w) - 2 - offset);
            if (vertical) { frame.origin.y = offset; frame.size.height = std::min(length, available); }
            else { frame.origin.x = offset; frame.size.width = std::min(length, available); }
            offset += length;
            if (items[index].kind == tool_kind::separator) {
                if (vertical) { frame.origin.y += frame.size.height / 2; frame.size.height = 1; }
                else { frame.origin.x += frame.size.width / 2; frame.size.width = 1; }
            } else {
                auto *button = (NSButton *)peer->controls[index];
                [button setEnabled:items[index].enabled && surface.get_input_enabled()];
                [button setState:items[index].checked ? NSControlStateValueOn : NSControlStateValueOff];
                [button setShowsBorderOnlyWhileMouseInside:!items[index].checked];
                // Allocate the bezel's alignment rectangle, including native
                // shadow/focus margins outside it. Its height follows icons.
                frame = [button frameForAlignmentRect:frame];
            }
            [peer->controls[index] setFrame:frame];
        }
        return true;
    }
}

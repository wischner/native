//
// Implements accordion with an AppKit stack and disclosure buttons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>
#include <typeinfo>

#include <native.h>

#include "globals.h"
#include "native_cells.h"

@interface native_accordion_stack : NSStackView {
@public
    void *_owner;
}
@end

@implementation native_accordion_stack
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    auto *owner = static_cast<native::accordion *>(_owner);
    if (!owner || !owner->get_created() ||
        typeid(*owner) == typeid(native::accordion))
        return;
    native::gpx &graphics = owner->get_gpx();
    native::rect invalid(
        static_cast<native::coord>(dirty.origin.x),
        static_cast<native::coord>(dirty.origin.y),
        static_cast<native::dim>(
            std::max<CGFloat>(0, dirty.size.width)),
        static_cast<native::dim>(
            std::max<CGFloat>(0, dirty.size.height)));
    graphics.set_clip(invalid);
    owner->on_native_paint(native::wnd_paint_event(invalid, graphics));
}
@end

@interface native_accordion_target : NSObject {
@public
    void *_owner;
}
- (void)toggle:(id)sender;
@end

@implementation native_accordion_target
- (void)toggle:(id)sender {
    auto *owner = static_cast<native::accordion *>(_owner);
    if (owner)
        owner->on_native_toggle(
            static_cast<std::size_t>([sender tag]));
}
@end

namespace
{
    // Stack views need an explicit height for borrowed scroll views, which
    // have no intrinsic content size. Update, rather than accumulate, it.
    void fit_height(NSView *view, CGFloat height) {
        [view setTranslatesAutoresizingMaskIntoConstraints:NO];
        for (NSLayoutConstraint *constraint in [view constraints]) {
            if ([[constraint identifier] isEqualToString:@"native_height"]) {
                [constraint setConstant:height];
                return;
            }
        }
        NSLayoutConstraint *constraint = [[view heightAnchor]
            constraintEqualToConstant:height];
        [constraint setIdentifier:@"native_height"];
        [constraint setActive:YES];
    }

    NSString *native_string(const std::string &value) {
        NSString *result = [NSString stringWithUTF8String:value.c_str()];
        return result ? result : @"";
    }

    NSImage *native_image(const native::img &source) {
        return [mac::cell_image(&source) retain];
    }

    void rebuild(native::accordion &control) {
        auto *binding =
            mac::accordion_bindings.object_from_handle(&control);
        if (!binding || !binding->stack)
            throw std::runtime_error(
                "macOS: missing accordion binding.");

        NSArray<NSView *> *views =
            [[binding->stack arrangedSubviews] copy];
        for (NSView *view in views) {
            [binding->stack removeArrangedSubview:view];
            [view removeFromSuperview];
        }
        [views release];
        for (NSButton *header : binding->headers)
            [header release];
        binding->headers.clear();
        [binding->scroll setBorderType:control.get_border_visible()
            ? NSBezelBorder : NSNoBorder];
        const CGFloat header_height = control.get_item_count()
            ? control.get_header_bounds(0).d.h : 0;
        const CGFloat headers_height = header_height * control.get_item_count();
        const bool single = control.get_mode() == native::accordion_mode::single;
        // A single expanded page owns scrolling. The outer viewport only
        // needs a scroller when its headers themselves cannot fit.
        [binding->scroll setHasVerticalScroller:!single || headers_height >
            NSHeight([[binding->scroll contentView] bounds])];
        [binding->scroll tile];
        const CGFloat single_height = std::max<CGFloat>(0,
            NSHeight([[binding->scroll contentView] bounds]) - headers_height);

        for (std::size_t index = 0;
             index < control.get_item_count();
             ++index) {
            native::accordion_item &item = control.get_item(index);
            NSButton *header = [[NSButton alloc]
                initWithFrame:NSMakeRect(
                                  0,
                                  0,
                                  control.get_dimensions().w,
                                  control.get_header_bounds(index).d.h)];
            [header setTitle:@""];
            [header setButtonType:NSButtonTypePushOnPushOff];
            [header setBezelStyle:NSBezelStyleDisclosure];
            [header setState:item.get_expanded() ? NSControlStateValueOn
                                                 : NSControlStateValueOff];
            [header setEnabled:item.get_enabled() ? YES : NO];
            [header setTag:static_cast<NSInteger>(index)];
            [header setTarget:binding->target];
            [header setAction:@selector(toggle:)];
            [header setAlignment:NSTextAlignmentLeft];
            NSStackView *row = [[NSStackView alloc] initWithFrame:NSZeroRect];
            [row setOrientation:NSUserInterfaceLayoutOrientationHorizontal];
            [row setAlignment:NSLayoutAttributeCenterY];
            [row setSpacing:4];
            [row addArrangedSubview:header];
            if (const native::img *icon = item.get_icon()) {
                NSImage *image = native_image(*icon);
                NSImageView *image_view = [[NSImageView alloc]
                    initWithFrame:NSMakeRect(0, 0, 16, 16)];
                [image_view setImage:image];
                [image_view setImageScaling:NSImageScaleProportionallyDown];
                [row addArrangedSubview:image_view];
                [[[image_view widthAnchor] constraintEqualToConstant:16] setActive:YES];
                fit_height(image_view, 16);
                [image_view release];
                [image release];
            }
            NSTextField *label = [NSTextField labelWithString:
                native_string(item.get_title())];
            [label setTextColor:item.get_enabled() ? [NSColor controlTextColor]
                : [NSColor disabledControlTextColor]];
            [row addArrangedSubview:label];
            [binding->stack addArrangedSubview:row];
            fit_height(row, control.get_header_bounds(index).d.h);
            [[[row widthAnchor] constraintEqualToAnchor:
                [binding->stack widthAnchor]] setActive:YES];
            [row release];
            binding->headers.push_back(header);

            if (!item.get_expanded())
                continue;
            NSView *content =
                mac::view_from_control(&item.get_content());
            if (content) {
                const native::rect body =
                    control.get_content_bounds(index);
                [binding->stack addArrangedSubview:content];
                fit_height(content, single ? single_height : body.d.h);
                [[[content widthAnchor] constraintEqualToAnchor:
                    [binding->stack widthAnchor]] setActive:YES];
            }
        }
        [binding->stack setNeedsLayout:YES];
        [binding->stack setNeedsDisplay:YES];
    }
} // namespace

namespace native
{
    void accordion::apply_items() { rebuild(*this); }

    void accordion::create_native() {
        auto *self = this;
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: accordion requires a created parent.");
        NSScrollView *scroll = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [scroll setHasVerticalScroller:YES];
        [scroll setAutohidesScrollers:YES];
        native_accordion_stack *stack = [[native_accordion_stack alloc]
            initWithFrame:NSMakeRect(0, 0, _bounds.d.w, _bounds.d.h)];
        stack->_owner = self;
        [stack setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [stack setAlignment:NSLayoutAttributeLeading];
        [stack setDistribution:NSStackViewDistributionFill];
        [stack setSpacing:0];
        [stack setTranslatesAutoresizingMaskIntoConstraints:NO];
        [scroll setDocumentView:stack];
        [[[stack widthAnchor] constraintEqualToAnchor:
            [[scroll contentView] widthAnchor]] setActive:YES];
        [parent addSubview:scroll];
        native_accordion_target *target =
            [[native_accordion_target alloc] init];
        target->_owner = self;
        auto *binding = new mac::mac_accordion();
        binding->scroll = scroll;
        binding->stack = stack;
        binding->target = target;
        mac::accordion_bindings.register_pair(self, binding);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void accordion::show_native() {
        auto *binding = mac::accordion_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->stack)
            throw std::runtime_error(
                "macOS: accordion is not created.");
        [binding->scroll setHidden:NO];
    }

    void accordion::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            mac::accordion_bindings.object_from_handle(self);
        if (binding) {
            for (NSButton *header : binding->headers)
                [header release];
            [binding->scroll removeFromSuperview];
            [binding->scroll setDocumentView:nil];
            [binding->stack release];
            [binding->scroll release];
            [binding->target release];
            mac::accordion_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

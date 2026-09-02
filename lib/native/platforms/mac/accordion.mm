//
// Implements accordion with an AppKit stack and disclosure buttons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <algorithm>
#include <stdexcept>

#include <native.h>

#include "globals.h"

@interface native_accordion_stack : NSStackView {
@public
    void *_owner;
}
@end

@implementation native_accordion_stack
- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    auto *owner = static_cast<native::accordion *>(_owner);
    if (!owner || !owner->get_created())
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
            [header setTitle:native_string(item.get_title())];
            [header setButtonType:NSButtonTypePushOnPushOff];
            [header setBezelStyle:NSBezelStyleDisclosure];
            [header setState:item.get_expanded() ? NSControlStateValueOn
                                                 : NSControlStateValueOff];
            [header setEnabled:item.get_enabled() ? YES : NO];
            [header setTag:static_cast<NSInteger>(index)];
            [header setTarget:binding->target];
            [header setAction:@selector(toggle:)];
            [header setAlignment:NSTextAlignmentLeft];
            if (const native::img *icon = item.get_icon()) {
                NSImage *image = native_image(*icon);
                [header setImage:image];
                [header setImagePosition:NSImageLeft];
                [header setImageScaling:
                            NSImageScaleProportionallyDown];
                [image release];
            }
            [binding->stack addArrangedSubview:header];
            [header setFrameSize:NSMakeSize(
                                     control.get_dimensions().w,
                                     control.get_header_bounds(index).d.h)];
            binding->headers.push_back(header);

            if (!item.get_expanded())
                continue;
            NSView *content =
                mac::view_from_control(&item.get_content());
            if (content) {
                const native::rect body =
                    control.get_content_bounds(index);
                [content setFrameSize:NSMakeSize(body.d.w, body.d.h)];
                [binding->stack addArrangedSubview:content];
            }
        }
        [binding->stack setNeedsLayout:YES];
        [binding->stack setNeedsDisplay:YES];
    }
} // namespace

namespace native
{
    void accordion::apply_items() { rebuild(*this); }

    void accordion::create() const {
        if (_created)
            return;
        auto *self = const_cast<accordion *>(this);
        NSView *parent = mac::parent_view(get_parent(), self);
        if (!parent)
            throw std::runtime_error(
                "macOS: accordion requires a created parent.");
        native_accordion_stack *stack = [[native_accordion_stack alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        stack->_owner = self;
        [stack setOrientation:NSUserInterfaceLayoutOrientationVertical];
        [stack setAlignment:NSLayoutAttributeLeading];
        [stack setDistribution:NSStackViewDistributionFill];
        [stack setSpacing:0];
        [parent addSubview:stack];
        native_accordion_target *target =
            [[native_accordion_target alloc] init];
        target->_owner = self;
        auto *binding = new mac::mac_accordion();
        binding->stack = stack;
        binding->target = target;
        mac::accordion_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void accordion::show() const {
        auto *binding = mac::accordion_bindings.object_from_handle(
            const_cast<accordion *>(this));
        if (!_created || !binding || !binding->stack)
            throw std::runtime_error(
                "macOS: accordion is not created.");
        NSView *parent = [binding->stack superview];
        if (parent) {
            [parent addSubview:binding->stack
                    positioned:NSWindowAbove
                    relativeTo:nil];
        }
        [binding->stack setHidden:NO];
    }

    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *binding =
            mac::accordion_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            for (NSButton *header : binding->headers)
                [header release];
            [binding->stack removeFromSuperview];
            [binding->stack release];
            [binding->target release];
            mac::accordion_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

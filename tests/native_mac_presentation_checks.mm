//
// Verifies macOS native theme parity, pointer cursors, ruler motion and
// composed property-dialog chrome using the actual AppKit controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#include <native.h>
#include "../src/vision_window.h"
#include "../lib/native/platforms/mac/globals.h"
#include <cmath>
#include <stdexcept>

namespace
{
    void expect(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }

    void attach(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }

    NSBitmapImageRep *render(NSView *view) {
        [view layoutSubtreeIfNeeded];
        NSBitmapImageRep *bitmap = [view
            bitmapImageRepForCachingDisplayInRect:[view bounds]];
        expect(bitmap != nil, "AppKit allocates a rendering target");
        [view cacheDisplayInRect:[view bounds] toBitmapImageRep:bitmap];
        return bitmap;
    }

    // cacheDisplayInRect changes control appearance for cached display.
    // Use the stock NSButton drawRect path as the live-painting reference.
    NSBitmapImageRep *render_control(NSButton *button) {
        const NSSize size = [button bounds].size;
        NSBitmapImageRep *bitmap = [[[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:nullptr pixelsWide:size.width pixelsHigh:size.height
            bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
            colorSpaceName:NSCalibratedRGBColorSpace
            bitmapFormat:0
            bytesPerRow:size.width * 4 bitsPerPixel:32] autorelease];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:
            [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
        [[NSColor whiteColor] setFill];
        NSRectFill([button bounds]);
        [[button effectiveAppearance] performAsCurrentDrawingAppearance:^{
            [button drawRect:[button bounds]];
        }];
        [NSGraphicsContext restoreGraphicsState];
        return bitmap;
    }

    void selection_theme_checks(native::app_wnd &root) {
        native::check check("Native", 10, 10, 140, 24);
        native::radio radio("Native", 10, 40, 140, 24);
        attach(check, root);
        attach(radio, root);
        native::button push("Native", 10, 70, 140, 24);
        attach(push, root);
        native::canvas surface(180, 10, 140, 24);
        native::theme::state primitive_state;
        int primitive_kind = 0;
        surface.on_wnd_paint.connect([&](native::wnd_paint_event event) {
            event.g.clear(native::rgba(255, 255, 255, 255));
            auto theme = native::theme::create(event.g);
            if (primitive_kind == 2)
                theme->draw_button({0, 0, 140, 24}, "Native", primitive_state);
            else if (primitive_kind == 1)
                theme->draw_radio({0, 0, 140, 24}, "Native", primitive_state);
            else theme->draw_check({0, 0, 140, 24}, "Native", primitive_state);
            return true;
        });
        attach(surface, root);
        NSWindow *root_window = mac::wnd_bindings.handle_from_object(&root);
        NSPanel *utility = [[NSPanel alloc] initWithContentRect:
            NSMakeRect(0, 0, 100, 100) styleMask:NSWindowStyleMaskTitled |
            NSWindowStyleMaskUtilityWindow backing:NSBackingStoreBuffered defer:YES];
        [utility setReleasedWhenClosed:NO];
        [root_window makeMainWindow];
        [utility makeKeyAndOrderFront:nil];
        for (bool dark : {false, true}) {
            NSAppearance *appearance = [NSAppearance appearanceNamed:
                dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
            NSWindow *window = mac::wnd_bindings.handle_from_object(&root);
            [window setAppearance:appearance];
            for (int kind : {0, 1, 2}) {
                NSButton *button = kind == 2
                    ? mac::button_bindings.object_from_handle(&push)->ns_button
                    : kind == 1 ? mac::radio_bindings.object_from_handle(&radio)->button
                                : mac::check_bindings.object_from_handle(&check)->button;
                for (int variant = 0; variant < 4; ++variant) {
                    const bool selected = variant & 1;
                    const bool disabled = variant & 2;
                    [button setEnabled:!disabled];
                    [button setState:selected ? NSControlStateValueOn
                                              : NSControlStateValueOff];
                    NSBitmapImageRep *reference = render_control(button);
                    primitive_state.selected = selected;
                    primitive_state.disabled = disabled;
                    primitive_kind = kind;
                    NSBitmapImageRep *window_pixels = render(mac::view_from_control(&surface));
                    native::img image(140, 24);
                    image.get_gpx().clear(native::rgba(255, 255, 255, 255));
                    auto *graphics = &image.get_gpx();
                    [appearance performAsCurrentDrawingAppearance:^{
                        auto theme = native::theme::create(*graphics);
                        native::theme::state state;
                        state.selected = selected;
                        state.disabled = disabled;
                        if (kind == 2) theme->draw_button({0, 0, 140, 24}, "Native", state);
                        else if (kind == 1) theme->draw_radio({0, 0, 140, 24}, "Native", state);
                        else theme->draw_check({0, 0, 140, 24}, "Native", state);
                    }];
                    double difference = 0;
                    double window_difference = 0;
                    for (int y = 0; y < 24; ++y)
                        for (int x = 0; x < 140; ++x) {
                            NSColor *color = [[reference colorAtX:x y:y]
                                colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                            const auto pixel = image.pixels()[y * 140 + x];
                            const double alpha = [color alphaComponent];
                            NSColor *painted = [[window_pixels colorAtX:x y:y]
                                colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                            window_difference += std::abs([painted redComponent] -
                                ([color redComponent] * alpha + 1 - alpha));
                            window_difference += std::abs([painted greenComponent] -
                                ([color greenComponent] * alpha + 1 - alpha));
                            window_difference += std::abs([painted blueComponent] -
                                ([color blueComponent] * alpha + 1 - alpha));
                            difference += std::abs(pixel.r / 255.0 -
                                ([color redComponent] * alpha + 1 - alpha));
                            difference += std::abs(pixel.g / 255.0 -
                                ([color greenComponent] * alpha + 1 - alpha));
                            difference += std::abs(pixel.b / 255.0 -
                                ([color blueComponent] * alpha + 1 - alpha));
                        }
                    expect(window_difference / (140 * 24 * 3) < 0.003,
                        ("window theme mismatch: kind=" + std::to_string(kind) +
                         " variant=" + std::to_string(variant) +
                         " dark=" + std::to_string(dark) +
                         " difference=" + std::to_string(window_difference / (140 * 24 * 3))).c_str());
                    expect(difference / (140 * 24 * 3) < 0.005,
                        ("image theme mismatch: kind=" + std::to_string(kind) +
                         " variant=" + std::to_string(variant) +
                         " dark=" + std::to_string(dark) +
                         " difference=" + std::to_string(difference / (140 * 24 * 3))).c_str());
                }
            }
            [window setAppearance:nil];
        }
        [utility close];
        [utility release];
        [root_window makeKeyAndOrderFront:nil];
    }

    // Deliver overlapping tracking callbacks with ancestors last, as AppKit
    // may do when a pointer crosses a native child control.
    void cursor_event(NSView *view, NSPoint window_point) {
        NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
            location:window_point modifierFlags:0 timestamp:0
            windowNumber:[[view window] windowNumber] context:nil
            eventNumber:0 clickCount:0 pressure:0];
        for (NSTrackingArea *area in [view trackingAreas]) {
            id target = [area owner];
            if ([target respondsToSelector:@selector(cursorUpdate:)])
                [target cursorUpdate:event];
        }
    }

    void ruler_motion_checks(native::app_wnd &root) {
        native::ruler horizontal(root, native::ruler_orientation::horizontal);
        native::ruler vertical(root, native::ruler_orientation::vertical);
        horizontal.set_track_mouse(true);
        vertical.set_track_mouse(true);
        NSWindow *window = mac::wnd_bindings.handle_from_object(&root);
        expect([window acceptsMouseMovedEvents],
            "AppKit window requests ordinary pointer motion without dragging");
        NSView *content = [window contentView];
        const native::point local(160, 140);
        const NSPoint position = [content convertPoint:
            NSMakePoint(local.x, local.y) toView:nil];
        NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
            location:position modifierFlags:0 timestamp:0
            windowNumber:[window windowNumber] context:nil eventNumber:0
            clickCount:0 pressure:0];
        // Pointer delivery refreshes the tracking areas, so retain this
        // snapshot of the callback targets for the duration of dispatch.
        NSArray *areas = [[content trackingAreas] copy];
        for (NSTrackingArea *area in areas) {
            id target = [area owner];
            if ([target respondsToSelector:@selector(mouseMoved:)])
                [target mouseMoved:event];
        }
        [areas release];
        expect(horizontal.get_tracked_value().has_value() &&
            vertical.get_tracked_value().has_value(),
            "ordinary window motion updates both ruler markers");
        expect(*horizontal.get_tracked_value() ==
                horizontal.get_origin() + (local.x - horizontal.get_bounds().x1()) *
                    horizontal.get_units_per_pixel() &&
            *vertical.get_tracked_value() ==
                vertical.get_origin() + (local.y - vertical.get_bounds().y1()) *
                    vertical.get_units_per_pixel(),
            "ruler marker values use window client coordinates");
    }

    void cursor_checks(native::app_wnd &root) {
        NSView *content = [mac::wnd_bindings.handle_from_object(&root) contentView];
        native::text_edit edit("Text", native::text_edit_mode::single_line,
            native::rect(100, 50, 150, 26));
        attach(edit, root);
        NSView *field = mac::view_from_control(&edit);
        NSPoint position = [field convertPoint:NSMakePoint(20, 12) toView:nil];
        [[NSCursor arrowCursor] set];
        cursor_event(field, position);
        cursor_event(content, position);
        expect([NSCursor currentCursor] == [NSCursor IBeamCursor],
            "parent tracking preserves the text field I-beam before editing");
        edit.destroy();
        native::panel first, second;
        native::split_view split(first, second,
            native::split_orientation::horizontal, 90, 70, 300, 150);
        attach(split, root);
        auto *peer = mac::split_view_bindings.object_from_handle(&split);
        NSRect pane = [peer->first frame];
        position = [peer->view convertPoint:NSMakePoint(
            NSMaxX(pane) + [peer->view dividerThickness] / 2, 60) toView:nil];
        [[NSCursor arrowCursor] set];
        cursor_event(peer->view, position);
        cursor_event(content, position);
        expect([NSCursor currentCursor] == [NSCursor resizeLeftRightCursor],
            "split divider advertises resize on hover without a mouse press");
    }

    void toolbar_checks(native::app_wnd &root) {
        vision::feature_properties dialog(root);
        dialog.create();
        dialog.show();
        NSView *dialog_content = [mac::wnd_bindings.handle_from_object(&dialog) contentView];
        NSWindow *window = mac::wnd_bindings.handle_from_object(&dialog);
        [window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
        auto *dialog_bitmap = render(dialog_content);
        int ink = 0;
        for (int y = 2; y < 22; ++y)
            for (int x = 2; x < 125; ++x) {
                NSColor *color = [[dialog_bitmap colorAtX:x y:y]
                    colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                if ([color redComponent] < 0.4) ++ink;
            }
        expect(ink > 40,
            "property panel repaint preserves sibling toolbar icons and labels");
        native::toolbar bar(root);
        auto icon = std::make_shared<native::img>(16, 16);
        icon->get_gpx().clear(native::rgba(240, 0, 0, 255));
        bar.set_items({{"test", "Tool", native::tool_kind::button,
            {}, false, true, icon}});
        NSView *content = [mac::wnd_bindings.handle_from_object(&root) contentView];
        bool found = false;
        for (NSView *view in [content subviews]) {
            const NSRect frame = [view frame];
            if (frame.origin.y != bar.get_bounds().p.y ||
                frame.size.height != bar.get_bounds().d.h) continue;
            auto *bitmap = render(view);
            int red = 0;
            for (NSInteger y = 0; y < [bitmap pixelsHigh]; ++y)
                for (NSInteger x = 0; x < [bitmap pixelsWide]; ++x) {
                    NSColor *color = [[bitmap colorAtX:x y:y]
                        colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                    if ([color redComponent] > 0.8 && [color greenComponent] < 0.2)
                        ++red;
                }
            expect(red > 20, "toolbar displays its icon on the native canvas");
            found = true;
        }
        expect(found, "toolbar creates a visible child surface");
    }

}

// Run presentation regressions in the application's AppKit UI thread.
void native_mac_presentation_checks(native::app_wnd &root) {
    toolbar_checks(root);
    selection_theme_checks(root);
    ruler_motion_checks(root);
    cursor_checks(root);
}

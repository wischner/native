//
// Verifies macOS native theme parity, pointer cursors, ruler motion and
// composed property-dialog chrome using the actual AppKit controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <native.h>
#include "../src/vision_window.h"
#include "../lib/native/platforms/mac/globals.h"
#include <algorithm>
#include <cmath>
#include <memory>
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

    // Include the native backing-layer mask, as the compositor does.
    NSBitmapImageRep *render_masked_control(NSButton *button) {
        [button setWantsLayer:YES];
        [button displayIfNeeded];
        [[button layer] displayIfNeeded];
        auto *bitmap = render_control(button);
        [NSGraphicsContext saveGraphicsState];
        auto *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
        [NSGraphicsContext setCurrentContext:context];
        [[NSColor whiteColor] setFill];
        NSRectFill([button bounds]);
        [[button layer] renderInContext:[context CGContext]];
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
            if (primitive_kind == 3)
                theme->draw_toolbar_button({0, 0, 140, 24}, "Native", primitive_state);
            else if (primitive_kind == 2)
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
        NSButton *accessory = [[NSButton alloc] initWithFrame:NSMakeRect(10, 100, 140, 24)];
        [accessory setButtonType:NSButtonTypePushOnPushOff];
        [accessory setBezelStyle:NSBezelStyleFlexiblePush];
        [accessory setShowsBorderOnlyWhileMouseInside:YES];
        [accessory setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
        [accessory setTitle:@"Native"];
        [[root_window contentView] addSubview:accessory];
        [root_window makeMainWindow];
        [utility makeKeyAndOrderFront:nil];
        for (bool dark : {false, true}) {
            NSAppearance *appearance = [NSAppearance appearanceNamed:
                dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
            NSWindow *window = mac::wnd_bindings.handle_from_object(&root);
            [window setAppearance:appearance];
            for (int kind : {0, 1, 2, 3}) {
                NSButton *button = kind == 3 ? accessory : kind == 2
                    ? mac::button_bindings.object_from_handle(&push)->ns_button
                    : kind == 1 ? mac::radio_bindings.object_from_handle(&radio)->button
                                : mac::check_bindings.object_from_handle(&check)->button;
                for (int variant = 0; variant < 4; ++variant) {
                    const bool selected = variant & 1;
                    const bool disabled = variant & 2;
                    [button setEnabled:!disabled];
                    [button setState:selected ? NSControlStateValueOn
                                              : NSControlStateValueOff];
                    if (kind == 3)
                        [button setShowsBorderOnlyWhileMouseInside:!selected];
                    NSBitmapImageRep *reference = render_control(button);
                    std::unique_ptr<native::canvas> reference_surface;
                    if (kind == 3) {
                        // AppKit changes flexible bezels' fill and idle text
                        // when caching a control directly. Paint the actual
                        // button into a sibling canvas so the native reference
                        // and primitive use the same destination context.
                        reference_surface = std::make_unique<native::canvas>(180, 70, 140, 24);
                        reference_surface->on_wnd_paint.connect([button](native::wnd_paint_event event) {
                            event.g.clear(native::rgba(255, 255, 255, 255));
                            [NSGraphicsContext saveGraphicsState];
                            auto *context = [[NSGraphicsContext currentContext] CGContext];
                            CGContextTranslateCTM(context, 0, 24);
                            CGContextScaleCTM(context, 1, -1);
                            [NSGraphicsContext setCurrentContext:[NSGraphicsContext
                                graphicsContextWithCGContext:context flipped:NO]];
                            [button drawRect:[button bounds]];
                            [NSGraphicsContext restoreGraphicsState];
                            return true;
                        });
                        attach(*reference_surface, root);
                        reference = render(mac::view_from_control(reference_surface.get()));
                    }
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
                        if (kind == 3) theme->draw_toolbar_button({0, 0, 140, 24}, "Native", state);
                        else if (kind == 2) theme->draw_button({0, 0, 140, 24}, "Native", state);
                        else if (kind == 1) theme->draw_radio({0, 0, 140, 24}, "Native", state);
                        else theme->draw_check({0, 0, 140, 24}, "Native", state);
                    }];
                    // Accessory labels use translucent semantic colors. Compare
                    // their image after AppKit composition, so both sides have
                    // undergone the same destination color-space conversion.
                    std::unique_ptr<native::canvas> image_surface;
                    NSBitmapImageRep *image_pixels = nil;
                    if (kind == 3) {
                        image_surface = std::make_unique<native::canvas>(180, 40, 140, 24);
                        image_surface->on_wnd_paint.connect([&](native::wnd_paint_event event) {
                            event.g.clear(native::rgba(255, 255, 255, 255))
                                .draw_img(image, native::point(0, 0));
                            return true;
                        });
                        attach(*image_surface, root);
                        image_pixels = render(mac::view_from_control(image_surface.get()));
                    }
                    // Different native hierarchies can snap accessory text
                    // to adjacent device pixels. Permit one whole-frame
                    // baseline shift, retaining the strict color/state error.
                    double difference = 1, window_difference = 1;
                    const int baseline_range = kind == 3 ? 1 : 0;
                    for (int baseline = -baseline_range; baseline <= baseline_range; ++baseline) {
                        double image_error = 0, window_error = 0;
                        for (int y = 0; y < 24; ++y)
                            for (int x = 0; x < 140; ++x) {
                                NSColor *color = y + baseline >= 0 && y + baseline < 24
                                    ? [[reference colorAtX:x y:y + baseline]
                                        colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]]
                                    : [NSColor colorWithSRGBRed:1 green:1 blue:1 alpha:1];
                                const auto pixel = image.pixels()[y * 140 + x];
                                const double alpha = [color alphaComponent];
                                NSColor *painted = [[window_pixels colorAtX:x y:y]
                                    colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                                window_error += std::abs([painted redComponent] -
                                    ([color redComponent] * alpha + 1 - alpha));
                                window_error += std::abs([painted greenComponent] -
                                    ([color greenComponent] * alpha + 1 - alpha));
                                window_error += std::abs([painted blueComponent] -
                                    ([color blueComponent] * alpha + 1 - alpha));
                                NSColor *image_color = image_pixels ? [[image_pixels colorAtX:x y:y]
                                    colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]] : nil;
                                image_error += std::abs((image_color ? [image_color redComponent] : pixel.r / 255.0) -
                                    ([color redComponent] * alpha + 1 - alpha));
                                image_error += std::abs((image_color ? [image_color greenComponent] : pixel.g / 255.0) -
                                    ([color greenComponent] * alpha + 1 - alpha));
                                image_error += std::abs((image_color ? [image_color blueComponent] : pixel.b / 255.0) -
                                    ([color blueComponent] * alpha + 1 - alpha));
                            }
                        difference = std::min(difference, image_error / (140 * 24 * 3));
                        window_difference = std::min(window_difference, window_error / (140 * 24 * 3));
                    }
                    if (window_difference >= 0.003) {
                        [[reference representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                            writeToFile:@"native-theme-reference.png" atomically:YES];
                        [[window_pixels representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                            writeToFile:@"native-theme-window.png" atomically:YES];
                        image.save("native-theme-image.png");
                    }
                    expect(window_difference < 0.003,
                        ("window theme mismatch: kind=" + std::to_string(kind) +
                         " variant=" + std::to_string(variant) +
                         " dark=" + std::to_string(dark) +
                         " difference=" + std::to_string(window_difference)).c_str());
                    expect(difference < 0.005,
                        ("image theme mismatch: kind=" + std::to_string(kind) +
                         " variant=" + std::to_string(variant) +
                         " dark=" + std::to_string(dark) +
                         " difference=" + std::to_string(difference)).c_str());
                }
            }
            [window setAppearance:nil];
        }
        [utility close];
        [utility release];
        [accessory removeFromSuperview]; [accessory release];
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

    // AppKit can retain a view through a pending repaint after window close.
    void closed_window_checks(native::app_wnd &root) {
        NSView *stale = nil;
        int paints = 0;
        {
            native::modeless_wnd dialog(root, "Late repaint", 20, 20, 160, 100);
            dialog.on_wnd_paint.connect([&](native::wnd_paint_event) { ++paints; return true; });
            dialog.create(); dialog.show();
            stale = [[mac::wnd_bindings.handle_from_object(&dialog) contentView] retain];
            dialog.destroy();
            dialog.create(); dialog.show();
            paints = 0;
            render(stale);
            expect(paints == 0, "an old native view cannot repaint a recreated window");
            dialog.destroy();
        }
        render(stale);
        expect(paints == 0, "a retained AppKit view cannot call a destroyed C++ window");
        [stale release];
    }

    // Changing an edge mask must not switch the stock NSButton renderer.
    void border_checks(native::app_wnd &root) {
        native::button preview("Preview", 20, 100, 140, 28);
        attach(preview, root);
        auto *button = mac::button_bindings.object_from_handle(&preview)->ns_button;
        NSBitmapImageRep *reference = render_masked_control(button);
        auto paper = [](NSBitmapImageRep *pixels, int x, int y) {
            NSColor *color = [[pixels colorAtX:x y:y]
                colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
            return [color redComponent] > 0.98 && [color greenComponent] > 0.98 &&
                [color blueComponent] > 0.98;
        };
        int low = 0, high = 27, left = 0, right = 139;
        while (low < 27 && paper(reference, 20, low)) ++low;
        while (high > 0 && paper(reference, 20, high)) --high;
        while (left < 139 && paper(reference, left, 14)) ++left;
        while (right > 0 && paper(reference, right, 14)) --right;
        expect(low < high && left < right, "native inset bezel is visible");
        for (unsigned bits = 0; bits < 16; ++bits) {
            const auto sides = static_cast<native::border_sides>(bits);
            preview.set_border_sides(sides);
            NSBitmapImageRep *actual = render_masked_control(button);
            expect([button bezelStyle] == NSBezelStyleRounded, "border mask preserves rounded AppKit bezel");
            const bool keep_left = native::has_border(sides, native::border_sides::left);
            const bool keep_right = native::has_border(sides, native::border_sides::right);
            expect(paper(actual, left, 14) == !keep_left && paper(actual, right, 14) == !keep_right,
                "border flags mask the actual inset native side edges");
            const int removed_vertical = int(paper(actual, 20, low)) + int(paper(actual, 20, high));
            const int expected_vertical = int(!native::has_border(sides, native::border_sides::top)) +
                int(!native::has_border(sides, native::border_sides::bottom));
            expect(removed_vertical == expected_vertical, "native top/bottom edges are removed independently");
            if (!bits) {
                expect(![button isBordered] && ![[button layer] mask],
                    "borderless native button omits its entire rounded bezel");
                for (int y = 0; y < 28; ++y)
                    for (int x = 0; x < 16; ++x)
                        expect(paper(actual, x, y) && paper(actual, 139 - x, y),
                            "removing every edge leaves no rounded corner fragments");
                continue;
            }
            expect([button isBordered], "restoring edges restores the native bezel");
            for (int y = 10; y < 19; ++y)
                for (int x = 16; x < 124; ++x)
                    if (![[reference colorAtX:x y:y] isEqual:[actual colorAtX:x y:y]]) {
                        throw std::runtime_error("native border interior: mask=" + std::to_string(bits) +
                            " x=" + std::to_string(x) + " y=" + std::to_string(y) +
                            " intrinsic height=" + std::to_string([button intrinsicContentSize].height));
                    }
        }
        native::text_edit help("Read-only help", native::text_edit_mode::multi_line);
        help.set_read_only(true).set_border_sides(native::border_sides::none);
        attach(help, root);
        auto *editor = mac::text_edit_bindings.object_from_handle(&help);
        expect(![editor->text_view drawsBackground] && ![editor->scroll drawsBackground] &&
            [editor->scroll borderType] == NSNoBorder, "borderless help uses the page background");
    }

    void message_box_checks(native::app_wnd &root) {
        NSWindow *window = mac::wnd_bindings.handle_from_object(&root);
        __block bool attached = false;
        [NSTimer scheduledTimerWithTimeInterval:0.05 repeats:NO
            block:^(NSTimer *) {
                NSWindow *sheet = [window attachedSheet];
                attached = sheet && [sheet sheetParent] == window;
                if (sheet)
                    [window endSheet:sheet
                          returnCode:NSAlertFirstButtonReturn];
                else
                    [NSApp abortModal];
            }];
        const auto result = native::message_box::show(
            root, "Owner placement", "Sheet regression",
            native::message_box_buttons::yes_no_cancel);
        expect(attached, "native message alert attaches to its owner");
        expect(result == native::message_box_result::yes,
            "owner sheet preserves the synchronous three-button result");
    }

    void toolbar_checks(native::app_wnd &root) {
        vision::feature_properties dialog(root);
        dialog.create();
        dialog.show();
        NSView *dialog_content = [mac::wnd_bindings.handle_from_object(&dialog) contentView];
        NSWindow *window = mac::wnd_bindings.handle_from_object(&dialog);
        [window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
        auto *dialog_bitmap = render(dialog_content);
        NSColor *bar_paper = [[dialog_bitmap colorAtX:200 y:12]
            colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
        int ink = 0;
        for (int y = 2; y < 22; ++y)
            for (int x = 2; x < 125; ++x) {
                NSColor *color = [[dialog_bitmap colorAtX:x y:y]
                    colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                // Native hover-only buttons dim idle content. Verify visible
                // contrast to their actual bar instead of requiring black.
                if (std::abs([color redComponent] - [bar_paper redComponent]) > 0.2)
                    ++ink;
            }
        [[dialog_bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
            writeToFile:@"native-mac-properties-refine.png" atomically:YES];
        expect(ink > 40,
            "property panel repaint preserves sibling toolbar icons and labels");
        const auto folder = native::file_icon::for_directory(std::filesystem::current_path(), 32);
        const auto folder_image = native::img::decode(folder.get_png().data(), folder.get_png().size());
        int visible_icon_pixels = 0;
        for (int index = 0; index < folder_image.w() * folder_image.h(); ++index)
            if (folder_image.pixels()[index].a) ++visible_icon_pixels;
        expect(folder.get_source() == native::file_icon_source::native && visible_icon_pixels > 40,
            "workspace folder icons produce visible native pixels");
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
            NSButton *native_button = nil;
            for (NSView *child in [view subviews])
                if ([child isKindOfClass:[NSButton class]]) native_button = (NSButton *)child;
            expect(native_button && [native_button bezelStyle] == NSBezelStyleFlexiblePush &&
                [native_button showsBorderOnlyWhileMouseInside],
                "edge toolbar uses flexible-height native flat buttons");
            int commands = 0;
            bar.on_command.connect([&](native::tool_command) { ++commands; return true; });
            [native_button performClick:nil];
            expect(commands == 1, "native toolbar action emits a portable command");
            bar.set_enabled("test", false);
            expect(![native_button isEnabled], "native toolbar follows disabled state");
            [native_button performClick:nil];
            expect(commands == 1, "disabled native toolbar does not emit commands");
            bar.set_enabled("test", true);
            auto *bitmap = render(view);
            int red = 0;
            for (NSInteger y = 0; y < [bitmap pixelsHigh]; ++y)
                for (NSInteger x = 0; x < [bitmap pixelsWide]; ++x) {
                    NSColor *color = [[bitmap colorAtX:x y:y]
                        colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
                    // AppKit converts images through the display profile. Check
                    // red dominance rather than assuming device RGB is sRGB.
                    if ([color redComponent] > 0.7 &&
                        [color redComponent] - [color greenComponent] > 0.5 &&
                        [color redComponent] - [color blueComponent] > 0.5)
                        ++red;
                }
            expect(red > 20, ("toolbar displays its icon on the native canvas: red=" + std::to_string(red)).c_str());
            bar.set_items({{"one", "One", native::tool_kind::exclusive, "group", true},
                           {"two", "Two", native::tool_kind::exclusive, "group", false}});
            NSButton *one = nil, *two = nil;
            for (NSView *child in [view subviews]) {
                if (![child isKindOfClass:[NSButton class]]) continue;
                if ([[(NSButton *)child identifier] isEqualToString:@"one"]) one = (NSButton *)child;
                if ([[(NSButton *)child identifier] isEqualToString:@"two"]) two = (NSButton *)child;
            }
            [one performClick:nil];
            expect([one state] == NSControlStateValueOn && commands == 1,
                "re-activating an exclusive native tool preserves its checked state");
            [two performClick:nil];
            expect([two state] == NSControlStateValueOn && [one state] == NSControlStateValueOff && commands == 2,
                "native exclusive tools update their peers and emit one command");
            found = true;
        }
        expect(found, "toolbar creates a visible child surface");
    }

}

void native_mac_navigation_checks(native::app_wnd &root);

// Run presentation regressions in the application's AppKit UI thread.
void native_mac_presentation_checks(native::app_wnd &root) {
    closed_window_checks(root);
    message_box_checks(root);
    border_checks(root);
    toolbar_checks(root);
    native_mac_navigation_checks(root);
    selection_theme_checks(root);
    ruler_motion_checks(root);
    cursor_checks(root);
}

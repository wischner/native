//
// Checks scalable native toolbar feedback and the actual Vision navigator
// artwork in both AppKit appearances, including its selected rows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#import <AppKit/AppKit.h>
#include <native.h>
#include "../src/vision_settings.h"
#include "../lib/native/platforms/mac/globals.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    NSBitmapImageRep *render(NSView *view) {
        [view layoutSubtreeIfNeeded];
        auto *image = [view bitmapImageRepForCachingDisplayInRect:[view bounds]];
        [view cacheDisplayInRect:[view bounds] toBitmapImageRep:image];
        return image;
    }

    NSView *find(NSView *host, Class type) {
        if ([host isKindOfClass:type]) return host;
        for (NSView *child in [host subviews])
            if (auto *match = find(child, type)) return match;
        return nil;
    }

    NSColor *pixel(NSBitmapImageRep *image, int x, int y) {
        return [[image colorAtX:x y:y]
            colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    }

    // Compare rendered glyph rows with a real native text field. Matching
    // view/alignment rectangles alone misses AppKit cell baseline offsets.
    void picker_alignment_checks(NSView *content, bool time, bool dark) {
        auto *picker = (NSDatePicker *)find(content, [NSDatePicker class]);
        expect(picker, "Vision's selected page has a native picker");
        auto *field = (NSTextField *)[picker superview];
        auto *reference = [[[NSTextField alloc] initWithFrame:
            NSMakeRect(500, 380, 120, [field bounds].size.height)] autorelease];
        [reference setFont:[picker font]];
        auto *format = [[[NSDateFormatter alloc] init] autorelease];
        [format setLocale:[picker locale] ?: [NSLocale currentLocale]];
        [format setCalendar:[picker calendar]];
        [format setTimeZone:[picker timeZone]];
        [format setDateStyle:time ? NSDateFormatterNoStyle : NSDateFormatterShortStyle];
        [format setTimeStyle:time ? NSDateFormatterShortStyle : NSDateFormatterNoStyle];
        [reference setStringValue:[format stringFromDate:[picker dateValue]]];
        [reference setEditable:NO];
        [content addSubview:reference];
        auto *standard = render(reference);
        auto *actual = render(field);
        const auto span = [dark](NSBitmapImageRep *image, int left, int right) {
            int top = int([image pixelsHigh]), bottom = -1;
            for (int y = 2; y < [image pixelsHigh] - 2; ++y)
                for (int x = left; x < right; ++x) {
                    auto *color = pixel(image, x, y);
                    const double light = ([color redComponent] + [color greenComponent] +
                        [color blueComponent]) / 3;
                    if ([color alphaComponent] > 0.5 && (dark ? light > 0.6 : light < 0.7)) {
                        top = std::min(top, y); bottom = std::max(bottom, y);
                    }
                }
            return std::pair{top, bottom};
        };
        const CGFloat scale = [actual pixelsHigh] / [field bounds].size.height;
        const auto expected = span(standard, int(5 * scale), int(70 * scale));
        const auto text = span(actual, int(5 * scale),
            int(([field bounds].size.width - (time ? 19 : 28)) * scale));
        const auto accessory = span(actual,
            int(([field bounds].size.width - (time ? 15 : 25)) * scale),
            int(([field bounds].size.width - 3) * scale));
        [[actual representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
            writeToFile:time ? (dark ? @"picker-time-dark.png" : @"picker-time-light.png")
                            : (dark ? @"picker-date-dark.png" : @"picker-date-light.png") atomically:YES];
        [[standard representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
            writeToFile:dark ? @"picker-reference-dark.png" : @"picker-reference-light.png" atomically:YES];
        [reference removeFromSuperview];
        expect(text == expected, "picker glyphs share the standard text-field baseline");
        if (accessory.second < accessory.first ||
            std::abs((accessory.first + accessory.second + 1) / 2.0 -
                [actual pixelsHigh] / 2.0) > 0.5) {
            throw std::runtime_error(std::string(time ? "time" : "date") +
                (dark ? " dark" : " light") + " accessory rows=" +
                std::to_string(accessory.first) + ".." +
                std::to_string(accessory.second) + " field height=" +
                std::to_string([actual pixelsHigh]));
        }
    }

    void toolbar_size_checks(native::app_wnd &root) {
        native::toolbar bar(root);
        auto image = std::make_shared<native::img>(32, 32);
        image->get_gpx().clear(native::rgba(240, 0, 0, 255));
        bar.set_items({{"large", "", native::tool_kind::toggle,
            {}, false, true, image}});
        auto *window = mac::wnd_bindings.handle_from_object(&root);
        for (auto edge : {native::window_edge::top, native::window_edge::left}) {
            bar.set_edge(edge);
            for (int extent : {16, 24, 32, 48}) {
                bar.set_icon_size({native::dim(extent), native::dim(extent)});
                NSButton *button = nil;
                for (NSView *host in [[window contentView] subviews])
                    for (NSView *child in [host subviews])
                        if ([child isKindOfClass:[NSButton class]] &&
                            [[(NSButton *)child identifier] isEqualToString:@"large"])
                            button = (NSButton *)child;
                expect(button != nil, "toolbar native button exists after resizing");
                bar.set_checked("large", false);
                auto *idle = render(button);
                bar.set_checked("large", true);
                auto *selected = render(button);
                int top = int([selected pixelsHigh]), bottom = -1;
                int left = int([selected pixelsWide]), right = -1;
                for (int y = 0; y < [selected pixelsHigh]; ++y)
                    for (int x = 0; x < [selected pixelsWide]; ++x) {
                        auto *a = pixel(idle, x, y);
                        auto *b = pixel(selected, x, y);
                        const double difference =
                            std::abs([a redComponent] - [b redComponent]) +
                            std::abs([a greenComponent] - [b greenComponent]) +
                            std::abs([a blueComponent] - [b blueComponent]);
                        if (difference > 0.04) {
                            top = std::min(top, y); bottom = std::max(bottom, y);
                            left = std::min(left, x); right = std::max(right, x);
                        }
                    }
                const CGFloat scale = [selected pixelsHigh] / [button bounds].size.height;
                if (bottom - top + 1 < extent * scale ||
                    right - left + 1 < extent * scale) {
                    [[idle representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                        writeToFile:@"toolbar-idle-refine.png" atomically:YES];
                    [[selected representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                        writeToFile:@"toolbar-selected-refine.png" atomically:YES];
                    throw std::runtime_error("toolbar highlight icon=" +
                        std::to_string(extent) + " width=" +
                        std::to_string(right - left + 1) + " height=" +
                        std::to_string(bottom - top + 1) + " scale=" +
                        std::to_string(scale));
                }
                if (extent == 48) {
                    auto *capture = render([button superview]);
                    [[capture representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                        writeToFile:edge == native::window_edge::top
                            ? @"native-mac-toolbar-48-top.png"
                            : @"native-mac-toolbar-48-left.png" atomically:YES];
                }
            }
        }
    }

    void navigation_checks(native::app_wnd &root) {
        vision::feature_settings dialog(root);
        dialog.create(); dialog.show();
        auto *window = mac::wnd_bindings.handle_from_object(&dialog);
        [window makeMainWindow];
        [window makeKeyAndOrderFront:nil];
        auto *outline = (NSOutlineView *)find([window contentView], [NSOutlineView class]);
        expect(outline && [outline numberOfRows] == 5,
            "Vision has three icon pages and two native section headings");
        for (bool dark : {false, true}) {
            [window setAppearance:[NSAppearance appearanceNamed:
                dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua]];
            for (NSNumber *identifier in @[@1, @2, @3]) {
                const NSInteger row = [outline rowForItem:identifier];
                [outline selectRowIndexes:[NSIndexSet indexSetWithIndex:row]
                    byExtendingSelection:NO];
                [outline layoutSubtreeIfNeeded];
                auto *cell = (NSTableCellView *)[outline viewAtColumn:0 row:row
                    makeIfNecessary:YES];
                [cell layoutSubtreeIfNeeded];
                auto *view = [cell imageView];
                if (![view image] || [[view image] isTemplate] ||
                    [view frame].size.width != 24 || [view frame].size.height != 24)
                    throw std::runtime_error("settings row=" +
                        std::to_string([identifier intValue]) + " icon=" +
                        std::to_string([view frame].size.width) + "x" +
                        std::to_string([view frame].size.height));
                auto *pixels = render(view);
                int color = 0, white = 0;
                for (int y = 0; y < [pixels pixelsHigh]; ++y)
                    for (int x = 0; x < [pixels pixelsWide]; ++x) {
                        auto *value = pixel(pixels, x, y);
                        // AppKit's cached NSImageView display can carry the
                        // native inactive alpha. Its RGB colors are straight.
                        if ([value alphaComponent] < 0.4) continue;
                        if ([value redComponent] > 0.85 &&
                            [value greenComponent] > 0.85 &&
                            [value blueComponent] > 0.85) ++white;
                        if (std::max({[value redComponent], [value greenComponent],
                                [value blueComponent]}) -
                            std::min({[value redComponent], [value greenComponent],
                                [value blueComponent]}) > 0.2) ++color;
                    }
                if (color <= 100 || white <= 20) {
                    [[pixels representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                        writeToFile:@"native-mac-navigation-icon.png" atomically:YES];
                    throw std::runtime_error("navigation icon color=" +
                        std::to_string(color) + " white=" + std::to_string(white));
                }
                if ([identifier intValue] != 1) {
                    picker_alignment_checks([window contentView], [identifier intValue] == 3, dark);
                    auto *capture = render([window contentView]);
                    NSString *path = [NSString stringWithFormat:
                        @"native-mac-settings-%d-%@.png", [identifier intValue],
                        dark ? @"dark" : @"light"];
                    [[capture representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                        writeToFile:path atomically:YES];
                }
            }
        }
    }
}

void native_mac_navigation_checks(native::app_wnd &root) {
    toolbar_size_checks(root);
    navigation_checks(root);
}

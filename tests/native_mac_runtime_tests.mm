//
// Verifies AppKit rendering for stock controls and native collection cells,
// while retaining the explicit derived-control drawing extension.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>
#include <native.h>
#include "../lib/native/platforms/mac/globals.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace
{
    int failures = 0;

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

    void image_text_checks() {
        native::img image(64, 48);
        image.get_gpx().clear(native::rgba(0, 0, 0, 0))
            .set_font(native::font_t::stock(native::font_role::control))
            .set_ink(native::rgba(220, 40, 30, 160))
            .draw_text("F", native::point(10, 8));
        int first = -1, last = -1, top_width = 0, bottom_width = 0;
        for (int y = 0; y < image.h(); ++y) {
            int count = 0;
            for (int x = 0; x < image.w(); ++x) {
                const auto pixel = image.pixels()[y * image.w() + x];
                if (pixel.a > 40) {
                    ++count;
                    expect(pixel.r > 200 && pixel.g < 60,
                        "image text retains straight RGBA color at partial alpha");
                }
            }
            if (count) {
                if (first < 0) { first = y; top_width = count; }
                last = y;
                bottom_width = count;
            }
        }
        expect(first >= 8 && last < 32 && top_width > bottom_width,
            "image glyph F is upright at its requested top-left position");
        const auto untouched = image.pixels()[0];
        expect(untouched.a == 0, "text does not modify pixels outside its glyph");
    }

    class custom_button final : public native::button
    {
    public:
        using native::button::button;
        int draws = 0;
    protected:
        void draw_text(native::gpx &g, native::theme &theme,
                       const native::rect &bounds,
                       const native::theme::state &state) override {
            ++draws;
            native::button::draw_text(g, theme, bounds, state);
        }
    };

    class virtual_rows final : public native::table_model
    {
    public:
        mutable std::size_t requests = 0;
        std::size_t row_count() const override { return 1000000; }
        native::table_row_id row_id(std::size_t row) const override {
            return row + 1;
        }
        native::table_cell cell(std::size_t row,
                                native::table_column_id) const override {
            ++requests;
            return {std::to_string(row + 1), nullptr};
        }
    };

    void virtual_grid_checks(native::app_wnd &root) {
        virtual_rows model;
        native::table_view table(10, 10, 450, 180);
        table.set_model(&model).set_columns({{1, "Row", 160}, {2, "Value", 180}})
            .set_data_mode(native::table_data_mode::virtualized)
            .set_grid_lines(native::table_grid_lines::both);
        attach(table, root);
        auto *peer = mac::table_view_bindings.object_from_handle(&table);
        render(peer->scroll);
        expect([peer->table numberOfRows] == 1000000 && model.requests < 1000,
            "native virtual grids only request visible cells");
        const CGFloat row_height = [peer->table rowHeight];
        table.set_row_height(35);
        table.set_row_height(std::nullopt);
        expect([peer->table rowHeight] == row_height,
            "clearing explicit row height restores AppKit's default");
        table.scroll_to_row(900001);
        render(peer->scroll);
        expect(NSLocationInRange(900000,
            [peer->table rowsInRect:[peer->table visibleRect]]),
            "native virtual grid reveals distant rows");
        [[peer->scroll contentView] scrollToPoint:NSMakePoint(0, row_height * 150)];
        [peer->scroll reflectScrolledClipView:[peer->scroll contentView]];
        expect(table.get_first_visible_row() == 151,
            "native scrollbar updates the portable first-visible row");
        table.set_grid_lines(native::table_grid_lines::none);
        table.set_grid_lines(native::table_grid_lines::both);
        expect(table.get_first_visible_row() == 151,
            "changing native grid lines preserves the scrolled row");
        expect([peer->table gridStyleMask] ==
            (NSTableViewSolidHorizontalGridLineMask |
             NSTableViewSolidVerticalGridLineMask),
            "virtual tables retain both native grid axes");
        table.scroll_to_row(1000000);
        render(peer->scroll);
        expect(NSMaxRange([peer->table rowsInRect:[peer->table visibleRect]]) == 1000000,
            "the final virtual row is reachable");
    }

    void geometry_checks(native::app_wnd &root) {
        native::list left({"Left", "Two"});
        native::list right({"Right", "Two"});
        native::split_view split(left, right,
            native::split_orientation::horizontal, 10, 10, 450, 180);
        split.set_minimums(40, 50);
        attach(split, root);
        auto *peer = mac::split_view_bindings.object_from_handle(&split);
        expect([peer->view isKindOfClass:[NSSplitView class]],
            "splitter uses NSSplitView");
        for (auto orientation : {native::split_orientation::horizontal,
                                 native::split_orientation::vertical}) {
            split.set_orientation(orientation);
            for (float ratio : {0.2f, 0.7f}) {
                split.set_ratio(ratio);
                split.set_dimensions(native::size(460, 190));
                [peer->view layoutSubtreeIfNeeded];
                for (native::wnd *page : {&split.get_first(), &split.get_second()}) {
                    NSView *view = mac::view_from_control(page);
                    expect(NSWidth([[view superview] bounds]) > 30 &&
                        NSHeight([[view superview] bounds]) > 30,
                        "native split panes have usable initial and resized bounds");
                    expect(NSEqualSizes([view frame].size,
                        [[view superview] bounds].size),
                        "both borrowed controls fill their NSSplitView panes");
                }
            }
        }
        int changes = 0;
        split.on_ratio_change.connect([&](float) { ++changes; return true; });
        [peer->view setPosition:80 ofDividerAtIndex:0];
        expect(changes == 1, "native divider movement emits one ratio change");
        render(peer->view);
        expect(!mac::wnd_gpx_bindings.object_from_handle(&split),
            "NSSplitView paints its divider without the portable painter");
        split.set_dimensions(native::size(8, 8));
        split.set_dimensions(native::size(450, 180));
        expect(NSWidth([peer->first frame]) > 0 && NSHeight([peer->second frame]) > 0,
            "native split survives shrinking below both pane minimums");
        split.destroy();

        native::list first({"First page"});
        native::list second({"Second page"});
        native::tab_view tabs(10, 10, 360, 180);
        tabs.add_item("First", first);
        tabs.add_item("Second", second);
        attach(tabs, root);
        auto *tab_peer = mac::tab_view_bindings.object_from_handle(&tabs);
        expect([tab_peer->view class] == [NSTabView class],
            "tabs use stock NSTabView, not buttons or a painted replacement");
        for (auto placement : {native::tab_placement::top,
                               native::tab_placement::bottom,
                               native::tab_placement::left,
                               native::tab_placement::right}) {
            tabs.set_tab_placement(placement);
            for (int selected : {1, 0}) {
                tabs.set_selected_index(selected);
                tabs.set_dimensions(native::size(400 + selected * 20, 210));
                [tab_peer->view layoutSubtreeIfNeeded];
                NSView *host = [[tab_peer->view selectedTabViewItem] view];
                NSView *child = mac::view_from_control(
                    &tabs.get_item(selected).get_content());
                expect([child superview] == host,
                    "NSTabViewItem owns the selected page hierarchy");
                expect(NSEqualSizes([child frame].size, [host bounds].size),
                    "tab page fits the native content rectangle after resizing");
            }
            render(tab_peer->view);
            expect(!mac::wnd_gpx_bindings.object_from_handle(&tabs),
                "all four tab placements use only AppKit painting");
        }
    }

    void checks(native::app_wnd &root) {
        image_text_checks();
        geometry_checks(root);
        virtual_grid_checks(root);
        native::button button("Native", 10, 10, 120, 32);
        native::check check("Enabled", 10, 50, 140, 24);
        native::radio first("First", 10, 80, 100, 24);
        native::radio second("Second", 115, 80, 100, 24);
        custom_button custom("Custom", 150, 10, 120, 32);
        for (native::wnd *control : std::vector<native::wnd *>{&button,
                 &check, &first, &second, &custom})
            attach(*control, root);
        int clicks = 0;
        button.on_click.connect([&] { ++clicks; return true; });
        [mac::button_bindings.object_from_handle(&button)->ns_button performClick:nil];
        [mac::check_bindings.object_from_handle(&check)->button performClick:nil];
        [mac::radio_bindings.object_from_handle(&first)->button performClick:nil];
        [mac::radio_bindings.object_from_handle(&second)->button performClick:nil];
        expect(clicks == 1 && check.get_checked() &&
            second.get_selected() && !first.get_selected(),
            "native button/check/radio actions update portable state");
        for (native::wnd *control : std::vector<native::wnd *>{&button,
                 &check, &first, &second}) {
            render(mac::view_from_control(control));
            expect(!mac::wnd_gpx_bindings.object_from_handle(control),
                   "stock AppKit controls do not invoke the portable painter");
        }
        render(mac::view_from_control(&custom));
        expect(custom.draws > 0, "derived button retains drawing hooks");

        auto image = std::make_shared<native::img>(16, 16);
        image->get_gpx().clear(native::rgba(220, 40, 30, 255));
        native::table_store store({{1, {{1, {"Image", image.get()}}, {2, {"12"}}}},
                                   {2, {{1, {"Text"}}, {2, {"24"}}}}});
        store.set_groups({{1, "Documents", 0, 2, true, true}});
        native::table_view table(10, 115, 360, 160);
        table.set_columns({{1, "Name", 220}, {2, "Size", 100}})
            .set_model(&store).set_grid_lines(native::table_grid_lines::both);
        attach(table, root);
        auto *table_peer = mac::table_view_bindings.object_from_handle(&table);
        expect([table_peer->table gridStyleMask] ==
            (NSTableViewSolidHorizontalGridLineMask |
             NSTableViewSolidVerticalGridLineMask),
            "both grid axes use NSTableView's native grid drawing");
        if (@available(macOS 11.0, *))
            expect([table_peer->table style] == NSTableViewStyleFullWidth,
                "data tables use the native full-width grid style");
        table.set_grid_lines(native::table_grid_lines::none);
        expect([table_peer->table gridStyleMask] == NSTableViewGridNone,
            "grid lines can be switched off without a painted overlay");
        table.set_grid_lines(native::table_grid_lines::both);
        render(table_peer->scroll);
        auto cell = static_cast<NSTableCellView *>([table_peer->table
            viewAtColumn:0 row:1 makeIfNecessary:YES]);
        [cell layoutSubtreeIfNeeded];
        expect([cell isKindOfClass:[NSTableCellView class]] &&
            [[[cell textField] stringValue] isEqualToString:@"Image"] &&
            [[cell imageView] image] && ![[cell textField] isHidden],
            "table cells contain visible AppKit text and image views");
        render(cell);
        render([table_peer->table headerView]);
        expect(!mac::wnd_gpx_bindings.object_from_handle(&table),
               "stock table cells and headers do not owner-draw");
        NSTableCellView *group = static_cast<NSTableCellView *>(
            [(id<NSTableViewDelegate>)[table_peer->table delegate]
                tableView:table_peer->table viewForTableColumn:nil row:0]);
        expect([[[group textField] stringValue] isEqualToString:@"Documents"],
               "native group row supplies a spanning label");
        NSButton *disclosure = static_cast<NSButton *>([[group subviews] firstObject]);
        [disclosure performClick:nil];
        expect(!table.get_group_expanded(1), "native table disclosure collapses rows");
        table.set_group_expanded(1, true);
        table.set_alternating_rows(true);
        render(table_peer->scroll);
        NSTableRowView *odd = [table_peer->table rowViewAtRow:2 makeIfNecessary:YES];
        for (NSAppearanceName name in @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]) {
            [[NSAppearance appearanceNamed:name] performAsCurrentDrawingAppearance:^{
                const auto space = [NSColorSpace deviceRGBColorSpace];
                const CGFloat base = [[[NSColor textBackgroundColor]
                    colorUsingColorSpace:space] redComponent];
                const CGFloat stripe = [[[odd backgroundColor]
                    colorUsingColorSpace:space] redComponent];
                const CGFloat grid = [[[table_peer->table gridColor]
                    colorUsingColorSpace:space] redComponent];
                expect(std::abs(base - stripe) > 0.06 && std::abs(base - grid) > 0.17,
                    "native row and grid colors remain distinct in light and dark appearance");
            }];
        }
        table.set_alternating_rows(false);
        render(table_peer->scroll);
        odd = [table_peer->table rowViewAtRow:2 makeIfNecessary:YES];
        expect([[odd backgroundColor] isEqual:[NSColor textBackgroundColor]],
            "turning stripes off restores the native plain row background");

        native::tree_view tree({}, 390, 115, 250, 160);
        tree.set_items({{"Folder", image, 1,
            {{"File", image, 2}}, true}});
        attach(tree, root);
        auto *tree_peer = mac::tree_view_bindings.object_from_handle(&tree);
        render(tree_peer->scroll);
        auto tree_cell = static_cast<NSTableCellView *>([tree_peer->outline
            viewAtColumn:0 row:0 makeIfNecessary:YES]);
        expect([tree_cell isKindOfClass:[NSTableCellView class]] &&
            [[tree_cell imageView] image] && [[tree_cell textField] stringValue],
            "outline supplies native image/text cells");
        render(tree_cell);
        auto child_cell = static_cast<NSTableCellView *>([tree_peer->outline
            viewAtColumn:0 row:1 makeIfNecessary:YES]);
        NSBitmapImageRep *child_bitmap = render(child_cell);
        const CGFloat scale = [child_bitmap pixelsWide] / NSWidth([child_cell bounds]);
        NSColor *child_ink = [[child_bitmap colorAtX:10 * scale y:12 * scale]
            colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
        expect([child_ink redComponent] > 0.6 && [child_ink greenComponent] < 0.4,
            "expanded outline child image is painted, not only assigned");
        expect([[child_cell imageView] image] &&
            ![[child_cell imageView] isHidden] &&
            NSWidth([[child_cell imageView] frame]) > 0,
            "expanded outline children retain a visible native image");
        expect(!mac::wnd_gpx_bindings.object_from_handle(&tree),
               "stock outline rows do not owner-draw");

        native::icon_view icons({}, 10, 300, 360, 160);
        icons.set_items({{"Image", image, 1}, {"Disabled", image, 2, false}});
        attach(icons, root);
        auto *icon_peer = mac::icon_view_bindings.object_from_handle(&icons);
        [icon_peer->collection layoutSubtreeIfNeeded];
        NSIndexPath *path = [NSIndexPath indexPathForItem:0 inSection:0];
        NSCollectionViewItem *item = [(id<NSCollectionViewDataSource>)icon_peer->adapter
            collectionView:icon_peer->collection itemForRepresentedObjectAtIndexPath:path];
        expect([item imageView] && [item textField] && [[item imageView] image],
               "collection items contain native image and text views");
        [icon_peer->collection setSelectionIndexPaths:[NSSet setWithObject:path]];
        [(id<NSCollectionViewDelegate>)icon_peer->adapter
            collectionView:icon_peer->collection
            didSelectItemsAtIndexPaths:[NSSet setWithObject:path]];
        expect(icons.get_selected_index() == 0, "native icon selection reaches model");
        render(icon_peer->scroll);
        expect(!mac::wnd_gpx_bindings.object_from_handle(&icons),
               "stock collection does not invoke portable painting");

        native::list page({"Native page"}, 0, 0, 200, 140);
        native::list other({"Second page"}, 0, 0, 200, 140);
        native::accordion accordion(390, 300, 250, 130);
        accordion.set_mode(native::accordion_mode::multiple);
        accordion.add_item("One", page).set_expanded(true);
        accordion.add_item("Two", other).set_expanded(true);
        attach(accordion, root);
        auto *accordion_peer = mac::accordion_bindings.object_from_handle(&accordion);
        render(accordion_peer->scroll);
        expect([accordion_peer->scroll documentView] == accordion_peer->stack &&
            [accordion_peer->scroll hasVerticalScroller],
            "accordion uses a native scrolling disclosure stack");
        expect(!mac::wnd_gpx_bindings.object_from_handle(&accordion),
               "stock accordion does not paint a duplicate portable stack");
        if (const char *path = std::getenv("NATIVE_MAC_TEST_SNAPSHOT")) {
            NSView *view = [mac::wnd_bindings.handle_from_object(&root) contentView];
            [view layoutSubtreeIfNeeded];
            NSBitmapImageRep *bitmap = [view
                bitmapImageRepForCachingDisplayInRect:[view bounds]];
            [view cacheDisplayInRect:[view bounds] toBitmapImageRep:bitmap];
            NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG
                properties:@{}];
            std::ofstream output(path, std::ios::binary);
            output.write(static_cast<const char *>([png bytes]), [png length]);
            expect(output.good(), "write the test's own offscreen-rendered snapshot");
        }
        [accordion_peer->headers[0] performClick:nil];
        expect(!accordion.get_item(0).get_expanded(),
               "native accordion disclosure updates borrowed-page state");
        accordion.set_mode(native::accordion_mode::single);
        for (int index : {0, 1, 0}) {
            accordion.get_item(index).set_expanded(true);
            for (int height : {130, 190}) {
                accordion.set_dimensions(native::size(250, height));
                render(accordion_peer->scroll);
                expect(![accordion_peer->scroll hasVerticalScroller],
                    "single-open accordion does not add a second scrollbar");
                expect(NSHeight([accordion_peer->stack bounds]) <=
                    NSHeight([[accordion_peer->scroll contentView] bounds]) + 1,
                    "single-open accordion fits the actual native viewport");
            }
        }
    }
}

int program(int, char **) {
    native::app_wnd root("AppKit control regressions", 80, 80, 680, 500);
    root.on_wnd_create.connect([&] {
        native::app::post([&] {
            try {
                checks(root);
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n';
                ++failures;
            }
            native::app::post([&] { root.destroy(); });
        });
        return true;
    });
    native::app::run(root);
    return failures ? 1 : 0;
}

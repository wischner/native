//
// Hosts an AppKit source-list sidebar, sidebar material and optional native
// search field. AppKit owns row selection, group headings and control drawing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "native_cells.h"
#include "../../picker_peer.h"
#include <algorithm>

@interface native_settings_material : NSVisualEffectView
@end
@implementation native_settings_material
- (BOOL)isFlipped { return YES; }
@end

@interface native_settings_cell : native_content_cell
@end
@implementation native_settings_cell
- (void)layout {
    [super layout];
    const NSRect bounds = [self bounds];
    const bool image = [[self imageView] image] != nil;
    const CGFloat edge = std::min<CGFloat>(24, bounds.size.height);
    [[self imageView] setFrame:NSMakeRect(2,
        (bounds.size.height - edge) / 2, edge, edge)];
    NSRect text = [[self textField] frame];
    text.origin.x = image ? edge + 8 : 2;
    text.size.width = std::max<CGFloat>(0,
        bounds.size.width - text.origin.x - 2);
    [[self textField] setFrame:text];
}
@end

@interface native_settings_source : NSObject <NSOutlineViewDataSource, NSOutlineViewDelegate, NSSearchFieldDelegate> {
@public
    native::settings_sidebar *_owner;
    NSMutableArray *_rows;
    BOOL _updating;
}
- (void)searchChanged:(NSSearchField *)sender;
@end

@implementation native_settings_source
- (NSInteger)outlineView:(NSOutlineView *)outline numberOfChildrenOfItem:(id)item {
    (void)outline; return item ? 0 : [_rows count];
}
- (id)outlineView:(NSOutlineView *)outline child:(NSInteger)index ofItem:(id)item {
    (void)outline; (void)item; return [_rows objectAtIndex:index];
}
- (BOOL)outlineView:(NSOutlineView *)outline isItemExpandable:(id)item {
    (void)outline; (void)item; return NO;
}
- (BOOL)outlineView:(NSOutlineView *)outline isGroupItem:(id)item {
    (void)outline; return [item isKindOfClass:[NSDictionary class]];
}
- (CGFloat)outlineView:(NSOutlineView *)outline heightOfRowByItem:(id)item {
    (void)outline;
    return [item isKindOfClass:[NSDictionary class]] ? 22 : 32;
}
- (BOOL)outlineView:(NSOutlineView *)outline shouldSelectItem:(id)item {
    (void)outline;
    return _owner && _owner->get_input_enabled() && [item isKindOfClass:[NSNumber class]] &&
        _owner->get_item([item intValue]).enabled;
}
- (NSView *)outlineView:(NSOutlineView *)outline viewForTableColumn:(NSTableColumn *)column item:(id)item {
    (void)column;
    const bool group = [item isKindOfClass:[NSDictionary class]];
    NSString *identifier = group ? @"section" : @"page";
    auto *cell = (native_content_cell *)[outline makeViewWithIdentifier:identifier owner:nil];
    if (!cell) {
        cell = [[[native_settings_cell alloc] initWithFrame:NSZeroRect] autorelease];
        [cell setIdentifier:identifier];
    }
    if (group) {
        mac::configure_cell(cell, [item objectForKey:@"title"], nil, NSTextAlignmentLeft);
        [[cell textField] setFont:[NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]]];
        [[cell textField] setTextColor:[NSColor secondaryLabelColor]];
    } else {
        const auto &page = _owner->get_item([item intValue]);
        mac::configure_cell(cell, [NSString stringWithUTF8String:page.title.c_str()],
            mac::cell_image(page.image.get()), NSTextAlignmentLeft, page.enabled);
    }
    return cell;
}
- (void)outlineViewSelectionDidChange:(NSNotification *)notification {
    if (_updating || !_owner) return;
    auto *outline = (NSOutlineView *)[notification object];
    if ([outline selectedRow] < 0) return;
    id item = [outline itemAtRow:[outline selectedRow]];
    if ([item isKindOfClass:[NSNumber class]]) _owner->on_native_selection([item intValue]);
}
- (void)searchChanged:(NSSearchField *)sender {
    if (!_updating && _owner)
        _owner->on_native_search([[sender stringValue] UTF8String]);
}
- (void)controlTextDidChange:(NSNotification *)notification {
    [self searchChanged:(NSSearchField *)[notification object]];
}
@end

namespace
{
    struct sidebar_state
    {
        native_settings_material *material = nil;
        NSScrollView *scroll = nil;
        NSOutlineView *outline = nil;
        NSSearchField *search = nil;
        native_settings_source *source = nil;
        std::vector<native::settings_page> items;
        ~sidebar_state() {
            if (source) source->_owner = nullptr;
            [outline setDelegate:nil]; [outline setDataSource:nil]; [search setDelegate:nil]; [search setTarget:nil];
            [material removeFromSuperview];
            if (source) [source->_rows release];
            [source release];
            [outline release]; [scroll release]; [search release]; [material release];
        }
    };
    bool same_items(const std::vector<native::settings_page> &a, const std::vector<native::settings_page> &b) {
        if (a.size() != b.size()) return false;
        for (std::size_t index = 0; index < a.size(); ++index)
            if (a[index].id != b[index].id || a[index].title != b[index].title ||
                a[index].section != b[index].section || a[index].image != b[index].image ||
                a[index].enabled != b[index].enabled) return false;
        return true;
    }
}
namespace native::detail
{
    bool settings_sidebar_uses_list() { return false; }
    bool settings_sidebar_uses_native_search() { return true; }
    int settings_sidebar_navigation_padding() { return 0; }
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        auto *peer = new sidebar_state;
        assign_peer_state(owner, peer);
        peer->source = [[native_settings_source alloc] init];
        peer->source->_owner = &owner;
        peer->source->_rows = [[NSMutableArray alloc] init];
        peer->material = [[native_settings_material alloc] initWithFrame:NSZeroRect];
        [peer->material setMaterial:NSVisualEffectMaterialSidebar];
        [peer->material setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
        [peer->material setState:NSVisualEffectStateFollowsWindowActiveState];
        peer->scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
        [peer->scroll setBorderType:NSNoBorder];
        [peer->scroll setDrawsBackground:NO];
        [peer->scroll setHasVerticalScroller:YES];
        [peer->scroll setAutohidesScrollers:YES];
        peer->outline = [[NSOutlineView alloc] initWithFrame:NSZeroRect];
        auto *column = [[[NSTableColumn alloc] initWithIdentifier:@"settings"] autorelease];
        [peer->outline addTableColumn:column];
        [peer->outline setOutlineTableColumn:column];
        [peer->outline setHeaderView:nil];
        [peer->outline setStyle:NSTableViewStyleSourceList];
        // System row-size styles overwrite image-view frames during row
        // reuse. Keep native source-list painting with explicit icon metrics.
        [peer->outline setRowSizeStyle:NSTableViewRowSizeStyleCustom];
        [peer->outline setRowHeight:32];
        [peer->outline setBackgroundColor:[NSColor clearColor]];
        [peer->outline setAllowsEmptySelection:YES];
        [peer->outline setFloatsGroupRows:NO];
        [peer->outline setDataSource:peer->source]; [peer->outline setDelegate:peer->source];
        [peer->scroll setDocumentView:peer->outline];
        [peer->material addSubview:peer->scroll];
        peer->search = [[NSSearchField alloc] initWithFrame:NSZeroRect];
        [peer->search setPlaceholderString:@"Search"];
        [peer->search setDelegate:peer->source];
        [peer->search setTarget:peer->source];
        [peer->search setAction:@selector(searchChanged:)];
        [peer->search setSendsWholeSearchString:NO];
        [peer->search setSendsSearchStringImmediately:YES];
        [peer->material addSubview:peer->search];
        [mac::panel_bindings.object_from_handle(&owner)->view addSubview:peer->material];
        return true;
    }
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *peer = peer_state<sidebar_state>(owner);
        if (!peer) return;
        peer->source->_updating = YES;
        const auto area = owner.get_client_bounds();
        const auto navigation = settings_navigation_bounds(owner);
        [peer->material setFrame:NSMakeRect(area.x1(), area.y1(), navigation.w(), area.h())];
        [peer->scroll setFrame:NSMakeRect(0, navigation.y1() - area.y1(), navigation.w(), navigation.h())];
        [peer->search setFrame:NSMakeRect(8, 8, std::max(0, int(navigation.w()) - 16), 24)];
        [peer->search setHidden:!owner.get_search_visible()];
        [peer->search setEnabled:owner.get_input_enabled()];
        NSString *query = [NSString stringWithUTF8String:owner.get_search_text().c_str()];
        if (![[peer->search stringValue] isEqualToString:query]) [peer->search setStringValue:query];
        const auto items = settings_visible_items(owner);
        if (!same_items(peer->items, items)) {
            peer->items = items;
            [peer->source->_rows removeAllObjects];
            std::string section;
            for (const auto &item : items) {
                if (!item.section.empty() && item.section != section)
                    // Include position so repeated headings remain distinct
                    // NSOutlineView items even when their labels are equal.
                    [peer->source->_rows addObject:@{@"title": [NSString stringWithUTF8String:item.section.c_str()],
                        @"position": @([peer->source->_rows count])}];
                section = item.section;
                [peer->source->_rows addObject:[NSNumber numberWithInt:item.id]];
            }
            [peer->outline reloadData];
        }
        const NSInteger row = [peer->outline rowForItem:[NSNumber numberWithInt:owner.get_selected_item()]];
        if (row >= 0) [peer->outline selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
        else [peer->outline deselectAll:nil];
        peer->source->_updating = NO;
    }
}

//
// Implements the native AppKit list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#import <AppKit/AppKit.h>
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/list.h>
#include "globals.h"
@interface native_list_adapter
    : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
@public
    void *_owner;
    BOOL _suppress;
}
@end
@implementation native_list_adapter
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table {
    (void)table;
    auto *o = static_cast<native::list *>(_owner);
    return o ? static_cast<NSInteger>(o->get_items().size()) : 0;
}
- (id)tableView:(NSTableView *)table
    objectValueForTableColumn:(NSTableColumn *)column
                          row:(NSInteger)row {
    (void)table;
    (void)column;
    auto *o = static_cast<native::list *>(_owner);
    if (!o || row < 0 ||
        row >= static_cast<NSInteger>(o->get_items().size()))
        return @"";
    return [NSString stringWithUTF8String:o->get_items()[row].c_str()];
}
- (void)tableViewSelectionDidChange:(NSNotification *)note {
    if (_suppress)
        return;
    auto *o = static_cast<native::list *>(_owner);
    if (o)
        o->on_native_selection(
            static_cast<int>([[note object] selectedRow]));
}
@end
namespace
{
    NSView *parent(native::list *c) {
        auto *p = c->get_parent();
        NSView *view = mac::parent_view(p);
        if (!view)
            throw std::runtime_error(
                "macOS: list requires a created parent.");
        return view;
    }
    native_list_adapter *adapter(mac::mac_list *b) {
        return static_cast<native_list_adapter *>(b->adapter);
    }
} // namespace
namespace native
{
    void list::apply_items() {
        auto *b = mac::list_bindings.object_from_handle(this);
        if (!b || !b->table)
            throw std::runtime_error("macOS: Missing list binding.");
        adapter(b)->_suppress = YES;
        [b->table reloadData];
        adapter(b)->_suppress = NO;
    }
    void list::apply_selected_index() {
        auto *b = mac::list_bindings.object_from_handle(this);
        if (!b || !b->table)
            throw std::runtime_error("macOS: Missing list binding.");
        adapter(b)->_suppress = YES;
        if (_selected_index < 0)
            [b->table deselectAll:nil];
        else
            [b->table selectRowIndexes:
                          [NSIndexSet
                              indexSetWithIndex:static_cast<NSUInteger>(
                                                    _selected_index)]
                  byExtendingSelection:NO];
        adapter(b)->_suppress = NO;
    }
    void list::create() const {
        if (_created)
            return;
        auto *self = const_cast<list *>(this);
        NSScrollView *s = [[NSScrollView alloc]
            initWithFrame:NSMakeRect(_bounds.p.x,
                                     _bounds.p.y,
                                     _bounds.d.w,
                                     _bounds.d.h)];
        [s setHasVerticalScroller:YES];
        [s setAutohidesScrollers:YES];
        [s setBorderType:NSLineBorder];
        NSTableView *t = [[NSTableView alloc]
            initWithFrame:NSMakeRect(0, 0, _bounds.d.w, _bounds.d.h)];
        NSTableColumn *c =
            [[NSTableColumn alloc] initWithIdentifier:@"native_item"];
        [c setWidth:std::max(1, static_cast<int>(_bounds.d.w) - 2)];
        [c setResizingMask:NSTableColumnAutoresizingMask];
        [t addTableColumn:c];
        [c release];
        [t setHeaderView:nil];
        [t setAllowsMultipleSelection:NO];
        [t setAllowsEmptySelection:YES];
        [t setSelectionHighlightStyle:
               NSTableViewSelectionHighlightStyleRegular];
        [t setIntercellSpacing:NSZeroSize];
        [t setColumnAutoresizingStyle:
               NSTableViewLastColumnOnlyAutoresizingStyle];
        [t setGridStyleMask:NSTableViewGridNone];
        [t setUsesAlternatingRowBackgroundColors:NO];
        [t setBackgroundColor:[NSColor textBackgroundColor]];
        native_list_adapter *a = [[native_list_adapter alloc] init];
        a->_owner = self;
        a->_suppress = YES;
        [t setDataSource:a];
        [t setDelegate:a];
        [s setDocumentView:t];
        [t sizeLastColumnToFit];
        [parent(self) addSubview:s];
        [t reloadData];
        if (_selected_index >= 0)
            [t selectRowIndexes:[NSIndexSet indexSetWithIndex:
                                                static_cast<NSUInteger>(
                                                    _selected_index)]
                byExtendingSelection:NO];
        a->_suppress = NO;
        auto *b = new mac::mac_list();
        b->scroll = s;
        b->table = t;
        b->adapter = a;
        mac::list_bindings.register_pair(self, b);
        _created = true;
        self->on_native_create();
    }
    void list::show() const {
        auto *b = mac::list_bindings.object_from_handle(
            const_cast<list *>(this));
        if (!_created || !b || !b->scroll)
            throw std::runtime_error("macOS: list is not created.");
        [b->scroll setHidden:NO];
    }
    void list::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<list *>(this);
        auto *b = mac::list_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (b) {
            [b->table setDataSource:nil];
            [b->table setDelegate:nil];
            [b->scroll removeFromSuperview];
            [b->scroll setDocumentView:nil];
            [b->scroll release];
            [b->table release];
            [b->adapter release];
            mac::list_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native

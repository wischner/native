//
// Implements the native AppKit combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <AppKit/AppKit.h>

#include <stdexcept>

#include <native/combo_box.h>

#include "globals.h"

@interface native_combo_delegate : NSObject <NSComboBoxDelegate> {
@public
    void *_owner;
}
@end

@implementation native_combo_delegate
- (void)comboBoxSelectionDidChange:(NSNotification *)notification {
    auto *owner = static_cast<native::combo_box *>(_owner);
    auto *state = owner ? mac::combo_box_bindings
                              .object_from_handle(owner) : nullptr;
    if (!owner || !state || state->suppress) return;
    owner->on_native_selection(
        static_cast<int>([(NSComboBox *)[notification object]
                              indexOfSelectedItem]));
}
- (void)controlTextDidChange:(NSNotification *)notification {
    auto *owner = static_cast<native::combo_box *>(_owner);
    auto *state = owner ? mac::combo_box_bindings
                              .object_from_handle(owner) : nullptr;
    if (!owner || !state || state->suppress ||
        owner->get_style() != native::combo_box_style::editable) return;
    NSString *value = [(NSComboBox *)[notification object] stringValue];
    owner->on_native_text(value ? [value UTF8String] : "");
}
@end

namespace
{
    NSString *string_value(const std::string &value) {
        NSString *text = [NSString stringWithUTF8String:value.c_str()];
        return text ? text : @"";
    }
}

namespace native
{
    void combo_box::apply_items() {
        auto *state = mac::combo_box_bindings.object_from_handle(this);
        if (!state || !state->combo)
            throw std::runtime_error("macOS: Missing combo box binding.");
        state->suppress = true;
        [state->combo removeAllItems];
        for (const auto &item : get_items())
            [state->combo addItemWithObjectValue:string_value(item)];
        state->suppress = false;
    }

    void combo_box::apply_selected_index() {
        auto *state = mac::combo_box_bindings.object_from_handle(this);
        if (!state || !state->combo)
            throw std::runtime_error("macOS: Missing combo box binding.");
        state->suppress = true;
        if (get_selected_index() < 0) {
            const NSInteger current = [state->combo indexOfSelectedItem];
            if (current >= 0)
                [state->combo deselectItemAtIndex:current];
        } else {
            [state->combo selectItemAtIndex:get_selected_index()];
        }
        state->suppress = false;
    }

    void combo_box::apply_text() {
        auto *state = mac::combo_box_bindings.object_from_handle(this);
        if (!state || !state->combo)
            throw std::runtime_error("macOS: Missing combo box binding.");
        state->suppress = true;
        [state->combo setStringValue:string_value(get_text())];
        state->suppress = false;
    }

    void combo_box::apply_style() {
        auto *state = mac::combo_box_bindings.object_from_handle(this);
        if (state && state->combo)
            [state->combo setEditable:
                get_style() == combo_box_style::editable ? YES : NO];
    }

    void combo_box::create() const {
        if (_created) return;
        NSView *parent = mac::parent_view(get_parent());
        if (!parent)
            throw std::runtime_error(
                "macOS: combo box requires a created parent.");
        auto *self = const_cast<combo_box *>(this);
        NSComboBox *combo = [[NSComboBox alloc]
            initWithFrame:NSMakeRect(_bounds.p.x, _bounds.p.y,
                                     _bounds.d.w, _bounds.d.h)];
        [combo setUsesDataSource:NO];
        [combo setCompletes:YES];
        [combo setEditable:get_style() == combo_box_style::editable
                              ? YES : NO];
        native_combo_delegate *delegate =
            [[native_combo_delegate alloc] init];
        delegate->_owner = self;
        [combo setDelegate:delegate];
        for (const auto &item : get_items())
            [combo addItemWithObjectValue:string_value(item)];
        if (get_selected_index() >= 0)
            [combo selectItemAtIndex:get_selected_index()];
        [combo setStringValue:string_value(get_text())];
        [parent addSubview:combo];
        auto *state = new mac::mac_combo_box;
        state->combo = combo;
        state->delegate = delegate;
        mac::combo_box_bindings.register_pair(self, state);
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        auto *state = mac::combo_box_bindings.object_from_handle(
            const_cast<combo_box *>(this));
        if (!_created || !state || !state->combo)
            throw std::runtime_error("macOS: combo box is not created.");
        [state->combo setHidden:NO];
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *state = mac::combo_box_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (state) {
            [state->combo setDelegate:nil];
            [state->combo removeFromSuperview];
            [state->combo release];
            [state->delegate release];
            mac::combo_box_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

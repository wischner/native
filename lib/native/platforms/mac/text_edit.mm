//
// Implements AppKit text fields and text views with live portable
// validation and shared clipboard command routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <stdexcept>
#include <string>

#import <AppKit/AppKit.h>

#include "globals.h"

namespace
{
    // Convert portable UTF-8 into a non-null AppKit string.
    NSString *native_string(const std::string &text) {
        NSString *value =
            [NSString stringWithUTF8String:text.c_str()];
        return value ? value : @"";
    }

    // Convert AppKit text into copied portable UTF-8.
    std::string portable_string(NSString *text) {
        const char *value = text ? [text UTF8String] : nullptr;
        return value ? std::string(value) : std::string();
    }

    // Return the editable text view for either AppKit editor mode.
    NSTextView *editing_view(mac::mac_text_edit *binding) {
        if (!binding)
            return nil;
        if (binding->text_view)
            return binding->text_view;
        id editor = binding->field
                        ? [binding->field currentEditor]
                        : nil;
        return [editor isKindOfClass:[NSTextView class]]
                   ? (NSTextView *)editor
                   : nil;
    }

    // Return the complete value stored by an AppKit editor.
    NSString *editor_value(mac::mac_text_edit *binding) {
        if (!binding)
            return @"";
        return binding->text_view ? [binding->text_view string]
                                  : [binding->field stringValue];
    }

    // Assign a complete value without entering the delegate again.
    void set_editor_value(mac::mac_text_edit *binding,
                          const std::string &text) {
        if (!binding)
            return;
        binding->suppress = true;
        if (binding->text_view)
            [binding->text_view setString:native_string(text)];
        else
            [binding->field setStringValue:native_string(text)];
        binding->suppress = false;
    }

    // Validate one delegate-originated complete value or restore it.
    void handle_change(native::text_edit *owner) {
        auto *binding = owner
                            ? mac::text_edit_bindings
                                  .object_from_handle(owner)
                            : nullptr;
        if (!binding || binding->suppress)
            return;
        const std::string candidate =
            portable_string(editor_value(binding));
        if (!owner->on_native_text(candidate))
            set_editor_value(binding, owner->get_text());
    }

    // Route one AppKit command selector through portable editor logic.
    bool handle_command(native::text_edit *owner, SEL command) {
        if (!owner)
            return false;
        if (command == @selector(copy:)) {
            owner->copy();
            return true;
        }
        if (command == @selector(cut:)) {
            owner->cut();
            return true;
        }
        if (command == @selector(paste:)) {
            owner->paste();
            return true;
        }
        if (command == @selector(selectAll:)) {
            owner->select_all();
            return true;
        }
        return false;
    }
} // namespace

@interface native_text_edit_delegate
    : NSObject <NSTextFieldDelegate, NSTextViewDelegate> {
@public
    void *_owner;
}
@end

@implementation native_text_edit_delegate

- (void)controlTextDidChange:(NSNotification *)notification {
    (void)notification;
    handle_change(static_cast<native::text_edit *>(_owner));
}

- (void)textDidChange:(NSNotification *)notification {
    (void)notification;
    handle_change(static_cast<native::text_edit *>(_owner));
}

- (BOOL)control:(NSControl *)control
        textView:(NSTextView *)view
    doCommandBySelector:(SEL)command {
    (void)control;
    (void)view;
    return handle_command(
        static_cast<native::text_edit *>(_owner), command);
}

- (BOOL)textView:(NSTextView *)view
    doCommandBySelector:(SEL)command {
    (void)view;
    return handle_command(
        static_cast<native::text_edit *>(_owner), command);
}

@end

namespace native
{
    void text_edit::apply_text() {
        auto *binding =
            mac::text_edit_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error(
                "macOS: Missing text-edit binding.");
        set_editor_value(binding, _text);
    }

    void text_edit::apply_read_only() {
        auto *binding =
            mac::text_edit_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error(
                "macOS: Missing text-edit binding.");
        if (binding->text_view)
            [binding->text_view setEditable:_read_only ? NO : YES];
        else
            [binding->field setEditable:_read_only ? NO : YES];
    }

    void text_edit::create() const {
        if (_created)
            return;
        wnd *parent = get_parent();
        NSWindow *window = parent
                               ? mac::wnd_bindings.handle_from_object(
                                     parent)
                               : nil;
        if (!parent || !parent->get_created() || !window)
            throw std::runtime_error(
                "macOS: text_edit requires a created parent.");

        auto *self = const_cast<text_edit *>(this);
        auto *binding = new mac::mac_text_edit;
        native_text_edit_delegate *delegate =
            [[native_text_edit_delegate alloc] init];
        delegate->_owner = self;
        binding->delegate = delegate;
        const NSRect frame = NSMakeRect(_bounds.p.x,
                                        _bounds.p.y,
                                        _bounds.d.w,
                                        _bounds.d.h);
        if (_mode == text_edit_mode::single_line) {
            binding->field =
                [[NSTextField alloc] initWithFrame:frame];
            [binding->field setStringValue:native_string(_text)];
            [binding->field setEditable:_read_only ? NO : YES];
            [binding->field setDelegate:delegate];
            [[window contentView] addSubview:binding->field];
        } else {
            binding->scroll =
                [[NSScrollView alloc] initWithFrame:frame];
            [binding->scroll setHasVerticalScroller:YES];
            [binding->scroll setBorderType:NSBezelBorder];
            binding->text_view = [[NSTextView alloc]
                initWithFrame:[[binding->scroll contentView] bounds]];
            [binding->text_view setString:native_string(_text)];
            [binding->text_view setEditable:_read_only ? NO : YES];
            [binding->text_view setDelegate:delegate];
            [binding->text_view setVerticallyResizable:YES];
            [binding->text_view setAutoresizingMask:NSViewWidthSizable];
            [binding->scroll setDocumentView:binding->text_view];
            [[window contentView] addSubview:binding->scroll];
        }

        mac::text_edit_bindings.register_pair(self, binding);
        _created = true;
        self->on_wnd_create.emit();
    }

    void text_edit::show() const {
        auto *binding = mac::text_edit_bindings.object_from_handle(
            const_cast<text_edit *>(this));
        if (!_created || !binding)
            throw std::runtime_error(
                "macOS: text_edit is not created.");
        NSView *view = binding->scroll
                           ? static_cast<NSView *>(binding->scroll)
                           : static_cast<NSView *>(binding->field);
        [view setHidden:NO];
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding =
            mac::text_edit_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            NSView *view = binding->scroll
                               ? static_cast<NSView *>(binding->scroll)
                               : static_cast<NSView *>(binding->field);
            [view removeFromSuperview];
            [binding->field release];
            [binding->text_view release];
            [binding->scroll release];
            [binding->delegate release];
            mac::text_edit_bindings.unregister_by_handle(self);
            delete binding;
        }
    }

    std::string text_edit::selected_text() const {
        auto *binding = mac::text_edit_bindings.object_from_handle(
            const_cast<text_edit *>(this));
        NSTextView *view = editing_view(binding);
        if (!view)
            return {};
        const NSRange selection = [view selectedRange];
        NSString *value = [view string];
        if (selection.length == 0 ||
            NSMaxRange(selection) > [value length])
            return {};
        return portable_string(
            [value substringWithRange:selection]);
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        auto *binding =
            mac::text_edit_bindings.object_from_handle(this);
        NSTextView *view = editing_view(binding);
        if (!binding || _read_only)
            return false;

        const NSRange selection = view
                                      ? [view selectedRange]
                                      : NSMakeRange(
                                            [editor_value(binding)
                                                length],
                                            0);
        NSMutableString *candidate =
            [[editor_value(binding) mutableCopy] autorelease];
        [candidate replaceCharactersInRange:selection
                                 withString:native_string(text)];
        const std::string portable = portable_string(candidate);
        if (!validate(portable))
            return false;

        set_editor_value(binding, portable);
        const NSUInteger cursor =
            selection.location + [native_string(text) length];
        NSTextView *updated = editing_view(binding);
        if (updated)
            [updated setSelectedRange:NSMakeRange(cursor, 0)];
        on_native_text(portable);
        return true;
    }

    void text_edit::select_all_native() const {
        auto *binding = mac::text_edit_bindings.object_from_handle(
            const_cast<text_edit *>(this));
        if (!binding)
            return;
        if (binding->text_view)
            [binding->text_view selectAll:nil];
        else
            [binding->field selectText:nil];
    }
} // namespace native

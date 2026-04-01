#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <stdexcept>
#include <utility>

#include <native.h>

#include "globals.h"

@interface MacButtonTarget : NSObject
{
@public
    native::button *_owner;
}
- (void)buttonAction:(id)sender;
@end

@implementation MacButtonTarget
- (void)buttonAction:(id)sender
{
    (void)sender;
    if (_owner)
        _owner->on_click.emit();
}
@end

namespace
{
    static NSString *to_nsstring(const std::string &text)
    {
        NSString *value = [NSString stringWithUTF8String:text.c_str()];
        if (!value)
            value = @"";
        return value;
    }
}

namespace native
{
    button::button(std::string text, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _text(std::move(text))
    {
    }

    button::button(const std::string &text, const point &pos, const size &dim)
        : button(text, pos.x, pos.y, dim.w, dim.h)
    {
    }

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    const std::string &button::text() const
    {
        return _text;
    }

    button &button::set_text(const std::string &text)
    {
        _text = text;

        if (_created)
        {
            auto *h = mac::button_bindings.from_a(const_cast<button *>(this));
            if (h && h->ns_button)
                [h->ns_button setTitle:to_nsstring(_text)];
        }

        return *this;
    }

    void button::create() const
    {
        if (_created)
            return;

        wnd *p = parent();
        if (!p)
            throw std::runtime_error("macOS: button requires a parent window.");

        NSWindow *window = mac::wnd_bindings.from_b(p);
        if (!window)
            throw std::runtime_error("macOS: button parent is not created.");

        NSView *content = [window contentView];
        if (!content)
            throw std::runtime_error("macOS: button parent has no content view.");

        NSButton *btn = [[NSButton alloc] initWithFrame:NSMakeRect(_bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h)];
        [btn setTitle:to_nsstring(_text)];
#if defined(NSBezelStyleRounded)
        [btn setBezelStyle:NSBezelStyleRounded];
#else
        [btn setBezelStyle:NSRoundedBezelStyle];
#endif

        MacButtonTarget *target = [[MacButtonTarget alloc] init];
        target->_owner = const_cast<button *>(this);

        [btn setTarget:target];
        [btn setAction:@selector(buttonAction:)];
        [content addSubview:btn];

        auto *self = const_cast<button *>(this);
        auto *h = new mac::macbutton();
        h->ns_button = btn;
        h->target = target;
        h->owner = self;
        mac::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const
    {
        if (!_created)
            throw std::runtime_error("macOS: Cannot show button before it is created.");

        auto *h = mac::button_bindings.from_a(const_cast<button *>(this));
        if (!h || !h->ns_button)
            throw std::runtime_error("macOS: Missing NSButton binding.");

        [h->ns_button setHidden:NO];
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = mac::button_bindings.from_a(self);

        if (h)
        {
            if (h->ns_button)
            {
                [h->ns_button removeFromSuperview];
                [h->ns_button release];
            }
            if (h->target)
                [h->target release];

            mac::button_bindings.unregister_by_a(self);
            delete h;
        }

        _created = false;
    }
} // namespace native

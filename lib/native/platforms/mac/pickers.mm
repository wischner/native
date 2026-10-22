//
// Uses AppKit NSDatePicker for civil dates and wall-clock time. A UTC
// Gregorian calendar preserves civil fields independently of DST changes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../picker_peer.h"
#include <algorithm>
#include <cmath>

namespace
{
    NSCalendar *calendar() {
        auto *value = [[[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease];
        [value setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        return value;
    }
    NSDate *date_value(native::date date, native::time_of_day time = {}) {
        auto *parts = [[[NSDateComponents alloc] init] autorelease];
        [parts setYear:date.year]; [parts setMonth:date.month]; [parts setDay:date.day];
        [parts setHour:time.hour]; [parts setMinute:time.minute]; [parts setSecond:time.second];
        return [calendar() dateFromComponents:parts];
    }
    native::date civil_date(NSDate *value) {
        auto *parts = [calendar() components:NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay fromDate:value];
        return {int([parts year]), unsigned([parts month]), unsigned([parts day])};
    }
    native::time_of_day civil_time(NSDate *value) {
        auto *parts = [calendar() components:NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond fromDate:value];
        return {unsigned([parts hour]), unsigned([parts minute]), unsigned([parts second])};
    }
}

// One standard-height field encloses the date/calendar action or the time
// editor/stepper. AppKit draws its bezel and adaptive control background.
@interface native_picker_field : NSTextField
@end
@implementation native_picker_field
- (BOOL)isFlipped { return YES; }
@end

@interface native_picker_target : NSObject <NSDatePickerCellDelegate> {
@public
    native::date_picker *_date;
    native::time_picker *_time;
    BOOL _updating;
    NSPopover *_popover;
}
- (void)changed:(NSDatePicker *)sender;
- (void)showCalendar:(NSButton *)sender;
@end

@implementation native_picker_target
- (void)showCalendar:(NSButton *)sender {
    if ([_popover isShown]) [_popover close];
    else [_popover showRelativeToRect:[sender bounds] ofView:sender preferredEdge:NSRectEdgeMaxY];
}
- (void)changed:(NSDatePicker *)sender {
    if (_updating) return;
    if (_date) _date->on_native_date(civil_date([sender dateValue]));
    else if (_time) {
        auto value = civil_time([sender dateValue]);
        if (!_time->get_seconds_visible()) value.second = _time->get_time().second;
        value = native::detail::stepped_time(*_time, value);
        _time->on_native_time(value);
    }
}
- (void)datePickerCell:(NSDatePickerCell *)cell validateProposedDateValue:(NSDate **)proposed
         timeInterval:(NSTimeInterval *)interval {
    (void)interval;
    if (_updating || !proposed || !*proposed) return;
    if (_date) {
        const auto value = civil_date(*proposed);
        if (!_date->is_date_enabled(value)) {
            *proposed = date_value(_date->get_date());
            _date->on_native_date(value);
        }
    } else if (_time) {
        auto value = civil_time(*proposed);
        if (!_time->get_seconds_visible()) value.second = _time->get_time().second;
        const auto stepped = native::detail::stepped_time(*_time, value);
        if (stepped != value) { value = stepped; *proposed = date_value({2000, 1, 1}, value); }
        if (!_time->is_time_enabled(value)) {
            *proposed = date_value({2000, 1, 1}, _time->get_time());
            _time->on_native_time(value);
        }
    }
    (void)cell;
}
@end

namespace
{
    struct picker_state
    {
        native_picker_field *field = nil;
        NSDatePicker *picker = nil;
        native_picker_target *target = nil;
        NSDatePicker *calendar_picker = nil;
        NSButton *calendar_button = nil;
        NSPopover *popover = nil;
        ~picker_state() {
            if (target) { target->_date = nullptr; target->_time = nullptr; }
            [popover close]; [popover setContentViewController:nil];
            [calendar_picker setTarget:nil]; [calendar_picker setDelegate:nil];
            [calendar_button setTarget:nil];
            [calendar_button removeFromSuperview]; [calendar_button release];
            [calendar_picker release]; [popover release];
            [picker setTarget:nil]; [picker setDelegate:nil];
            [picker removeFromSuperview]; [picker release]; [target release];
            [field removeFromSuperview]; [field release];
        }
    };
    picker_state &create(native::panel &owner, NSDatePickerStyle style) {
        auto *peer = new picker_state;
        peer->target = [[native_picker_target alloc] init];
        peer->field = [[native_picker_field alloc] initWithFrame:NSZeroRect];
        [peer->field setBezeled:YES];
        [peer->field setEditable:NO];
        [peer->field setSelectable:NO];
        [peer->field setClipsToBounds:YES];
        [peer->field setAccessibilityElement:NO];
        [peer->field setFocusRingType:NSFocusRingTypeNone];
        peer->picker = [[NSDatePicker alloc] initWithFrame:NSZeroRect];
        native::detail::assign_peer_state(owner, peer);
        [peer->picker setDatePickerStyle:style];
        // The small native stepper fits a regular text field. Keep its
        // segment text at the same regular font size as the date editor.
        [peer->picker setControlSize:style == NSDatePickerStyleTextFieldAndStepper
            ? NSControlSizeSmall : NSControlSizeRegular];
        [peer->picker setBezeled:NO];
        [peer->picker setBordered:NO];
        [peer->picker setDrawsBackground:NO];
        [peer->picker setCalendar:calendar()];
        [peer->picker setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [peer->picker setTarget:peer->target]; [peer->picker setAction:@selector(changed:)];
        [peer->picker setDelegate:peer->target];
        [peer->picker setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSControlSizeRegular]]];
        [peer->field setFont:[peer->picker font]];
        [peer->field setDrawsBackground:YES];
        [peer->field setBackgroundColor:[NSColor controlColor]];
        [peer->field addSubview:peer->picker];
        [mac::panel_bindings.object_from_handle(&owner)->view addSubview:peer->field];
        return *peer;
    }
    void geometry(native::panel &owner, picker_state &peer) {
        const auto area = owner.get_client_bounds();
        // NSDatePicker is unflipped inside the flipped field. Match its
        // native text baseline to the surrounding NSTextField; centering its
        // alignment rectangle alone puts text and the stepper too high.
        [peer.picker sizeToFit];
        const NSSize fitting =
            [peer.picker alignmentRectForFrame:[peer.picker frame]].size;
        const CGFloat accessory = peer.calendar_button ? 24 : 0;
        const CGFloat width = std::min<CGFloat>(
            fitting.width + accessory + 8, area.w());
        const CGFloat height = std::min<CGFloat>(area.h(),
            std::ceil([peer.field intrinsicContentSize].height));
        const CGFloat y = area.y1() + std::max<CGFloat>(0, (area.h() - height) / 2);
        [peer.field setFrame:NSMakeRect(area.x1(), y, width, height)];
        const NSRect editor = NSMakeRect(4, 0,
            std::max<CGFloat>(0, width - accessory - (accessory ? 8 : 4)),
            fitting.height);
        NSRect frame = [peer.picker frameForAlignmentRect:editor];
        frame.origin.y = 0;
        [peer.picker setFrame:frame];
        // Baseline offsets depend on the final native control height.
        const auto insets = [peer.picker alignmentRectInsets];
        const CGFloat baseline = height - [peer.field lastBaselineOffsetFromBottom];
        frame = [peer.picker frame];
        frame.origin.y += baseline - (NSMaxY(frame) - insets.bottom -
            [peer.picker lastBaselineOffsetFromBottom]);
        [peer.picker setFrameOrigin:frame.origin];
        if (peer.calendar_button) {
            // Let the native image cell center its symbol in the complete
            // standard-height field, without borrowing the host's padding.
            [peer.calendar_button setFrame:NSMakeRect(
                std::max<CGFloat>(0, width - 26), 0,
                std::min<CGFloat>(22, width), height)];
            [peer.calendar_picker setDateValue:[peer.picker dateValue]];
            [peer.calendar_picker setMinDate:[peer.picker minDate]];
            [peer.calendar_picker setMaxDate:[peer.picker maxDate]];
        }
        const bool valid = peer.target->_date ? peer.target->_date->get_valid() : peer.target->_time->get_valid();
        [peer.field setToolTip:valid ? nil : @"The current value is unavailable."];
    }
}
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) {
        auto &peer = create(owner, NSDatePickerStyleTextField);
        peer.target->_date = &owner;
        [peer.picker setDatePickerElements:NSDatePickerElementFlagYearMonthDay];
        [peer.picker setPresentsCalendarOverlay:NO];
        // Keep the native segment editor and offer a discoverable calendar
        // action. The popover also contains AppKit's actual date picker.
        peer.calendar_picker = [[NSDatePicker alloc] initWithFrame:NSZeroRect];
        [peer.calendar_picker setDatePickerStyle:NSDatePickerStyleClockAndCalendar];
        [peer.calendar_picker setDatePickerElements:NSDatePickerElementFlagYearMonthDay];
        [peer.calendar_picker setCalendar:calendar()];
        [peer.calendar_picker setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [peer.calendar_picker setDelegate:peer.target];
        [peer.calendar_picker setTarget:peer.target];
        [peer.calendar_picker setAction:@selector(changed:)];
        [peer.calendar_picker sizeToFit];
        auto *controller = [[[NSViewController alloc] init] autorelease];
        [controller setView:peer.calendar_picker];
        peer.popover = [[NSPopover alloc] init];
        [peer.popover setBehavior:NSPopoverBehaviorTransient];
        [peer.popover setContentViewController:controller];
        [peer.popover setContentSize:[peer.calendar_picker frame].size];
        peer.target->_popover = peer.popover;
        peer.calendar_button = [[NSButton alloc] initWithFrame:NSZeroRect];
        [peer.calendar_button setBordered:NO];
        [peer.calendar_button setImage:[NSImage imageWithSystemSymbolName:@"calendar" accessibilityDescription:@"Choose date"]];
        [peer.calendar_button setImagePosition:NSImageOnly];
        [peer.calendar_button setToolTip:@"Choose date"];
        [peer.calendar_button setTarget:peer.target];
        [peer.calendar_button setAction:@selector(showCalendar:)];
        [peer.field addSubview:peer.calendar_button];
        return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        create(owner, NSDatePickerStyleTextFieldAndStepper).target->_time = &owner;
        return true;
    }
    void refresh_date_picker_peer(date_picker &owner) {
        auto *peer = peer_state<picker_state>(owner);
        if (!peer) return;
        peer->target->_updating = YES;
        const bool in_range = owner.get_date() >= owner.get_minimum_date() && owner.get_date() <= owner.get_maximum_date();
        [peer->picker setMinDate:nil]; [peer->picker setMaxDate:nil];
        [peer->picker setMinDate:in_range ? date_value(owner.get_minimum_date()) : nil];
        [peer->picker setMaxDate:in_range ? date_value(owner.get_maximum_date()) : nil];
        [peer->picker setDateValue:date_value(owner.get_date())];
        geometry(owner, *peer); peer->target->_updating = NO;
    }
    void refresh_time_picker_peer(time_picker &owner) {
        auto *peer = peer_state<picker_state>(owner);
        if (!peer) return;
        peer->target->_updating = YES;
        [peer->picker setDatePickerElements:owner.get_seconds_visible()
            ? NSDatePickerElementFlagHourMinuteSecond : NSDatePickerElementFlagHourMinute];
        [peer->picker setMinDate:nil]; [peer->picker setMaxDate:nil];
        if (owner.get_time() >= owner.get_minimum_time() && owner.get_time() <= owner.get_maximum_time()) {
            [peer->picker setMinDate:date_value({2000, 1, 1}, owner.get_minimum_time())];
            [peer->picker setMaxDate:date_value({2000, 1, 1}, owner.get_maximum_time())];
        }
        [peer->picker setDateValue:date_value({2000, 1, 1}, owner.get_time())];
        geometry(owner, *peer); peer->target->_updating = NO;
    }
}

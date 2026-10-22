//
// Uses Haiku's native shared-kit DateEdit and TimeEdit controls. Their
// private SDK ABI is isolated here, like the existing ColumnListView peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "input_look.h"
#include "../../picker_peer.h"
#include <Autolock.h>
#include <DateTimeEdit.h>
#include <CalendarView.h>
#include <Button.h>
#include <ControlLook.h>
#include <Region.h>
#include <TextControl.h>
#include <Messenger.h>
#include <StringView.h>
#include <Message.h>
#include <Window.h>
#include <algorithm>
#include <stdexcept>
#include <charconv>
#include <cmath>
#include "ui_dispatch.h"

namespace
{
    constexpr uint32 calendar_closed = 'nCcl';
    // Keep the SDK's locale formatting and segmented editing, correcting its
    // omitted accessory width and fixed six-pixel text baseline.
    template<class editor, bool stepper>
    class fitted_edit : public editor
    {
    public:
        using editor::editor;
        BSize MinSize() override {
            return BSize(this->CountSections() * this->MinSectionWidth() +
                (this->CountSections() - 1) * this->SeparatorWidth() + 20,
                PreferredHeight());
        }
        BSize PreferredSize() override { return MinSize(); }
        float MinSectionWidth() override {
            return this->StringWidth(stepper ? "00" : "0000") + 4;
        }
        float SeparatorWidth() override {
            if constexpr (stepper) return editor::SeparatorWidth();
            return std::ceil(this->StringWidth("/"));
        }
        float PreferredHeight() override {
            BTextControl standard("measure", nullptr, "", nullptr);
            return standard.PreferredSize().height;
        }
        void DrawBorder(const BRect &update) override {
            BRect inside(this->Bounds());
            const uint32 flags = this->IsFocus() && this->Window() && this->Window()->IsActive()
                ? BControlLook::B_FOCUSED : 0;
            be_control_look->DrawTextControlBorder(this, inside, update,
                this->ViewColor(), flags);
            this->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
            this->FillRect(inside, B_SOLID_LOW);
            if constexpr (stepper) {
                const float middle = std::floor(this->Bounds().Height() / 2);
                const float half = std::floor((inside.Height() - 1) / 2);
                this->fUpRect = BRect(inside.right - 13, middle - half,
                    inside.right - 2, middle - 1);
                this->fDownRect = this->fUpRect.OffsetByCopy(0, half + 1);
                auto up = this->fUpRect, down = this->fDownRect;
                const auto base = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
                be_control_look->DrawArrowShape(this, up, update, base, BControlLook::B_UP_ARROW);
                be_control_look->DrawArrowShape(this, down, update, base, BControlLook::B_DOWN_ARROW);
            } else {
                this->fUpRect = this->fDownRect = BRect();
            }
        }
        void DrawSection(uint32 index, BRect bounds, bool focus) override {
            this->PushState();
            BRegion clip(bounds); this->ConstrainClippingRegion(&clip);
            this->SetLowColor(focus ? mix_color(ui_color(B_CONTROL_HIGHLIGHT_COLOR),
                ui_color(B_DOCUMENT_BACKGROUND_COLOR), 192) : ui_color(B_DOCUMENT_BACKGROUND_COLOR));
            this->FillRect(bounds, B_SOLID_LOW);
            editor::DrawSection(index, centered(bounds), focus);
            this->PopState();
        }
        void DrawSeparator(uint32 index, BRect bounds) override {
            editor::DrawSeparator(index, centered(bounds));
        }
    private:
        BRect centered(BRect bounds) {
            font_height metrics; this->GetFontHeight(&metrics);
            bounds.OffsetBy(0, std::floor((metrics.ascent - metrics.descent) / 2) - 6);
            return bounds;
        }
    };
    class calendar_view final : public BPrivate::BCalendarView
    {
    public:
        explicit calendar_view(native::date_picker &owner)
            : BCalendarView("native_calendar"), _owner(owner), _guard(owner) {}
        bool commit() {
            if (!_guard.alive()) return false;
            const auto value = Date();
            const bool accepted = _owner.on_native_date(
                {value.Year(), unsigned(value.Month()), unsigned(value.Day())});
            if (accepted) Window()->PostMessage(B_QUIT_REQUESTED);
            else if (_guard.alive()) {
                const auto previous = _owner.get_date();
                SetDate(previous.year, previous.month, previous.day);
            }
            return accepted;
        }
        void DrawDay(BView *view, BRect frame, const char *text, bool selected,
                     bool enabled, bool focus, bool highlight) override {
            int day = 0;
            std::from_chars(text, text + std::char_traits<char>::length(text), day);
            if (_guard.alive() && day > 0)
                enabled = enabled && _owner.is_date_enabled(
                    {Year(), unsigned(Month()), unsigned(day)});
            BCalendarView::DrawDay(view, frame, text, selected, enabled, focus, highlight);
        }
    private:
        native::date_picker &_owner;
        native::detail::window_guard _guard;
    };
    class calendar_window final : public BWindow
    {
    public:
        calendar_window(native::date_picker &owner, BView &anchor, BControl &trigger)
            : BWindow(BRect(0, 0, 260, 250), "Choose date", B_BORDERED_WINDOW_LOOK,
                      B_FLOATING_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE)
            , _trigger(&trigger), _identity(this)
            , _toggle_bounds(trigger.ConvertToScreen(trigger.Bounds())) {
            AddToSubset(anchor.Window());
            _calendar = new calendar_view(owner);
            _calendar->SetWeekNumberHeaderVisible(false);
            const auto value = owner.get_date();
            _calendar->SetDate(value.year, value.month, value.day);
            _calendar->ResizeToPreferred();
            AddChild(_calendar);
            _calendar->SetSelectionMessage(new BMessage('pick'));
            _calendar->SetInvocationMessage(new BMessage('pick'));
            _calendar->SetTarget(this);
            // The dropdown covers its field and keeps seven legible day
            // columns even when the application gives the editor little room.
            const float column = std::ceil(_calendar->StringWidth("00")) + 8;
            const float width = std::max({anchor.Bounds().Width(),
                _calendar->MinSize().width + 16, 7 * column + 24});
            _calendar->ResizeTo(width - 16, _calendar->Bounds().Height());
            BTextControl standard("measure", nullptr, "", nullptr);
            const float button_side = std::ceil(standard.PreferredSize().height);
            const float header_bottom = 6 + button_side;
            _calendar->MoveTo(8, header_bottom + 6);
            auto *previous = new BButton(BRect(8, 6, 8 + button_side, header_bottom),
                "previous", "<", new BMessage('prev'));
            auto *next = new BButton(BRect(width - 8 - button_side, 6, width - 8, header_bottom),
                "next", ">", new BMessage('next'));
            _month = new BStringView(BRect(16 + button_side, 6,
                width - 16 - button_side, header_bottom), "month", "");
            _month->SetAlignment(B_ALIGN_CENTER);
            AddChild(previous); AddChild(next); AddChild(_month);
            previous->SetTarget(this); next->SetTarget(this);
            ResizeTo(width, _calendar->Frame().bottom + 8);
            update_month();
            BPoint point(0, anchor.Bounds().bottom + 2);
            anchor.ConvertToScreen(&point);
            MoveTo(point);
        }
        ~calendar_window() override {
            BMessage closed(calendar_closed);
            closed.AddMessenger("popup", _identity);
            _trigger.SendMessage(&closed);
        }
        void DispatchMessage(BMessage *message, BHandler *target) override {
            if (message->what == B_QUIT_REQUESTED) { BWindow::DispatchMessage(message, target); return; }
            haiku::ui_callback callback(*this);
            BWindow::DispatchMessage(message, target);
        }
        void MessageReceived(BMessage *message) override {
            if (message->what == 'pick') { _calendar->commit(); return; }
            if (message->what == 'prev' || message->what == 'next') {
                auto value = _calendar->Date();
                value.AddMonths(message->what == 'next' ? 1 : -1);
                _calendar->SetDate(value); update_month(); return;
            }
            BWindow::MessageReceived(message);
        }
        void WindowActivated(bool active) override {
            if (active) return;
            BPoint mouse;
            uint32 buttons;
            _calendar->GetMouse(&mouse, &buttons, false);
            _calendar->ConvertToScreen(&mouse);
            // Let the original arrow consume this click and close its open
            // popup, instead of closing here and reopening on mouse-up.
            if (buttons && _toggle_bounds.Contains(mouse)) return;
            PostMessage(B_QUIT_REQUESTED);
        }
    private:
        calendar_view *_calendar;
        BStringView *_month;
        BMessenger _trigger;
        BMessenger _identity;
        BRect _toggle_bounds;
        void update_month() {
            const native::date value{_calendar->Year(), unsigned(_calendar->Month()), unsigned(_calendar->Day())};
            _month->SetText(value.to_string().substr(0, 7).c_str());
        }
    };
    class calendar_button final : public BButton
    {
    public:
        explicit calendar_button(native::date_picker &owner)
            : BButton("calendar", nullptr, nullptr), _owner(owner) {
            SetFlat(true); SetToolTip("Choose a date");
        }
        void Draw(BRect update) override {
            PushState();
            SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
            FillRect(Bounds(), B_SOLID_LOW);
            // DateEdit owns the single frame and paper. Draw the same
            // native arrow as combos without adding a second button surface.
            haiku::draw_combo_indicator(*this, Bounds(), update);
            PopState();
        }
        ~calendar_button() override { _popup.SendMessage(B_QUIT_REQUESTED); }
        status_t Invoke(BMessage * = nullptr) override {
            if (_popup.IsValid()) {
                _popup.SendMessage(B_QUIT_REQUESTED);
                _popup = BMessenger();
                SetValue(B_CONTROL_OFF);
                return B_OK;
            }
            SetValue(B_CONTROL_ON);
            auto *popup = new calendar_window(_owner, *Parent(), *this);
            _popup = BMessenger(popup);
            popup->Show(); return B_OK;
        }
        void MessageReceived(BMessage *message) override {
            if (message->what == calendar_closed) {
                BMessenger closed;
                if (message->FindMessenger("popup", &closed) == B_OK && closed == _popup) {
                    _popup = BMessenger();
                    SetValue(B_CONTROL_OFF);
                }
            } else BButton::MessageReceived(message);
        }
        void MouseUp(BPoint point) override {
            if (!Bounds().Contains(point) && _popup.IsValid()) {
                _popup.SendMessage(B_QUIT_REQUESTED);
                _popup = BMessenger();
                SetValue(B_CONTROL_OFF);
            }
            BButton::MouseUp(point);
        }
    private:
        native::date_picker &_owner;
        BMessenger _popup;
    };
    class date_edit final : public fitted_edit<BPrivate::DateEdit, false>
    {
    public:
        explicit date_edit(native::date_picker &owner)
            : fitted_edit("native_date", 3, new BMessage(0x6e446174)), _owner(owner) {}
        status_t Invoke(BMessage * = nullptr) override {
            if (_updating) return B_OK;
            const auto value = GetDate();
            return _owner.on_native_date({value.Year(), unsigned(value.Month()), unsigned(value.Day())})
                ? B_OK : B_BAD_VALUE;
        }
        void synchronize() {
            _updating = true;
            const auto value = _owner.get_date();
            SetDate(value.year, value.month, value.day);
            if (FocusIndex() >= 0) SectionFocus(FocusIndex());
            SetToolTip(_owner.get_valid() ? nullptr : "The current date is unavailable.");
            _updating = false;
        }
    private:
        native::date_picker &_owner;
        bool _updating = false;
    };
    class time_edit final : public fitted_edit<BPrivate::TimeEdit, true>
    {
    public:
        explicit time_edit(native::time_picker &owner)
            : fitted_edit("native_time", 3, new BMessage(0x6e54696d)), _owner(owner) {}
        status_t Invoke(BMessage * = nullptr) override {
            if (_updating) return B_OK;
            const auto value = GetTime();
            return _owner.on_native_time(native::detail::stepped_time(_owner,
                {unsigned(value.Hour()), unsigned(value.Minute()),
                 _owner.get_seconds_visible() ? unsigned(value.Second()) : _owner.get_time().second})) ? B_OK : B_BAD_VALUE;
        }
        void synchronize() {
            _updating = true;
            fSectionCount = _owner.get_seconds_visible() ? 3 : 2;
            if (fFocus >= int32(fSectionCount)) fFocus = fSectionCount - 1;
            const auto value = _owner.get_time();
            SetTime(value.hour, value.minute, value.second);
            if (FocusIndex() >= 0) SectionFocus(FocusIndex());
            SetToolTip(_owner.get_valid() ? nullptr : "The current time is unavailable.");
            InvalidateLayout();
            _updating = false;
        }
    private:
        native::time_picker &_owner;
        bool _updating = false;
    };
    struct picker_state {
        date_edit *date = nullptr;
        time_edit *time = nullptr;
        calendar_button *calendar = nullptr;
    };
    BView *host(native::panel &owner) { return haiku::panel_bindings.object_from_handle(&owner)->view; }
    void geometry(native::panel &owner, BView &view) {
        const auto area = owner.get_client_bounds();
        const auto preferred = view.PreferredSize();
        const float width = std::min<float>(area.w(), preferred.width + 1);
        const float height = std::min<float>(area.h(), preferred.height + 1);
        view.MoveTo(area.x1(), area.y1() + (area.h() - height) / 2);
        view.ResizeTo(std::max(0.0f, width - 1), std::max(0.0f, height - 1));
    }
}
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) {
        BAutolock lock(host(owner)->Window());
        if (!lock.IsLocked()) throw std::runtime_error("Cannot lock Haiku picker parent.");
        auto *peer = new picker_state;
        assign_peer_state(owner, peer);
        peer->date = new date_edit(owner); host(owner)->AddChild(peer->date);
        peer->calendar = new calendar_button(owner); peer->date->AddChild(peer->calendar);
        peer->calendar->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        peer->calendar->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
        return true;
    }
    bool create_time_picker_peer(time_picker &owner) {
        BAutolock lock(host(owner)->Window());
        if (!lock.IsLocked()) throw std::runtime_error("Cannot lock Haiku picker parent.");
        auto *peer = new picker_state;
        assign_peer_state(owner, peer);
        peer->time = new time_edit(owner); host(owner)->AddChild(peer->time); return true;
    }
    void refresh_date_picker_peer(date_picker &owner) {
        auto *peer = peer_state<picker_state>(owner);
        if (!peer || !peer->date) return;
        BAutolock lock(host(owner)->Window());
        if (!lock.IsLocked()) return;
        peer->date->synchronize(); geometry(owner, *peer->date);
        const auto area = peer->date->Bounds();
        const auto indicator = haiku::combo_indicator_bounds(area);
        peer->calendar->ResizeTo(indicator.Width(), indicator.Height());
        peer->calendar->MoveTo(indicator.LeftTop());
    }
    void refresh_time_picker_peer(time_picker &owner) {
        auto *peer = peer_state<picker_state>(owner);
        if (!peer || !peer->time) return;
        BAutolock lock(host(owner)->Window());
        if (!lock.IsLocked()) return;
        peer->time->synchronize(); geometry(owner, *peer->time);
    }
}

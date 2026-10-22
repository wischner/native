//
// Hosts Win32 common-controls date/time pickers in their Native panel.
// The shared model validates arbitrary exclusions after native proposals.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../picker_peer.h"
#include <stdexcept>

namespace
{
    struct picker_state
    {
        HWND parent = nullptr, child = nullptr;
        native::date_picker *date = nullptr;
        native::time_picker *time = nullptr;
        bool updating = false;
    };
    SYSTEMTIME system_time(native::date value) {
        SYSTEMTIME result{};
        result.wYear = value.year; result.wMonth = value.month; result.wDay = value.day;
        return result;
    }
    SYSTEMTIME system_time(native::time_of_day value) {
        SYSTEMTIME result{};
        result.wYear = 2000; result.wMonth = 1; result.wDay = 1;
        result.wHour = value.hour; result.wMinute = value.minute; result.wSecond = value.second;
        return result;
    }
    // Literal spaces keep the native segmented editor clear of its frame.
    // Read the current locale each refresh so date ordering stays local.
    void date_format(HWND child) {
        wchar_t format[128]{};
        if (!GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE,
                            format, 128))
            throw std::runtime_error("Cannot read the Windows date format.");
        const std::wstring padded = L"' '" + std::wstring(format) + L"' '";
        DateTime_SetFormat(child, padded.c_str());
    }
    LRESULT CALLBACK notification(HWND window, UINT message, WPARAM wparam,
                                   LPARAM lparam, UINT_PTR id, DWORD_PTR data) {
        auto *peer = reinterpret_cast<picker_state *>(data);
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, notification, id);
        if (message == WM_NOTIFY && lparam) {
            auto *event = reinterpret_cast<NMDATETIMECHANGE *>(lparam);
            if (event->nmhdr.hwndFrom == peer->child && event->nmhdr.code == DTN_DATETIMECHANGE) {
                if (peer->updating || event->dwFlags != GDT_VALID) return 0;
                const auto value = event->st;
                if (peer->date) peer->date->on_native_date({value.wYear, value.wMonth, value.wDay});
                else peer->time->on_native_time(native::detail::stepped_time(*peer->time,
                    {value.wHour, value.wMinute,
                     peer->time->get_seconds_visible() ? unsigned(value.wSecond) : peer->time->get_time().second}));
                return 0;
            }
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }
    picker_state &create(native::panel &owner, bool time) {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_DATE_CLASSES};
        if (!InitCommonControlsEx(&controls)) throw std::runtime_error("Cannot initialize date controls.");
        auto *peer = new picker_state;
        native::detail::assign_peer_state(owner, peer);
        peer->parent = windows::wnd_bindings.handle_from_object(&owner);
        peer->child = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | (time ? DTS_TIMEFORMAT : DTS_SHORTDATECENTURYFORMAT),
            0, 0, owner.get_dimensions().w, owner.get_dimensions().h, peer->parent,
            nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!peer->child) throw std::runtime_error("Cannot create date/time picker.");
        if (!SetWindowSubclass(peer->parent, notification, 1, reinterpret_cast<DWORD_PTR>(peer)))
            throw std::runtime_error("Cannot route date/time picker notifications.");
        SendMessageW(peer->child, WM_SETFONT, reinterpret_cast<WPARAM>(windows::control_font()), TRUE);
        return *peer;
    }
    void geometry(native::panel &owner, picker_state &peer) {
        const auto area = owner.get_client_bounds();
        SIZE preferred{};
        SendMessageW(peer.child, DTM_GETIDEALSIZE, 0,
                     reinterpret_cast<LPARAM>(&preferred));
        const int width = std::min<int>(area.w(), std::max<LONG>(preferred.cx, 80));
        const int height = area.h();
        MoveWindow(peer.child, area.x1(), area.y1() + (int(area.h()) - height) / 2,
                   width, height, TRUE);
    }
}
namespace native::detail
{
    bool create_date_picker_peer(date_picker &owner) { create(owner, false).date = &owner; return true; }
    bool create_time_picker_peer(time_picker &owner) { create(owner, true).time = &owner; return true; }
    void refresh_date_picker_peer(date_picker &owner) {
        auto *peer = peer_state<picker_state>(owner);
        if (!peer || !peer->child) return;
        peer->updating = true;
        date_format(peer->child);
        const auto value = system_time(owner.get_date());
        SYSTEMTIME limits[] = {system_time(owner.get_minimum_date()), system_time(owner.get_maximum_date())};
        const bool in_range = owner.get_date() >= owner.get_minimum_date() && owner.get_date() <= owner.get_maximum_date();
        DateTime_SetRange(peer->child, in_range ? GDTR_MIN | GDTR_MAX : 0, limits);
        DateTime_SetSystemtime(peer->child, GDT_VALID, &value);
        geometry(owner, *peer); peer->updating = false;
    }
    void refresh_time_picker_peer(time_picker &owner) {
        auto *peer = peer_state<picker_state>(owner);
        if (!peer || !peer->child) return;
        peer->updating = true;
        const auto value = system_time(owner.get_time());
        SYSTEMTIME limits[] = {system_time(owner.get_minimum_time()), system_time(owner.get_maximum_time())};
        const bool in_range = owner.get_time() >= owner.get_minimum_time() && owner.get_time() <= owner.get_maximum_time();
        DateTime_SetRange(peer->child, in_range ? GDTR_MIN | GDTR_MAX : 0, limits);
        DateTime_SetFormat(peer->child, owner.get_seconds_visible() ? L"' 'HH:mm:ss' '" : L"' 'HH:mm' '");
        DateTime_SetSystemtime(peer->child, GDT_VALID, &value);
        geometry(owner, *peer); peer->updating = false;
    }
}

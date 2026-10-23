//
// Centers cached WINGs standard panels after mapping so their default
// screen placement cannot override parent-relative geometry.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "dialog_position.h"
namespace
{
    void center(WMWindow *dialog, WMWindow *owner) {
        const auto origin = linux::wmaker::screen_position(owner);
        const auto parent = WMGetViewSize(WMWidgetView(owner));
        const auto child = WMGetViewSize(WMWidgetView(dialog));
        WMMoveWidget(
            dialog,
            origin.x + (int(parent.width) - int(child.width)) / 2,
            origin.y + (int(parent.height) - int(child.height)) / 2);
    }
    void mapped(XEvent *event, void *data) {
        if (event->type != MapNotify)
            return;
        auto *owner = static_cast<WMWindow *>(data);
        const auto origin = linux::wmaker::screen_position(owner);
        const auto parent = WMGetViewSize(WMWidgetView(owner));
        XWindowAttributes child{};
        XGetWindowAttributes(
            event->xmap.display, event->xmap.window, &child);
        XWindowChanges geometry{};
        geometry.x = origin.x + (int(parent.width) - child.width) / 2;
        geometry.y = origin.y + (int(parent.height) - child.height) / 2;
        XReconfigureWMWindow(event->xmap.display,
                             event->xmap.window,
                             DefaultScreen(event->xmap.display),
                             CWX | CWY,
                             &geometry);
    }
} // namespace
namespace linux::wmaker
{
    WMPoint screen_position(WMWidget *widget) {
        Display *display = WMScreenDisplay(WMWidgetScreen(widget));
        Window child = None;
        int x = 0, y = 0;
        XTranslateCoordinates(display,
                              WMWidgetXID(widget),
                              DefaultRootWindow(display),
                              0,
                              0,
                              &x,
                              &y,
                              &child);
        return WMPoint{x, y};
    }

    void position_dialog(WMWindow *dialog, WMWindow *owner) {
        center(dialog, owner);
        WMCreateEventHandler(
            WMWidgetView(dialog), StructureNotifyMask, mapped, owner);
    }
    void finish_position_dialog(WMWindow *dialog, WMWindow *owner) {
        WMDeleteEventHandler(
            WMWidgetView(dialog), StructureNotifyMask, mapped, owner);
    }
} // namespace linux::wmaker

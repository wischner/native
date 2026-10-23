//
// Centers native Motif dialogs using final preferred sizes and the
// owner's client coordinates. The map callback runs after child
// geometry negotiation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "dialog_position.h"
#include <Xm/Xm.h>
#include <X11/Shell.h>
namespace
{
    // A reparenting manager interprets requested positions at its
    // frame. Correct the final client origin after that frame has been
    // installed.
    void
    mapped(Widget shell, XtPointer data, XEvent *event, Boolean *) {
        if (event->type != MapNotify)
            return;
        auto owner = static_cast<Widget>(data);
        Display *display = XtDisplay(shell);
        Window root = None, frame = XtWindow(shell), parent = None;
        for (int depth = 0; depth < 16; ++depth) {
            Window *children = nullptr;
            unsigned count = 0;
            const bool queried = XQueryTree(
                display, frame, &root, &parent, &children, &count);
            if (children)
                XFree(children);
            if (!queried || parent == root || parent == None)
                break;
            frame = parent;
        }
        int inset_x = 0, inset_y = 0;
        Window child = None;
        if (frame != XtWindow(shell))
            XTranslateCoordinates(display,
                                  XtWindow(shell),
                                  frame,
                                  0,
                                  0,
                                  &inset_x,
                                  &inset_y,
                                  &child);
        Dimension width = 0, height = 0, owner_width = 0,
                  owner_height = 0;
        XtVaGetValues(
            shell, XtNwidth, &width, XtNheight, &height, nullptr);
        XtVaGetValues(owner,
                      XtNwidth,
                      &owner_width,
                      XtNheight,
                      &owner_height,
                      nullptr);
        Position x = 0, y = 0;
        XtTranslateCoords(owner,
                          (int(owner_width) - width) / 2,
                          (int(owner_height) - height) / 2,
                          &x,
                          &y);
        XWindowChanges geometry{};
        geometry.x = x - inset_x;
        geometry.y = y - inset_y;
        XReconfigureWMWindow(display,
                             XtWindow(shell),
                             DefaultScreen(display),
                             CWX | CWY,
                             &geometry);
    }
    void center(Widget dialog, XtPointer data, XtPointer) {
        auto owner = static_cast<Widget>(data);
        Dimension width = 0, height = 0, owner_width = 0,
                  owner_height = 0;
        XtVaGetValues(
            dialog, XmNwidth, &width, XmNheight, &height, nullptr);
        XtVaGetValues(owner,
                      XmNwidth,
                      &owner_width,
                      XmNheight,
                      &owner_height,
                      nullptr);
        Position x = 0, y = 0;
        XtTranslateCoords(owner,
                          (int(owner_width) - width) / 2,
                          (int(owner_height) - height) / 2,
                          &x,
                          &y);
        XtVaSetValues(XtParent(dialog), XtNx, x, XtNy, y, nullptr);
    }
} // namespace
namespace linux::openmotif
{
    void position_dialog(Widget dialog, Widget owner) {
        XtVaSetValues(
            XtParent(dialog), XtNwinGravity, StaticGravity, nullptr);
        XtVaSetValues(dialog, XmNdefaultPosition, False, nullptr);
        XtAddCallback(dialog, XmNmapCallback, center, owner);
        XtAddEventHandler(XtParent(dialog),
                          StructureNotifyMask,
                          False,
                          mapped,
                          owner);
    }
} // namespace linux::openmotif

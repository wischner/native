//
// Athena Form specialization for containers whose geometry is owned by
// Native layout. Children must not shrink their parent to a preferred size
// or unmap the entire container while a pane is being resized.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <X11/IntrinsicP.h>
#include <X11/Xaw/FormP.h>

#include "globals.h"

namespace
{
    Boolean layout(FormWidget host, unsigned int, unsigned int, Bool) {
        for (Cardinal i = 0; i < host->composite.num_children; ++i) {
            Widget child = host->composite.children[i];
            if (!XtIsManaged(child)) continue;
            const auto &constraints = static_cast<FormConstraints>(
                child->core.constraints)->form;
            int x = constraints.dx, y = constraints.dy;
            if (Widget base = constraints.horiz_base)
                x += base->core.x + base->core.width + 2 * base->core.border_width;
            if (Widget base = constraints.vert_base)
                y += base->core.y + base->core.height + 2 * base->core.border_width;
            XtMoveWidget(child, x, y);
        }
        host->form.preferred_width = host->core.width;
        host->form.preferred_height = host->core.height;
        host->form.needs_relayout = False;
        return True;
    }

    void managed(Widget host) {
        auto *form = reinterpret_cast<FormWidget>(host);
        // Only initial management enforces outer portable bounds here.
        // A SetValues geometry request must complete through Xt itself;
        // rewriting that child's size inside its constraint callback can
        // suppress the server resize and the resulting ConfigureNotify.
        for (Cardinal i = 0; i < form->composite.num_children; ++i) {
            Widget child = form->composite.children[i];
            auto *owner = linux::x11::wnd_bindings.object_from_handle(child);
            if (!owner || !XtIsManaged(child)) continue;
            const auto d = owner->get_dimensions();
            const int border = 2 * child->core.border_width;
            XtResizeWidget(child,
                linux::x11::widget_dimension(int(d.w) - border),
                linux::x11::widget_dimension(int(d.h) - border),
                child->core.border_width);
        }
        layout(reinterpret_cast<FormWidget>(host), 0, 0, True);
    }

    // Resize notifications drive Native layout; Form must not do another
    // proportional resize, or temporarily unmap all of the child windows.
    void resized(Widget) {}

    Dimension requested_dimension(Widget child, Dimension value, bool horizontal) {
        if (value) return value;
        // Xaw Paned temporarily requests a zero cross-axis size when its
        // orientation changes. Keep the assigned portable extent instead
        // of sending that internal sentinel to XConfigureWindow.
        const auto *owner = linux::x11::wnd_bindings.object_from_handle(child);
        if (!owner) return 1;
        const auto size = owner->get_dimensions();
        return linux::x11::widget_dimension(
            int(horizontal ? size.w : size.h) - 2 * child->core.border_width);
    }

    XtGeometryResult geometry(Widget child, XtWidgetGeometry *request,
                              XtWidgetGeometry *) {
        if (request->request_mode & XtCWQueryOnly) return XtGeometryYes;
        const auto mode = request->request_mode;
        XtConfigureWidget(child,
            mode & CWX ? request->x : child->core.x,
            mode & CWY ? request->y : child->core.y,
            requested_dimension(child,
                mode & CWWidth ? request->width : child->core.width, true),
            requested_dimension(child,
                mode & CWHeight ? request->height : child->core.height, false),
            mode & CWBorderWidth ? request->border_width : child->core.border_width);
        layout(reinterpret_cast<FormWidget>(XtParent(child)), 0, 0, True);
        return XtGeometryDone;
    }

    Boolean constraints_changed(Widget, Widget, Widget child,
                                 ArgList, Cardinal *) {
        layout(reinterpret_cast<FormWidget>(XtParent(child)), 0, 0, True);
        return False;
    }

    FormClassRec make_class() {
        FormClassRec result{};
        auto &core = result.core_class;
        core.superclass = formWidgetClass;
        core.class_name = const_cast<char *>("NativeLayoutHost");
        core.widget_size = sizeof(FormRec);
        core.realize = XtInheritRealize;
        core.compress_motion = True;
        core.compress_exposure = XtExposeCompressMultiple;
        core.compress_enterleave = True;
        core.resize = resized;
        core.expose = XtInheritExpose;
        core.set_values_almost = XtInheritSetValuesAlmost;
        core.version = XtVersion;
        core.tm_table = XtInheritTranslations;
        core.query_geometry = XtInheritQueryGeometry;
        core.display_accelerator = XtInheritDisplayAccelerator;
        result.composite_class.geometry_manager = geometry;
        result.composite_class.change_managed = managed;
        result.composite_class.insert_child = XtInheritInsertChild;
        result.composite_class.delete_child = XtInheritDeleteChild;
        result.constraint_class.constraint_size = sizeof(FormConstraintsRec);
        result.constraint_class.set_values = constraints_changed;
        result.form_class.layout = layout;
        return result;
    }
}

namespace linux::x11
{
    WidgetClass layout_host_class() {
        static FormClassRec record = make_class();
        return reinterpret_cast<WidgetClass>(&record);
    }
}

//
// Keeps native editor, tab and value-widget fonts aligned with the same
// Motif control-font resource used by portable labels and measurements.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "control_font.h"
#include "globals.h"
#include <native/font.h>
namespace linux::openmotif
{
    void apply_control_font(Widget widget) {
        const auto &font =
            native::font_t::stock(native::font_role::control);
        auto *resource = font_bindings.object_from_handle(font.id());
        if (!widget || !resource || !resource->metrics)
            return;
        auto fonts = XmFontListCreate(
            resource->metrics,
            const_cast<char *>(XmFONTLIST_DEFAULT_TAG));
        XtVaSetValues(widget, XmNfontList, fonts, nullptr);
        XmFontListFree(fonts);
    }
} // namespace linux::openmotif

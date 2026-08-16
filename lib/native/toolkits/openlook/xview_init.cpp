//
// Initializes XView and supplies usable core-font resources when an X
// server does not provide the historical Lucida OPEN LOOK fonts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "xview_init.h"

#include <stdexcept>
#include <string>
#include <vector>

#include <native/app.h>

#include "globals.h"

#include <X11/Xlib.h>
#include <xview/defaults.h>
#include <xview/server.h>
#include <xview/xview.h>

namespace linux::openlook
{
    namespace
    {
        bool font_available(Display *display, const char *name) {
            int count = 0;
            char **fonts = display
                               ? XListFonts(display, name, 1, &count)
                               : nullptr;
            if (fonts)
                XFreeFontNames(fonts);
            return count > 0;
        }
    } // namespace

    void initialize_xview() {
        if (initialized)
            return;

        Display *probe = cached_display;
        int font_count = 0;
        char **lucida_fonts = probe
                                  ? XListFonts(
                                        probe,
                                        "*-lucida-*",
                                        1,
                                        &font_count)
                                  : nullptr;
        if (lucida_fonts)
            XFreeFontNames(lucida_fonts);

        int argc = native::app::argc;
        char **argv = native::app::argv;
        std::vector<std::string> resources = {
            "OpenWindows.KeyboardCommand.Again: "
            "a+Ctrl+Meta,L2",
            "OpenWindows.KeyboardCommand.Undo: z+Ctrl,L4",
            "OpenWindows.KeyboardCommand.Copy: c+Ctrl,L6",
            "OpenWindows.KeyboardCommand.Paste: v+Ctrl,L8",
            "OpenWindows.KeyboardCommand.FindForward: "
            "f+Ctrl+Meta,L9",
            "OpenWindows.KeyboardCommand.FindBackward: "
            "F+Ctrl+Meta,L9+Shift",
            "OpenWindows.KeyboardCommand.Cut: x+Ctrl,L10",
            "OpenWindows.KeyboardCommand.DefaultAction: "
            "Return+Ctrl+Meta",
            "OpenWindows.KeyboardCommand.CopyThenPaste: "
            "p+Ctrl+Meta",
            "OpenWindows.KeyboardCommand.MatchDelimiter: "
            "d+Ctrl+Meta",
            "OpenWindows.KeyboardCommand.Empty: e+Ctrl+Meta",
            "OpenWindows.KeyboardCommand.IncludeFile: "
            "i+Ctrl+Meta",
            "OpenWindows.KeyboardCommand.Load: l+Ctrl+Meta",
            "OpenWindows.KeyboardCommand.Store: s+Ctrl+Meta"};
        std::vector<char *> xview_arguments;
        char xrm_option[] = "-xrm";
        constexpr const char *fixed_regular =
            "-misc-fixed-medium-r-normal--15-120-100-100-"
            "c-90-iso8859-1";
        constexpr const char *fixed_bold =
            "-misc-fixed-bold-r-normal--15-120-100-100-"
            "c-90-iso8859-1";
        std::string regular_font =
            font_available(probe, fixed_regular)
                ? fixed_regular
                : "fixed";
        std::string bold_font =
            font_available(probe, fixed_bold)
                ? fixed_bold
                : regular_font;
        std::string regular_resource =
            "OpenWindows.RegularFont: " + regular_font;
        std::string bold_resource =
            "OpenWindows.BoldFont: " + bold_font;
        std::string mono_resource =
            "OpenWindows.MonospaceFont: " + regular_font;
        if (probe && font_count == 0) {
            resources.push_back(regular_resource);
            resources.push_back(bold_resource);
            resources.push_back(mono_resource);
        }

        if (argv && argc > 0)
            xview_arguments.assign(argv, argv + argc);
        for (std::string &resource : resources) {
            xview_arguments.push_back(xrm_option);
            xview_arguments.push_back(resource.data());
        }
        xview_arguments.push_back(nullptr);
        argc = static_cast<int>(xview_arguments.size() - 1);
        argv = xview_arguments.data();

        xv_init(XV_INIT_ARGC_PTR_ARGV,
                &argc,
                argv,
                XV_USE_LOCALE,
                TRUE,
                nullptr);
        auto *display = reinterpret_cast<Display *>(
            xv_get(xv_default_server, XV_DISPLAY));
        if (!display) {
            throw std::runtime_error(
                "OpenLook/XView: failed to initialize XView.");
        }

        if (font_count == 0) {
            char regular_name[] =
                "openwindows.regularfont";
            char bold_name[] = "openwindows.boldfont";
            char mono_name[] =
                "openwindows.monospacefont";
            defaults_set_string(
                regular_name, regular_font.data());
            defaults_set_string(bold_name, bold_font.data());
            defaults_set_string(mono_name, regular_font.data());
        }
        if (probe && probe != display)
            XCloseDisplay(probe);
        cached_display = display;
        initialized = true;
    }
} // namespace linux::openlook

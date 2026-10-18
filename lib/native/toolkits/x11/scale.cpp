//
// Implements the X11 desktop-scale boundary: reads Xft.dpi once,
// converts between logical and device pixels, scales the Athena
// spacing resources, and selects device-size core fonts and bitmaps.
// Strokes stay one device pixel; text uses hand-tuned bitmap faces at
// the nearest installed size instead of server-magnified glyphs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "scale.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>

#include "globals.h"

namespace
{
    constexpr double reference_dpi = 96.0;
    constexpr double maximum_scale = 4.0;

    std::string control_font_name;
    std::string default_font_name;
    bool default_font_known = false;

    // Athena spacing resources and their stock defaults, scaled
    // together. Border, highlight and rule widths are strokes and keep
    // their device-pixel defaults.
    struct pixel_resource
    {
        const char *specifier;
        int value;
    };
    constexpr pixel_resource pixel_resources[] = {
        {"*Label.internalWidth", 4},
        {"*Label.internalHeight", 2},
        {"*Command.internalWidth", 4},
        {"*Command.internalHeight", 2},
        {"*Toggle.internalWidth", 4},
        {"*Toggle.internalHeight", 2},
        {"*MenuButton.internalWidth", 4},
        {"*MenuButton.internalHeight", 2},
        {"*List.internalWidth", 4},
        {"*List.internalHeight", 2},
        {"*List.columnSpacing", 6},
        {"*List.rowSpacing", 2},
        {"*Scrollbar.thickness", 14},
        {"*Scrollbar.minimumThumb", 7},
        {"*SmeBSB.leftMargin", 4},
        {"*SmeBSB.rightMargin", 4},
        {"*Text.leftMargin", 2},
        {"*Text.rightMargin", 4},
        {"*Text.topMargin", 2},
        {"*Text.bottomMargin", 2},
        {"*Paned.internalBorderWidth", 1},
        {"*Paned.gripIndent", 10},
        {"*Grip.width", 8},
        {"*Grip.height", 8},
        {"*Box.hSpace", 4},
        {"*Box.vSpace", 4},
        {"*Form.defaultDistance", 4},
        {"*Dialog.defaultDistance", 4},
    };

    // Read Xft.dpi from the display's root resource database.
    // Returns 0 when the resource is unavailable.
    double read_xft_dpi(Display *display) {
        const char *resources = XResourceManagerString(display);
        if (!resources)
            return 0.0;

        double dpi = 0.0;
        XrmInitialize();
        XrmDatabase database = XrmGetStringDatabase(resources);
        if (database) {
            char *type = nullptr;
            XrmValue value{};
            if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type,
                               &value) &&
                value.addr)
                dpi = std::strtod(value.addr, nullptr);
            XrmDestroyDatabase(database);
        }
        return dpi;
    }

    double detect_scale(Display *display) {
        const double dpi = read_xft_dpi(display);
        if (!(dpi > 0.0))
            return 1.0;
        // Quarter steps keep near-96 values (for example 100 DPI) at 1
        // and make logical/device round trips stable.
        const double factor =
            std::round(dpi / reference_dpi * 4.0) / 4.0;
        return std::clamp(factor, 1.0, maximum_scale);
    }

    // Return a core font's XLFD name, or an empty string.
    std::string font_name(Display *display, const XFontStruct *font) {
        unsigned long atom = 0;
        if (!font ||
            !XGetFontProperty(const_cast<XFontStruct *>(font), XA_FONT,
                              &atom))
            return {};
        char *name = XGetAtomName(display, static_cast<Atom>(atom));
        if (!name)
            return {};
        std::string result(name);
        XFree(name);
        return result;
    }

    // Split an XLFD into its fourteen fields; empty for other names.
    std::vector<std::string> xlfd_fields(const std::string &name) {
        std::vector<std::string> fields;
        std::size_t start = 0;
        for (;;) {
            const std::size_t dash = name.find('-', start);
            fields.push_back(name.substr(start, dash - start));
            if (dash == std::string::npos)
                break;
            start = dash + 1;
        }
        // An XLFD starts with a dash and has fourteen fields.
        if (fields.size() != 15 || !fields[0].empty())
            return {};
        return fields;
    }

    struct font_family
    {
        const char *foundry;
        const char *family;
    };

    // Hand-tuned bitmap families tried after the logical font's own,
    // by spacing. Each provides many sizes in the 75dpi/100dpi sets.
    constexpr font_family proportional_families[] = {
        {"adobe", "helvetica"},
        {"b&h", "lucida"},
    };
    constexpr font_family monospace_families[] = {
        {"b&h", "lucidatypewriter"},
        {"xos4", "terminus"},
        {"misc", "fixed"},
    };

    //
    // Select the installed, unscaled face nearest a logical font at
    // device size. The logical family wins ties, then the listed
    // families in order. Server-scaled instances are never chosen.
    //
    std::string device_font_name(Display *display,
                                 const std::string &logical) {
        const auto fields = xlfd_fields(logical);
        if (fields.empty())
            return {};
        const int pixels = std::atoi(fields[7].c_str());
        if (pixels <= 0)
            return {};
        const int target = linux::x11::to_device(pixels);
        const std::string &weight = fields[3];
        const bool proportional = fields[11] == "p" || fields[11] == "P";

        std::vector<font_family> families = {
            {fields[1].c_str(), fields[2].c_str()}};
        for (const auto &family : proportional ? std::vector<font_family>(
                 std::begin(proportional_families),
                 std::end(proportional_families))
                 : std::vector<font_family>(std::begin(monospace_families),
                                            std::end(monospace_families)))
            families.push_back(family);

        std::string best;
        int best_distance = 0;
        for (const font_family &family : families) {
            const std::string pattern = std::string("-") + family.foundry +
                "-" + family.family + "-" + weight +
                "-r-normal-*-*-*-*-*-*-*-iso8859-1";
            int count = 0;
            char **names =
                XListFonts(display, pattern.c_str(), 1000, &count);
            for (int index = 0; index < count; ++index) {
                const auto candidate = xlfd_fields(names[index]);
                const int size = candidate.empty()
                    ? 0 : std::atoi(candidate[7].c_str());
                if (size <= 0)
                    continue;
                const int distance = std::abs(size - target);
                if (best.empty() || distance < best_distance) {
                    best = names[index];
                    best_distance = distance;
                }
            }
            if (names)
                XFreeFontNames(names);
            if (!best.empty() && best_distance == 0)
                break;
        }
        return best;
    }

    void put_resource(XrmDatabase *database,
                      const char *specifier,
                      const std::string &value) {
        XrmPutStringResource(database, specifier, value.c_str());
    }
} // namespace

namespace linux::x11
{
    double coordinate_scale() {
        static double scale = 0.0;
        if (scale > 0.0)
            return scale;
        if (!cached_display)
            return 1.0;
        scale = detect_scale(cached_display);
        return scale;
    }

    int to_device(int logical) {
        return static_cast<int>(
            std::lround(logical * coordinate_scale()));
    }

    int to_logical(int device) {
        return static_cast<int>(
            std::lround(device / coordinate_scale()));
    }

    int pointer_to_logical(int device) {
        return static_cast<int>(
            std::floor(device / coordinate_scale()));
    }

    native::point pointer_to_logical(int x, int y) {
        return native::point(
            static_cast<native::coord>(pointer_to_logical(x)),
            static_cast<native::coord>(pointer_to_logical(y)));
    }

    native::size size_to_logical(int width, int height) {
        return native::size(
            static_cast<native::dim>(std::max(0, to_logical(width))),
            static_cast<native::dim>(std::max(0, to_logical(height))));
    }

    native::rect rect_to_logical(const native::rect &device) {
        const int left = to_logical(device.p.x);
        const int top = to_logical(device.p.y);
        return native::rect(
            static_cast<native::coord>(left),
            static_cast<native::coord>(top),
            static_cast<native::dim>(std::max(
                0, to_logical(device.p.x + int(device.d.w)) - left)),
            static_cast<native::dim>(std::max(
                0, to_logical(device.p.y + int(device.d.h)) - top)));
    }

    native::rect rect_to_device(const native::rect &logical) {
        const int left = to_device(logical.p.x);
        const int top = to_device(logical.p.y);
        return native::rect(
            static_cast<native::coord>(left),
            static_cast<native::coord>(top),
            static_cast<native::dim>(std::max(
                0, to_device(logical.p.x + int(logical.d.w)) - left)),
            static_cast<native::dim>(std::max(
                0, to_device(logical.p.y + int(logical.d.h)) - top)));
    }

    Dimension device_dimension(int logical) {
        return widget_dimension(to_device(logical));
    }

    void scale_resources(Widget shell) {
        Display *display = XtDisplay(shell);

        // Probe the font a control receives from the unscaled database.
        Widget probe =
            XtVaCreateWidget("font_probe", commandWidgetClass, shell,
                             nullptr);
        if (probe) {
            XFontStruct *font = nullptr;
            XtVaGetValues(probe, XtNfont, &font, nullptr);
            control_font_name = font_name(display, font);
            XtDestroyWidget(probe);
        }

        if (coordinate_scale() == 1.0)
            return;

        if (const char *font = XGetDefault(display, "*", "font"))
            default_font_name = font;
        default_font_known = true;
        XrmDatabase database = XtDatabase(display);
        for (const pixel_resource &resource : pixel_resources)
            put_resource(&database, resource.specifier,
                         std::to_string(to_device(resource.value)));
        const std::string device =
            device_font_name(display, control_font_name);
        if (!device.empty())
            put_resource(&database, "*font", device);
    }

    const std::string &logical_control_font() {
        return control_font_name;
    }

    const char *logical_default_font(Display *display) {
        if (!default_font_known)
            return XGetDefault(display, "*", "font");
        return default_font_name.empty() ? nullptr
                                         : default_font_name.c_str();
    }

    Font load_device_font(Display *display, const XFontStruct *logical) {
        if (!display || !logical || coordinate_scale() == 1.0)
            return 0;
        const std::string device =
            device_font_name(display, font_name(display, logical));
        if (device.empty())
            return 0;
        return XLoadFont(display, device.c_str());
    }

    Pixmap create_device_bitmap(Display *display,
                                Drawable drawable,
                                const char *bits,
                                unsigned int width,
                                unsigned int height) {
        if (!display || !bits || !width || !height)
            return None;
        const unsigned int device_width = static_cast<unsigned int>(
            std::max(1, to_device(static_cast<int>(width))));
        const unsigned int device_height = static_cast<unsigned int>(
            std::max(1, to_device(static_cast<int>(height))));
        const unsigned int source_stride = (width + 7) / 8;
        const unsigned int stride = (device_width + 7) / 8;
        std::vector<char> scaled(stride * device_height, 0);
        for (unsigned int y = 0; y < device_height; ++y) {
            const unsigned int source_y = y * height / device_height;
            for (unsigned int x = 0; x < device_width; ++x) {
                const unsigned int source_x = x * width / device_width;
                const auto byte = static_cast<unsigned char>(
                    bits[source_y * source_stride + source_x / 8]);
                if (byte & (1U << (source_x % 8)))
                    scaled[y * stride + x / 8] = static_cast<char>(
                        scaled[y * stride + x / 8] | (1 << (x % 8)));
            }
        }
        return XCreateBitmapFromData(display, drawable, scaled.data(),
                                     device_width, device_height);
    }
} // namespace linux::x11

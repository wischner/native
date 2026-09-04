//
// Implements standard WINGs alert panels.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstddef>
#include <stdexcept>
#include <string>

#include <WINGs/WINGs.h>
#include <wraster.h>

#include <native/app_wnd.h>
#include <native/message_box.h>

#include "globals.h"
#include "../../message_box_common.h"
#include "../../message_box_icons.h"

namespace
{
    WMPixmap *create_message_icon(native::message_box_icon icon) {
        if (icon == native::message_box_icon::none)
            return nullptr;

        const native::img &source =
            native::detail::message_box_icon_image(icon);
        RImage *raster = RCreateImage(source.w(), source.h(), true);
        if (!raster)
            return nullptr;

        const std::size_t count =
            static_cast<std::size_t>(source.w()) * source.h();
        for (std::size_t index = 0; index < count; ++index) {
            const native::rgba pixel = source.pixels()[index];
            raster->data[index * 4] = pixel.r;
            raster->data[index * 4 + 1] = pixel.g;
            raster->data[index * 4 + 2] = pixel.b;
            raster->data[index * 4 + 3] = pixel.a;
        }

        WMPixmap *pixmap = WMCreatePixmapFromRImage(
            linux::wmaker::screen, raster, 128);
        RReleaseImage(raster);
        return pixmap;
    }
} // namespace

namespace native
{
    message_box_result message_box::show(
        app_wnd &owner,
        const std::string &message,
        const std::string &title,
        message_box_buttons buttons,
        message_box_icon icon) {
        detail::validate_message_box_owner(owner);
        auto *owner_state = linux::wmaker::state(&owner);
        if (!owner_state || !owner_state->window)
            throw std::runtime_error(
                "Window Maker/WINGs: Message box has no owner.");

        const int count = detail::message_box_button_count(buttons);
        const char *first =
            detail::message_box_button_label(buttons, 0);
        const char *second = count > 1
            ? detail::message_box_button_label(buttons, 1) : nullptr;
        const char *third = count > 2
            ? detail::message_box_button_label(buttons, 2) : nullptr;
        WMAlertPanel *panel = WMCreateAlertPanel(
            linux::wmaker::screen, owner_state->window,
            title.c_str(), message.c_str(), first, second, third);
        if (!panel)
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create an alert panel.");

        if (WMPixmap *pixmap = create_message_icon(icon)) {
            WMSetLabelImage(panel->iLbl, pixmap);
            WMReleasePixmap(pixmap);
        }
        if (panel->tLbl) {
            WMFont *title_font = WMBoldSystemFontOfSize(
                linux::wmaker::screen, 12);
            if (title_font) {
                WMSetLabelFont(panel->tLbl, title_font);
                WMReleaseFont(title_font);
            }
        }

        WMView *owner_view = WMWidgetView(owner_state->window);
        WMView *panel_view = WMWidgetView(panel->win);
        const WMPoint origin = WMGetViewScreenPosition(owner_view);
        const WMSize owner_size = WMGetViewSize(owner_view);
        const WMSize panel_size = WMGetViewSize(panel_view);
        WMSetWindowInitialPosition(
            panel->win,
            origin.x + (static_cast<int>(owner_size.width) -
                        static_cast<int>(panel_size.width)) / 2,
            origin.y + (static_cast<int>(owner_size.height) -
                        static_cast<int>(panel_size.height)) / 2);
        WMMapWidget(panel->win);
        WMRunModalLoop(linux::wmaker::screen, panel_view);
        const int result = panel->result;
        WMDestroyAlertPanel(panel);
        const int index = result == WAPRDefault ? 0
                        : result == WAPRAlternate ? 1
                        : result == WAPROther ? 2 : -1;
        return index >= 0 && index < count
            ? detail::message_box_result_for_button(buttons, index)
            : detail::message_box_dismissed_result(buttons);
    }
} // namespace native

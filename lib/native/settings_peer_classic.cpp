//
// Composes a labeled native search editor beside the existing
// icon-capable navigation tree on the classic WINGs, Motif and GEM
// adapters. All outside spacing is shared; each toolkit retains its own
// text and scrollbar widgets.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "picker_peer.h"
#include "wnd_peer.h"
#include <native.h>
namespace
{
    struct search_resources final
        : native::detail::settings_sidebar_resources
    {
        native::settings_sidebar &owner;
        native::canvas row;
        native::text_edit edit;
        explicit search_resources(native::settings_sidebar &value)
            : owner(value) {
            row.set_parent(&owner);
            row.on_wnd_paint.connect(
                [this](native::wnd_paint_event event) {
                    auto appearance = native::theme::create(event.g);
                    event.g
                        .clear(appearance->native_palette().button_bg)
                        .set_font(native::font_t::stock(
                            native::font_role::control))
                        .set_ink(
                            appearance->native_palette().button_text)
                        .draw_text("Search:",
                                   native::rect(
                                       0,
                                       0,
                                       native::font_t::stock(
                                           native::font_role::control)
                                               .measure_text("Search:")
                                               .width +
                                           6,
                                       row.get_dimensions().h),
                                   {native::text_align::start,
                                    native::text_valign::center});
                    return true;
                });
            edit.set_parent(&row);
            edit.on_change.connect([this](const std::string &text) {
                owner.on_native_search(text);
                return true;
            });
        }
        void close() noexcept override {
            row.destroy();
        }
    };
} // namespace
namespace native::detail
{
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        std::unique_ptr<settings_sidebar_resources> resources =
            std::make_unique<search_resources>(owner);
        assign_peer_state(owner, resources.release());
        return false;
    }
    bool settings_sidebar_uses_native_search() {
        return true;
    }
    bool settings_sidebar_uses_list() {
        return false;
    }
    int settings_sidebar_navigation_padding() {
        return 6;
    }
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *resources = static_cast<search_resources *>(
            peer_state<settings_sidebar_resources>(owner));
        if (!resources)
            return;
        if (!owner.get_search_visible()) {
            resources->row.destroy();
            return;
        }
        const auto navigation = settings_navigation_bounds(owner);
        const auto &font = font_t::stock(font_role::control);
        const int prompt = font.measure_text("Search:").width + 6;
        const int height =
            theme::create(owner.get_gpx())->get_text_edit_height();
        resources->row.set_bounds(
            rect(navigation.x1(), 6, navigation.w(), height));
        resources->edit.set_bounds(
            rect(prompt,
                 0,
                 std::max(1, int(navigation.w()) - prompt),
                 height));
        resources->edit.set_text(owner.get_search_text());
        if (!resources->row.get_created())
            resources->row.create();
        if (!resources->edit.get_created())
            resources->edit.create();
        resources->row.show();
        resources->edit.show();
    }
} // namespace native::detail

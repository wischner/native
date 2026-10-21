//
// Integrates Granite 7's C interface and live elementary color
// preferences. GTK4 supplies the native controls and client-side window
// decoration.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../wayland/globals.h"
#include <granite-7.h>
#include <filesystem>
#include <cstdlib>
#include "../../message_box_common.h"
namespace
{
    void color_scheme(GraniteSettings *settings) {
        g_object_set(
            gtk_settings_get_default(),
            "gtk-application-prefer-dark-theme",
            granite_settings_get_prefers_color_scheme(settings) ==
                GRANITE_SETTINGS_COLOR_SCHEME_DARK,
            nullptr);
    }
} // namespace
namespace linux::wayland
{
    GtkWidget *new_entry() {
        auto *entry = granite_validated_entry_new();
        granite_validated_entry_set_is_valid(entry, true);
        return GTK_WIDGET(entry);
    }
    // Granite 7.4's supported message dialog derives from GtkDialog.
    // Keep compatibility with its C ABI while GTK-only uses
    // GtkAlertDialog.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    native::message_box_result
    show_message(native::app_wnd &owner,
                 const std::string &message,
                 const std::string &title,
                 native::message_box_buttons buttons,
                 native::message_box_icon icon) {
        using namespace native;
        const char *symbol =
            icon == message_box_icon::error      ? "dialog-error"
            : icon == message_box_icon::warning  ? "dialog-warning"
            : icon == message_box_icon::question ? "dialog-question"
                                                 : "dialog-information";
        auto *dialog = GTK_WINDOW(
            granite_message_dialog_new_with_image_from_icon_name(
                title.c_str(),
                message.c_str(),
                symbol,
                GTK_BUTTONS_NONE));
        gtk_window_set_transient_for(dialog,
                                     GTK_WINDOW(state(owner).widget));
        gtk_window_set_modal(dialog, true);
        const int count = detail::message_box_button_count(buttons);
        for (int i = 0; i < count; ++i)
            gtk_dialog_add_button(
                GTK_DIALOG(dialog),
                detail::message_box_button_label(buttons, i),
                i + 1);
        struct response_state
        {
            bool done = false;
            int response = -1;
        } response;
        g_signal_connect(
            dialog,
            "response",
            G_CALLBACK(+[](GtkDialog *, int id, gpointer data) {
                auto &response = *static_cast<response_state *>(data);
                response.response = id;
                response.done = true;
            }),
            &response);
        gtk_window_present(dialog);
        while (!response.done)
            g_main_context_iteration(nullptr, true);
        gtk_window_destroy(dialog);
        return response.response > 0
                   ? detail::message_box_result_for_button(
                         buttons, response.response - 1)
                   : detail::message_box_dismissed_result(buttons);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS
    void configure_desktop(GtkWindow *window) {
        static bool initialized = false;
        if (!initialized) {
            granite_init();
            std::error_code error;
            if (std::getenv("DBUS_SYSTEM_BUS_ADDRESS") ||
                std::filesystem::exists("/run/dbus/system_bus_socket",
                                        error)) {
                auto *settings = granite_settings_get_default();
                color_scheme(settings);
                g_signal_connect(
                    settings,
                    "notify::prefers-color-scheme",
                    G_CALLBACK(
                        +[](GObject *settings, GParamSpec *, gpointer) {
                            color_scheme(GRANITE_SETTINGS(settings));
                        }),
                    nullptr);
            }
            initialized = true;
        }
        gtk_widget_add_css_class(GTK_WIDGET(window),
                                 GRANITE_STYLE_CLASS_BACKGROUND);
    }
} // namespace linux::wayland

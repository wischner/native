//
// Generic Wayland GTK desktop integration without Granite.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../message_box_common.h"
namespace linux::wayland
{
    GtkWidget *new_entry() {
        return gtk_entry_new();
    }
    native::message_box_result
    show_message(native::app_wnd &owner,
                 const std::string &message,
                 const std::string &title,
                 native::message_box_buttons buttons,
                 native::message_box_icon) {
        using namespace native;
        auto *dialog = gtk_alert_dialog_new("%s", title.c_str());
        gtk_alert_dialog_set_detail(dialog, message.c_str());
        const int count = detail::message_box_button_count(buttons);
        const char *labels[4] = {};
        for (int i = 0; i < count; ++i)
            labels[i] = detail::message_box_button_label(buttons, i);
        gtk_alert_dialog_set_buttons(dialog, labels);
        struct response_state
        {
            bool done = false;
            int response = -1;
        } response;
        gtk_alert_dialog_choose(
            dialog,
            GTK_WINDOW(linux::wayland::state(owner).widget),
            nullptr,
            +[](GObject *dialog, GAsyncResult *result, gpointer data) {
                auto &response = *static_cast<response_state *>(data);
                GError *error = nullptr;
                response.response = gtk_alert_dialog_choose_finish(
                    GTK_ALERT_DIALOG(dialog), result, &error);
                if (error)
                    g_error_free(error);
                response.done = true;
            },
            &response);
        while (!response.done)
            g_main_context_iteration(nullptr, true);
        g_object_unref(dialog);
        return response.response >= 0
                   ? detail::message_box_result_for_button(
                         buttons, response.response)
                   : detail::message_box_dismissed_result(buttons);
    }
    void configure_desktop(GtkWindow *) {}
} // namespace linux::wayland

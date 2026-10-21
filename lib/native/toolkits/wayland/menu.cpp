//
// Builds native GTK popover menus from portable command identifiers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../menu_shortcut.h"
namespace
{
    // Translate the parsed portable mnemonic to GTK underscore syntax.
    std::string menu_label(const std::string &label, std::size_t index) {
        std::string result;
        for (std::size_t i = 0; i < label.size(); ++i) {
            if (i == index)
                result.push_back('_');
            if (label[i] == '_')
                result.push_back('_');
            result.push_back(label[i]);
        }
        return result;
    }
} // namespace
#include <cctype>
namespace native
{
    main_menu::~main_menu() {
        detach();
    }
    void main_menu::detach() {
        if (_owner) {
            auto *peer =
                detail::peer_state<linux::wayland::widget_state>(
                    *_owner);
            if (peer && peer->widget && peer->menu) {
                gtk_box_remove(GTK_BOX(peer->box), peer->menu);
                peer->menu = nullptr;
                if (peer->shortcuts && peer->widget) {
                    gtk_widget_remove_controller(peer->widget,
                                                 peer->shortcuts);
                    peer->shortcuts = nullptr;
                }
            }
        }
        _owner = nullptr;
        _id = 0;
    }
    void main_menu::attach(app_wnd &owner) {
        detach();
        _owner = &owner;
        auto &peer = linux::wayland::state(owner);
        if (tops().empty())
            return;
        peer.shortcuts = gtk_shortcut_controller_new();
        gtk_shortcut_controller_set_scope(
            GTK_SHORTCUT_CONTROLLER(peer.shortcuts),
            GTK_SHORTCUT_SCOPE_GLOBAL);
        gtk_widget_add_controller(peer.widget, peer.shortcuts);
        auto *model = g_menu_new();
        auto *actions = g_simple_action_group_new();
        for (const auto &top : tops()) {
            auto *submenu = g_menu_new();
            auto *section = g_menu_new();
            for (const auto &entry : top.items) {
                if (entry.separator) {
                    g_menu_append_section(
                        submenu, nullptr, G_MENU_MODEL(section));
                    g_object_unref(section);
                    section = g_menu_new();
                    continue;
                }
                const auto name = "command" + std::to_string(entry.id);
                if (!entry.shortcut.empty()) {
                    auto shortcut =
                        detail::parse_menu_shortcut(entry.shortcut);
                    std::string trigger;
                    if (shortcut.control)
                        trigger += "<Control>";
                    if (shortcut.alt)
                        trigger += "<Alt>";
                    if (shortcut.shift)
                        trigger += "<Shift>";
                    if (shortcut.command)
                        trigger += "<Super>";
                    if (shortcut.key.size() == 1)
                        shortcut.key[0] =
                            std::tolower(static_cast<unsigned char>(
                                shortcut.key[0]));
                    if (shortcut.key == "Enter")
                        shortcut.key = "Return";
                    if (shortcut.key == "Del")
                        shortcut.key = "Delete";
                    if (shortcut.key == "Space")
                        shortcut.key = "space";
                    trigger += shortcut.key;
                    if (auto *parsed =
                            gtk_shortcut_trigger_parse_string(
                                trigger.c_str()))
                        gtk_shortcut_controller_add_shortcut(
                            GTK_SHORTCUT_CONTROLLER(peer.shortcuts),
                            gtk_shortcut_new(
                                parsed,
                                gtk_named_action_new(
                                    ("native." + name).c_str())));
                }

                auto *action =
                    g_simple_action_new(name.c_str(), nullptr);
                g_object_set_data(G_OBJECT(action),
                                  "native-id",
                                  GINT_TO_POINTER(entry.id));
                g_signal_connect(
                    action,
                    "activate",
                    G_CALLBACK(+[](GSimpleAction *action,
                                   GVariant *,
                                   gpointer data) {
                        auto &owner = *static_cast<app_wnd *>(data);
                        if (owner.get_input_enabled())
                            owner.on_native_menu(
                                GPOINTER_TO_INT(g_object_get_data(
                                    G_OBJECT(action), "native-id")));
                    }),
                    &owner);
                g_action_map_add_action(G_ACTION_MAP(actions),
                                        G_ACTION(action));
                g_object_unref(action);
                auto label = menu_label(
                    entry.label, entry.mnemonic_index);
                g_menu_append(
                    section, label.c_str(), ("native." + name).c_str());
            }
            g_menu_append_section(
                submenu, nullptr, G_MENU_MODEL(section));
            g_object_unref(section);
            const auto title = menu_label(
                top.title, top.mnemonic_index);
            g_menu_append_submenu(
                model, title.c_str(), G_MENU_MODEL(submenu));
            g_object_unref(submenu);
        }
        gtk_widget_insert_action_group(
            peer.widget, "native", G_ACTION_GROUP(actions));
        g_object_unref(actions);
        peer.menu =
            gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(model));
        g_object_unref(model);
        gtk_box_prepend(GTK_BOX(peer.box), peer.menu);
        _id = 1;
    }
} // namespace native

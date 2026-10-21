//
// Exposes portable row positions as lazy GObjects for GTK virtual
// views.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "index_model.h"
namespace
{
    struct index_model_state
    {
        GObject parent;
        guint count;
    };
    GType item_type(GListModel *) {
        return G_TYPE_OBJECT;
    }
    guint count(GListModel *model) {
        return reinterpret_cast<index_model_state *>(model)->count;
    }
    gpointer item(GListModel *model, guint position) {
        if (position >= count(model))
            return nullptr;
        auto *object = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        g_object_set_data(
            object, "native-index", GUINT_TO_POINTER(position));
        return object;
    }
    // Register the small C-ABI adapter without imposing GObject's
    // generated C type spelling on Native's internal C++ class names.
    GType model_type() {
        static const GType type = [] {
            const auto value =
                g_type_register_static_simple(G_TYPE_OBJECT,
                                              "NativeIndexModel",
                                              sizeof(GObjectClass),
                                              nullptr,
                                              sizeof(index_model_state),
                                              nullptr,
                                              GTypeFlags(0));
            const GInterfaceInfo interface = {
                +[](gpointer value, gpointer) {
                    auto *interface =
                        static_cast<GListModelInterface *>(value);
                    interface->get_item_type = item_type;
                    interface->get_n_items = count;
                    interface->get_item = item;
                },
                nullptr,
                nullptr};
            g_type_add_interface_static(
                value, G_TYPE_LIST_MODEL, &interface);
            return value;
        }();
        return type;
    }
} // namespace
namespace linux::wayland
{
    GListModel *index_model(guint count) {
        auto *model = reinterpret_cast<index_model_state *>(
            g_object_new(model_type(), nullptr));
        model->count = count;
        return G_LIST_MODEL(model);
    }
    guint item_index(GObject *item) {
        return GPOINTER_TO_UINT(
            g_object_get_data(item, "native-index"));
    }
} // namespace linux::wayland

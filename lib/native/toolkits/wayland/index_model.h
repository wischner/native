//
// Declares a lazy GTK item-index model shared by native collection
// adapters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <gio/gio.h>
namespace linux::wayland
{
    // Create a lazy index model without materializing the portable
    // rows.
    GListModel *index_model(guint count);
    // Return the portable display index associated with one native
    // item.
    guint item_index(GObject *item);
} // namespace linux::wayland

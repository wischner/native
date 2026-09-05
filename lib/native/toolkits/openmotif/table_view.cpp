//
// Hosts materialized and virtual tables in the same Motif-themed viewport.
// XmContainer cannot implement virtual rows or the portable grid contract;
// native XmScrollBar peers retain Motif scrolling and input conventions.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <native.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void table_view::apply_table() { invalidate(); }
    void table_view::apply_selection() { invalidate(); }
    void table_view::apply_scroll() { invalidate(); }

    void table_view::create_native() {
        auto *state = new linux::openmotif::motif_collection();
        state->widget = linux::openmotif::create_collection_host(*this, *state);
        linux::openmotif::table_view_bindings.register_pair(this, state);
        synchronize_theme_metrics();
    }

    void table_view::show_native() {
        auto *state = linux::openmotif::table_view_bindings
            .object_from_handle(this);
        if (!_created || !state || !state->widget)
            throw std::runtime_error("Motif: table_view is not created.");
        XtManageChild(state->widget);
    }

    void table_view::destroy_native() {
        if (!_created)
            return;
        auto *state = linux::openmotif::table_view_bindings
            .object_from_handle(this);
        linux::openmotif::destroy_collection_host(*this, state);
        linux::openmotif::table_view_bindings.unregister_by_handle(this);
    }
} // namespace native

//
// Declares the internal custom-content property editor and popup host.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <native/canvas.h>
#include <native/property_grid.h>

namespace native::detail
{
    class property_drop_down_editor final : public canvas
    {
    public:
        // Keep a private item snapshot and the grid's validated commit route.
        property_drop_down_editor(property_item item, property_drop_down::commit commit);
        // Release the popup before its source row disappears.
        ~property_drop_down_editor() override;
        // Synchronize a silent programmatic value update.
        void set_value(property_value value);

    protected:
        // Destroy popup resources when scrolling removes this row.
        void destroy_native() override;

    private:
        class popup;
        property_item _item;
        property_drop_down::commit _commit;
        std::unique_ptr<popup> _popup;
        std::shared_ptr<int> _lifetime;
        std::shared_ptr<int> _session;

        // Paint the converted value and a compact dropdown arrow.
        void paint(wnd_paint_event event);
        // Open custom content under the value cell, or close an open popup.
        void open(point position);
    };
}

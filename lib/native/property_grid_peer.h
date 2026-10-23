//
// Recognizes private inspector editors inside their canvas value host so
// native adapters can retain property-field colors and border behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
namespace native
{
    class wnd;
    class property_grid;
    namespace detail
    {
        // Return the inspector through its nested canvas value hosts.
        property_grid *property_grid_owner(const wnd &editor);
    }
}

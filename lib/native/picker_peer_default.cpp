//
// Selects native choice composition when a toolkit has no date/time widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "picker_peer.h"
namespace native::detail
{
    bool create_date_picker_peer(date_picker &) { return false; }
    bool create_time_picker_peer(time_picker &) { return false; }
    void refresh_date_picker_peer(date_picker &) {}
    void refresh_time_picker_peer(time_picker &) {}
}

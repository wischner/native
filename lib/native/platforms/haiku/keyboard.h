//
// Declares private Haiku raw-position translation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <native/events.h>
#include <SupportDefs.h>
namespace haiku
{
    // Translate Haiku raw key positions, independently of the active keymap.
    native::key_code physical_key(int32 position);
}

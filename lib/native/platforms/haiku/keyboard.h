//
// Declares private Haiku raw-position translation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <native/events.h>
#include <SupportDefs.h>
#include <native/wnd.h>
class BMessage;
namespace haiku
{
    // Deliver one physical press/release from a BeAPI message.
    bool dispatch_key(native::wnd &owner, BMessage *message,
                      native::key_action action);

    // Translate Haiku raw key positions, independently of the active keymap.
    native::key_code physical_key(int32 position);
}

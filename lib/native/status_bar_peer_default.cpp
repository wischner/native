//
// Supplies the no-native-peer status-bar adapter factory.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "status_bar_peer.h"

namespace native::detail
{
    std::unique_ptr<status_bar_peer> create_status_bar_peer() {
        return nullptr;
    }
} // namespace native::detail

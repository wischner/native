//
// Retains themed toolbar painting where no native edge-button adapter exists.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "toolbar_peer.h"
namespace native::detail
{
    bool toolbar_peer_active(const toolbar &, const canvas &) { return false; }
    bool refresh_toolbar_peer(toolbar &, canvas &, const std::vector<rect> &) { return false; }
}

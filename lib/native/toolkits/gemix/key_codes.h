//
// Names the USB scan codes supplied by both hosted GEM input transports.
// These are internal adapters, not changes to public AES/VDI interfaces.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

namespace linux::gemix::key_scan
{
    constexpr int home = 74;
    constexpr int page_up = 75;
    constexpr int delete_forward = 76;
    constexpr int end = 77;
    constexpr int page_down = 78;
    constexpr int right = 79;
    constexpr int left = 80;
    constexpr int down = 81;
    constexpr int up = 82;
}

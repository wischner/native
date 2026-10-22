//
// Declares native control-edge clipping shared by AppKit view painting and
// layer masks. Native button margins are resolved from intrinsic geometry.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#import <AppKit/AppKit.h>
namespace native { class wnd; }
namespace mac
{
    // Preserve selected native edges without replacing the control painter.
    NSRect border_clip_bounds(native::wnd &owner, NSView *view);
}

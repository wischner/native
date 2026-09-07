//
// Supplies native hit-tested input and presented-pixel captures to the
// inspector runtime fixture. Backend types remain in the implementation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <native.h>

namespace inspector_input
{
    // Read the presented pixels, including native editor children.
    std::unique_ptr<native::img> capture(native::app_wnd &owner);
    // Install a UI heartbeat for the test worker's posted assertions.
    void start();
    // Move before pressing so native pointer grabs can settle.
    void move(native::app_wnd &owner, native::point point);
    // Hit-test the actual native hierarchy and send a press or release.
    void pointer(native::app_wnd &owner, native::point point, bool pressed);
    // Type a character through the focused native control.
    void type(native::app_wnd &owner, char character);
}

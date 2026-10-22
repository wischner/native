//
// Declares native GTK pixel-parity assertions for the Wayland fixture.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>
// Compare theme primitives with independently snapshotted live
// controls.
void test_native_gtk_theme(native::app_wnd &window);
// Verify removed native button edges through independently captured pixels.
void test_native_gtk_borders(native::app_wnd &window);

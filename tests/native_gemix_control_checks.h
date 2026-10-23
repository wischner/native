//
// Shares the real Rasta input driver's callbacks with GEM control
// checks. All portable/native state is inspected on the application
// thread.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native.h>
#include <functional>

struct gem_control_driver
{
    std::function<void(unsigned, native::point)> pointer;
    std::function<void(std::function<bool()>, const char *)> wait;
};

// Exercise fresh controls, modal paint and composed picker geometry.
void check_gemix_controls(native::app_wnd &owner,
                          const gem_control_driver &driver);

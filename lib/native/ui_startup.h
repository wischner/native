//
// Declares backend startup serialization around portable application
// creation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
namespace native::detail
{
    // Exclude backend dispatch until initial portable window setup completes.
    void begin_ui_startup();
    // Release startup exclusion, including after a failed app::run
    // transaction.
    void end_ui_startup();
} // namespace native::detail

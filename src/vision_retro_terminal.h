//
// Declares Vision's portable line-oriented CRT terminal acceptance
// mode. It uses public Native drawing, process, and scoped UI-delivery
// APIs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
namespace vision
{
    // Run the terminal; --terminal-smoke paints and closes
    // automatically.
    int run_retro_terminal(int argc, char **argv);
} // namespace vision

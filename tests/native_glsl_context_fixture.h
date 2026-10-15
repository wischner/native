//
// Declares deterministic EGL/GLX state for the Linux context regression tests.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

struct glsl_context_fixture {
    unsigned api;
    void *egl_context;
    void *egl_display;
    void *egl_draw;
    void *egl_read;
    void *glx_context;
    unsigned long glx_draw;
    unsigned long glx_read;
    bool fail_creation;
    bool fail_activation;
    int activations;
    int context_destructions;
    int display_terminations;
    bool display_initialized;
};

// Reset simulated driver state and optionally bind a toolkit context.
extern "C" glsl_context_fixture *reset_glsl_context_fixture(
    bool with_glx, bool with_egl);

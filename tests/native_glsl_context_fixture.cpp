//
// Simulates EGL/GLX context exclusion without a display server or GPU.
// Both loader names resolve to this library so driver state is shared.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "native_glsl_context_fixture.h"
#include <cstdint>
#include <cstring>

namespace {
    glsl_context_fixture state{};
    unsigned error = 0x3000;
    void *handle(std::uintptr_t value) {
        return reinterpret_cast<void *>(value);
    }
}

extern "C" glsl_context_fixture *reset_glsl_context_fixture(
    bool with_glx, bool with_egl) {
    state = {};
    state.api = 0x30a0;
    state.egl_context = with_egl ? handle(1) : nullptr;
    state.egl_display = with_egl ? handle(2) : nullptr;
    state.egl_draw = with_egl ? handle(3) : nullptr;
    state.egl_read = with_egl ? handle(4) : nullptr;
    state.glx_context = with_glx ? handle(5) : nullptr;
    state.glx_draw = with_glx ? 6 : 0;
    state.glx_read = with_glx ? 7 : 0;
    error = 0x3000;
    return &state;
}

extern "C" unsigned eglGetError() {
    const auto result = error;
    error = 0x3000;
    return result;
}
extern "C" unsigned eglQueryAPI() { return state.api; }
extern "C" unsigned eglReleaseThread() { return 1; }
extern "C" unsigned eglBindAPI(unsigned api) {
    state.api = api;
    return 1;
}
extern "C" void *eglGetCurrentContext() { return state.egl_context; }
extern "C" void *eglGetCurrentDisplay() { return state.egl_display; }
extern "C" void *eglGetCurrentSurface(int which) {
    return which == 0x3059 ? state.egl_draw : state.egl_read;
}
extern "C" void *fixture_platform(unsigned, void *, const int *) {
    return handle(8);
}
extern "C" void *eglGetProcAddress(const char *name) {
    return std::strcmp(name, "eglGetPlatformDisplayEXT") == 0
        ? reinterpret_cast<void *>(&fixture_platform) : nullptr;
}
extern "C" unsigned eglInitialize(void *, int *, int *) {
    state.display_initialized = true;
    return 1;
}
extern "C" const char *eglQueryString(void *, int) {
    return state.display_initialized ? "1.5" : nullptr;
}
extern "C" unsigned eglChooseConfig(
    void *, const int *, void **config, int, int *count) {
    *config = handle(9);
    *count = 1;
    return 1;
}
extern "C" void *eglCreatePbufferSurface(void *, void *, const int *) {
    return handle(10);
}
extern "C" void *eglCreateContext(
    void *, void *, void *, const int *) {
    if (state.fail_creation) {
        error = 0x3009;
        return nullptr;
    }
    return handle(11);
}
extern "C" unsigned eglMakeCurrent(
    void *display, void *draw, void *read, void *context) {
    if (context == handle(11) &&
        (!state.display_initialized || state.glx_context || state.fail_activation)) {
        error = 0x3002;
        return 0;
    }
    state.egl_context = context;
    state.egl_display = context ? display : nullptr;
    state.egl_draw = context ? draw : nullptr;
    state.egl_read = context ? read : nullptr;
    if (context == handle(11)) ++state.activations;
    return 1;
}
extern "C" unsigned eglDestroyContext(void *, void *) {
    ++state.context_destructions;
    return 1;
}
extern "C" unsigned eglDestroySurface(void *, void *) { return 1; }
extern "C" unsigned eglTerminate(void *) {
    ++state.display_terminations;
    state.display_initialized = false;
    return 1;
}
extern "C" void *glXGetCurrentContext() { return state.glx_context; }
extern "C" void *glXGetCurrentDisplay() {
    return state.glx_context ? handle(12) : nullptr;
}
extern "C" unsigned long glXGetCurrentDrawable() { return state.glx_draw; }
extern "C" unsigned long glXGetCurrentReadDrawable() {
    return state.glx_read;
}
extern "C" int glXMakeContextCurrent(
    void *, unsigned long draw, unsigned long read, void *context) {
    if (context && state.egl_context) return 0;
    state.glx_context = context;
    state.glx_draw = draw;
    state.glx_read = read;
    return 1;
}

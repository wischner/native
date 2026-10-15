//
// Verifies toolkit context preservation, repeated shader activation and
// rollback after creation/activation failures in the Linux EGL adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "native_glsl_context_fixture.h"
#include "glsl_context.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void check(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }
    void check_restored(const glsl_context_fixture &actual,
                        const glsl_context_fixture &before) {
        check(actual.api == before.api, "client API was changed");
        check(actual.egl_context == before.egl_context &&
              actual.egl_display == before.egl_display &&
              actual.egl_draw == before.egl_draw &&
              actual.egl_read == before.egl_read,
              "toolkit EGL context/surfaces were changed");
        check(actual.glx_context == before.glx_context &&
              actual.glx_draw == before.glx_draw &&
              actual.glx_read == before.glx_read,
              "toolkit GLX context/drawables were changed");
    }
    void round_trip(bool glx, bool egl) {
        auto *state = reset_glsl_context_fixture(glx, egl);
        const auto before = *state;
        {
            auto context = native::detail::make_glsl_context();
            check_restored(*state, before);
            for (int i = 0; i != 10; ++i) {
                context->enter();
                check(!state->glx_context, "GLX was not released");
                check(state->egl_context && state->api == 0x30a2,
                      "image OpenGL context was not activated");
                bool rejected = false;
                try { context->enter(); }
                catch (const std::logic_error &) { rejected = true; }
                check(rejected, "nested activation was not rejected");
                context->leave();
                context->leave();
                check_restored(*state, before);
            }
            // Destruction while entered also restores the borrowed context.
            context->enter();
        }
        check_restored(*state, before);
        check(state->activations == 11, "activation count mismatch");
        check(state->context_destructions == 1, "context resource leaked");
    }
    void activation_failure(bool glx, bool egl) {
        auto *state = reset_glsl_context_fixture(glx, egl);
        const auto before = *state;
        {
            auto context = native::detail::make_glsl_context();
            state->fail_activation = true;
            bool failed = false;
            try { context->enter(); }
            catch (const std::runtime_error &failure) {
                failed = std::string(failure.what()).find("0x3002")
                    != std::string::npos;
            }
            check(failed, "missing EGL activation diagnostic");
            check_restored(*state, before);
            state->fail_activation = false;
            context->enter();
            context->leave();
            check_restored(*state, before);
        }
        check_restored(*state, before);
    }
    void shared_display(bool borrowed) {
        auto *state = reset_glsl_context_fixture(false, false);
        state->display_initialized = borrowed;
        auto first = native::detail::make_glsl_context();
        auto second = native::detail::make_glsl_context();
        first.reset();
        check(state->display_terminations == 0, "live display was terminated");
        second->enter();
        second->leave();
        state->fail_creation = true;
        try { auto failed = native::detail::make_glsl_context(); }
        catch (const std::runtime_error &) {}
        state->fail_creation = false;
        second->enter();
        second->leave();
        second.reset();
        check(state->display_terminations == (borrowed ? 0 : 1),
              "display ownership was not respected");
    }
    void creation_failure() {
        auto *state = reset_glsl_context_fixture(true, false);
        const auto before = *state;
        state->fail_creation = true;
        bool failed = false;
        try { auto context = native::detail::make_glsl_context(); }
        catch (const std::runtime_error &failure) {
            failed = std::string(failure.what()).find("0x3009")
                != std::string::npos;
        }
        check(failed, "missing EGL creation diagnostic");
        check_restored(*state, before);
    }
}
int main() {
    try {
        round_trip(true, false);
        round_trip(false, true);
        round_trip(false, false);
        activation_failure(true, false);
        activation_failure(false, true);
        creation_failure();
        shared_display(false);
        shared_display(true);
        std::cout << "Linux EGL/GLX context preservation passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}

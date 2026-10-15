//
// Checks WGL/CGL context ownership using two real offscreen contexts.
// Unavailable desktop OpenGL is reported as a CTest skip.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "glsl_context.h"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#endif

namespace {
    auto current_context() {
#ifdef _WIN32
        return wglGetCurrentContext();
#else
        return CGLGetCurrentContext();
#endif
    }
    void check(bool condition) {
        if (!condition) throw std::runtime_error("Desktop GL context changed.");
    }
}

int main() {
    std::unique_ptr<native::detail::glsl_context> caller;
    try { caller = native::detail::make_glsl_context(); }
    catch (const std::exception &error) {
        std::cout << "SKIP: " << error.what() << '\n';
        return 77;
    }
    try {
        const auto original = current_context();
#ifdef _WIN32
        const auto original_dc = wglGetCurrentDC();
#endif
        caller->enter();
        const auto borrowed = current_context();
#ifdef _WIN32
        const auto borrowed_dc = wglGetCurrentDC();
#endif
        check(borrowed != nullptr);
        {
            auto image = native::detail::make_glsl_context();
            check(current_context() == borrowed);
            for (int i = 0; i < 10; ++i) {
                image->enter();
                check(current_context() != borrowed);
                bool rejected = false;
                try { image->enter(); }
                catch (const std::logic_error &) { rejected = true; }
                check(rejected);
                image->leave();
                image->leave();
                check(current_context() == borrowed);
#ifdef _WIN32
                check(wglGetCurrentDC() == borrowed_dc);
#endif
            }
            image->enter(); // Destruction must restore the borrowed context.
        }
        check(current_context() == borrowed);
        caller->leave();
        check(current_context() == original);
#ifdef _WIN32
        check(wglGetCurrentDC() == original_dc);
#endif
        caller.reset();
        check(current_context() == original);
        std::cout << "Desktop GL context preservation passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

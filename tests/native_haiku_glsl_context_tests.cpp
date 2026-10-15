//
// Exercises real Haiku OSMesa context isolation, nesting and retirement.
// Foreign Mesa bindings must be rejected without changing their state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "glsl_context.h"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
    void check(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    // Keep the entry-point provider alive until every borrowed function and
    // foreign context has finished, independently of tested peer teardown.
    void exercise(native::detail::glsl_context &provider) {
        const auto current = reinterpret_cast<void *(*)()>(
            provider.proc("_glapi_get_context"));
        const auto viewport = reinterpret_cast<void (*)(int, int, int, int)>(
            provider.proc("glViewport"));
        const auto integer = reinterpret_cast<void (*)(unsigned, int *)>(
            provider.proc("glGetIntegerv"));
        const auto create = reinterpret_cast<void *(*)(const int *, void *)>(
            provider.proc("OSMesaCreateContextAttribs"));
        const auto destroy = reinterpret_cast<void (*)(void *)>(
            provider.proc("OSMesaDestroyContext"));
        const auto bind = reinterpret_cast<unsigned char (*)(void *, void *, unsigned, int, int)>(
            provider.proc("OSMesaMakeCurrent"));
        check(current && viewport && integer && create && destroy && bind,
              "missing Haiku context test entry points");
        check(!current(), "construction left an image context current");
        auto first = native::detail::make_glsl_context();
        first->leave();
        first->enter();
        void *outer = current();
        viewport(2, 3, 4, 5);
        bool nested = false;
        try { first->enter(); } catch (const std::logic_error &) { nested = true; }
        check(nested && current() == outer, "re-entry changed the current context");
        auto second = native::detail::make_glsl_context();
        check(current() == outer, "candidate construction changed its caller");
        second->enter();
        void *inner = current();
        check(inner && inner != outer, "peers share a rendering context");
        bool order = false;
        try { first->leave(); } catch (const std::logic_error &) { order = true; }
        check(order && current() == inner, "out-of-order leave changed its caller");
        second->leave();
        std::array<int, 4> actual{};
        integer(0x0ba2, actual.data());
        check(current() == outer && actual == std::array<int, 4>{2, 3, 4, 5},
              "inner leave did not restore the outer context and viewport");
        second->enter();
        second.reset();
        check(current() == outer, "entered destruction did not restore its caller");
        first->leave();
        check(!current(), "leave did not clear the image binding");
        first->enter();
        second = native::detail::make_glsl_context();
        second->enter();
        inner = current();
        first.reset();
        check(current() == inner, "retiring an outer peer changed the inner binding");
        second->leave();
        check(!current(), "leave restored a retired outer peer");
        second->enter();
        second->leave();
        second->leave();

        const int attributes[] = {0x22, 0x1908, 0x33, 0x34, 0x36, 3, 0x37, 3, 0};
        void *foreign = create(attributes, nullptr);
        check(foreign, "cannot create a foreign OSMesa context");
        std::array<unsigned char, 24> pixels{};
        try {
            check(bind(foreign, pixels.data(), 0x1401, 3, 2), "cannot bind foreign context");
            void *foreign_dispatch = current();
            bool rejected = false;
            try { second->enter(); } catch (const std::runtime_error &) { rejected = true; }
            check(rejected && current() == foreign_dispatch,
                  "image activation changed a foreign Mesa binding");
            rejected = false;
            try { auto candidate = native::detail::make_glsl_context(); }
            catch (const std::runtime_error &) { rejected = true; }
            check(rejected && current() == foreign_dispatch,
                  "candidate construction changed a foreign Mesa binding");
            check(bind(nullptr, nullptr, 0x1401, 0, 0), "cannot clear foreign binding");
            destroy(foreign);
        } catch (...) {
            bind(nullptr, nullptr, 0x1401, 0, 0);
            destroy(foreign);
            throw;
        }
        second->enter();
        second->leave();
        check(!current(), "rendering did not recover after foreign release");
    }
} // namespace
int main() {
    std::unique_ptr<native::detail::glsl_context> provider;
    try {
        provider = native::detail::make_glsl_context();
    } catch (const std::exception &error) {
        std::cout << "SKIP: " << error.what() << '\n';
        return 77;
    }
    try {
        exercise(*provider);
        provider->enter();
        const auto text = reinterpret_cast<const unsigned char *(*)(unsigned)>(
            provider->proc("glGetString"));
        check(text, "missing renderer description entry point");
        std::cout << "Haiku OpenGL: " << text(0x1f02)
                  << "; renderer: " << text(0x1f01) << '\n';
        provider->leave();
        std::cout << "Haiku OSMesa context isolation and restoration passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

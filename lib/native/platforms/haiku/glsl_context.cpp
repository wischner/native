//
// Owns Haiku's optional Mesa software context for offscreen GLSL images.
// Native context nesting preserves binding order and resource ownership;
// unrelated Mesa clients are rejected before their dispatch is changed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../glsl_context.h"

#include <array>
#include <dlfcn.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
    using context_handle = void *;
    using gl_function = void (*)();
    constexpr unsigned unsigned_byte = 0x1401;

    // Resolve the stable OSMesa C ABI without a build-time Mesa dependency.
    // The dispatch getter identifies foreign clients without interpreting
    // their private context data as an OSMesa context.
    class osmesa_api final
    {
    public:
        osmesa_api() {
            _library = dlopen("libOSMesa.so.8", RTLD_NOW | RTLD_LOCAL);
            if (!_library)
                _library = dlopen("libOSMesa.so", RTLD_NOW | RTLD_LOCAL);
            if (!_library)
                throw std::runtime_error(
                    "Haiku GLSL requires the Mesa OSMesa runtime.");
            try {
                load(create, "OSMesaCreateContextAttribs");
                load(destroy, "OSMesaDestroyContext");
                load(make_current, "OSMesaMakeCurrent");
                load(get_proc, "OSMesaGetProcAddress");
                load(current_dispatch_context, "_glapi_get_context");
            } catch (...) {
                dlclose(_library);
                throw;
            }
        }
        ~osmesa_api() {
            dlclose(_library);
        }
        osmesa_api(const osmesa_api &) = delete;
        osmesa_api &operator=(const osmesa_api &) = delete;

        void *proc(const char *name) const {
            if (void *symbol = dlsym(_library, name))
                return symbol;
            return reinterpret_cast<void *>(get_proc(name));
        }

        context_handle (*create)(const int *, context_handle) = nullptr;
        void (*destroy)(context_handle) = nullptr;
        unsigned char (*make_current)(context_handle, void *, unsigned,
                                      int, int) = nullptr;
        gl_function (*get_proc)(const char *) = nullptr;
        void *(*current_dispatch_context)() = nullptr;

    private:
        template <typename function_type>
        void load(function_type &function, const char *name) {
            function = reinterpret_cast<function_type>(
                dlsym(_library, name));
            if (!function)
                throw std::runtime_error(
                    std::string("Haiku Mesa entry point missing: ") + name);
        }
        void *_library = nullptr;
    };

    // Mesa/LLVM dispatch and worker metadata outlive individual contexts.
    // Keep one lazily loaded driver module until process shutdown, while
    // each peer still destroys its actual OpenGL resources and context.
    std::shared_ptr<osmesa_api> driver() {
        static const auto api = std::make_shared<osmesa_api>();
        return api;
    }

    // An inner activation owns its predecessor until restoration completes.
    // Retirement skips a predecessor whose C++ adapter was destroyed while
    // another peer was current, without borrowing the destroyed adapter.
    struct context_state
    {
        std::shared_ptr<osmesa_api> api = driver();
        context_handle context = nullptr;
        void *dispatch_context = nullptr;
        std::array<unsigned char, 4> pixel{};
        std::shared_ptr<context_state> previous;
        bool retired = false;

        ~context_state() {
            if (!context)
                return;
            if (dispatch_context &&
                api->current_dispatch_context() == dispatch_context)
                api->make_current(nullptr, nullptr, unsigned_byte, 0, 0);
            api->destroy(context);
        }
    };
    thread_local std::weak_ptr<context_state> active_context;

    class osmesa_context final : public native::detail::glsl_context
    {
    public:
        osmesa_context() : _state(std::make_shared<context_state>()) {
            // OSMESA_FORMAT, PROFILE/CORE_PROFILE, MAJOR/MINOR_VERSION.
            const int attributes[] = {
                0x22, 0x1908, 0x33, 0x34, 0x36, 3, 0x37, 3, 0};
            _state->context = _state->api->create(attributes, nullptr);
            if (!_state->context)
                throw std::runtime_error(
                    "Haiku Mesa cannot create an OpenGL 3.3 core context.");
            enter();
            try {
                const auto get_integer = reinterpret_cast<void (*)(unsigned, int *)>(
                    proc("glGetIntegerv"));
                if (!get_integer)
                    throw std::runtime_error("Haiku Mesa cannot query OpenGL.");
                const auto get_string =
                    reinterpret_cast<const unsigned char *(*)(unsigned)>(
                        proc("glGetString"));
                if (!get_string)
                    throw std::runtime_error("Haiku Mesa cannot query its renderer.");
                const auto version_bytes = get_string(0x1f02);
                const auto renderer_bytes = get_string(0x1f01);
                const std::string version = version_bytes
                    ? reinterpret_cast<const char *>(version_bytes) : "";
                const std::string renderer = renderer_bytes
                    ? reinterpret_cast<const char *>(renderer_bytes) : "";
                // The packaged Mesa 22.0.5 llvmpipe faults in its vertex
                // JIT on Haiku. Reject before drawing; the same package's
                // softpipe passes the real shader regressions. Driver
                // selection belongs to the launching process, not setenv
                // inside a library while other clients may initialize GL.
                if (version.find("Mesa 22.0.5") != std::string::npos &&
                    renderer.find("llvmpipe") != std::string::npos)
                    throw std::runtime_error(
                        "Haiku Mesa 22.0.5 llvmpipe is unsupported; "
                        "launch with GALLIUM_DRIVER=softpipe.");
                int major = 0, minor = 0;
                get_integer(0x821b, &major);
                get_integer(0x821c, &minor);
                if (major < 3 || (major == 3 && minor < 3))
                    throw std::runtime_error(
                        "Haiku GLSL requires OpenGL 3.3 or newer.");
                leave();
            } catch (...) {
                try { leave(); } catch (...) {}
                throw;
            }
        }
        ~osmesa_context() override {
            _state->retired = true;
            if (_entered && _state->api->current_dispatch_context() ==
                                _state->dispatch_context) {
                try { leave(); } catch (...) {}
            }
        }
        void enter() override {
            if (_entered)
                throw std::logic_error("Nested OpenGL image context activation.");
            auto previous = active_context.lock();
            void *current = _state->api->current_dispatch_context();
            if (current && (!previous || current != previous->dispatch_context))
                throw std::runtime_error(
                    "Haiku GLSL requires releasing the caller's OpenGL context.");
            if (!current)
                previous.reset();
            if (!_state->api->make_current(_state->context,
                    _state->pixel.data(), unsigned_byte, 1, 1)) {
                if (!restore(previous))
                    throw std::runtime_error(
                        "Cannot restore Haiku Mesa after image activation failed.");
                throw std::runtime_error("Cannot activate Haiku Mesa image context.");
            }
            _state->dispatch_context = _state->api->current_dispatch_context();
            _state->previous = std::move(previous);
            active_context = _state;
            _entered = true;
        }
        void leave() override {
            if (!_entered)
                return;
            if (_state->api->current_dispatch_context() != _state->dispatch_context)
                throw std::logic_error("Haiku image contexts must leave in binding order.");
            auto previous = _state->previous;
            while (previous && previous->retired)
                previous = previous->previous;
            if (!restore(previous))
                throw std::runtime_error("Cannot restore the Haiku Mesa context.");
            active_context = previous;
            _state->previous.reset();
            _entered = false;
        }
        void *proc(const char *name) override {
            return _state->api->proc(name);
        }

    private:
        bool restore(const std::shared_ptr<context_state> &previous) {
            return previous
                ? previous->api->make_current(previous->context,
                    previous->pixel.data(), unsigned_byte, 1, 1)
                : _state->api->make_current(nullptr, nullptr, unsigned_byte, 0, 0);
        }
        std::shared_ptr<context_state> _state;
        bool _entered = false;
    };
} // namespace

namespace native::detail
{
    std::unique_ptr<glsl_context> make_glsl_context() {
        return std::make_unique<osmesa_context>();
    }
} // namespace native::detail

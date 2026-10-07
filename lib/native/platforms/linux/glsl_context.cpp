//
// Owns an offscreen EGL/OpenGL context for original GLSL image
// programs. Uses a pbuffer and standard EGL, independent of the
// selected UI toolkit.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../glsl_context.h"
#include <dlfcn.h>
#include <stdexcept>

namespace
{
    class egl_context final : public native::detail::glsl_context
    {
      public:
        egl_context() {
            library = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
            if (!library)
                throw std::runtime_error(
                    "GLSL rendering requires libEGL.so.1.");
            try {
                load(get_proc, "eglGetProcAddress");
                load(initialize, "eglInitialize");
                load(bind_api, "eglBindAPI");
                load(choose, "eglChooseConfig");
                load(create_surface, "eglCreatePbufferSurface");
                load(create_context, "eglCreateContext");
                load(make_current, "eglMakeCurrent");
                load(destroy_context, "eglDestroyContext");
                load(destroy_surface, "eglDestroySurface");
                load(terminate, "eglTerminate");
                auto platform = reinterpret_cast<void *(*)(
                    unsigned, void *, const int *)>(
                    get_proc("eglGetPlatformDisplayEXT"));
                if (!platform)
                    throw std::runtime_error(
                        "EGL surfaceless platform unavailable.");
                display = platform(0x31dd, nullptr, nullptr);
                if (!display ||
                    !initialize(display, nullptr, nullptr) ||
                    !bind_api(0x30a2))
                    fail();
                const int config_attributes[] = {
                    0x3033, 1, 0x3040, 8, 0x3024, 8, 0x3023, 8,
                    0x3022, 8, 0x3021, 8, 0x3038};
                void *config = nullptr;
                int count = 0;
                if (!choose(display, config_attributes, &config, 1,
                            &count) ||
                    !count)
                    fail();
                const int surface_attributes[] = {0x3057, 1, 0x3056, 1,
                                                  0x3038};
                surface =
                    create_surface(display, config, surface_attributes);
                const int context_attributes[] = {0x3098, 3, 0x30fb, 3,
                                                  0x30fd, 1, 0x3038};
                context = create_context(display, config, nullptr,
                                         context_attributes);
                if (!surface || !context)
                    fail();
            } catch (...) {
                cleanup();
                throw;
            }
        }
        ~egl_context() override {
            cleanup();
        }
        void enter() override {
            if (!make_current(display, surface, surface, context))
                fail();
        }
        void leave() override {
            make_current(display, nullptr, nullptr, nullptr);
        }
        void *proc(const char *name) override {
            return get_proc(name);
        }

      private:
        template <class type>
        void load(type &function, const char *name) {
            function = reinterpret_cast<type>(dlsym(library, name));
            if (!function)
                throw std::runtime_error("Missing EGL entry point.");
        }
        [[noreturn]] void fail() {
            throw std::runtime_error("Cannot create or activate the "
                                     "OpenGL 3.3 image context.");
        }
        void cleanup() noexcept {
            if (display && make_current)
                make_current(display, nullptr, nullptr, nullptr);
            if (context && destroy_context)
                destroy_context(display, context);
            if (surface && destroy_surface)
                destroy_surface(display, surface);
            if (display && terminate)
                terminate(display);
            if (library)
                dlclose(library);
        }
        void *library = nullptr, *display = nullptr, *surface = nullptr,
             *context = nullptr;
        void *(*get_proc)(const char *) = nullptr;
        unsigned (*initialize)(void *, int *, int *) = nullptr;
        unsigned (*bind_api)(unsigned) = nullptr;
        unsigned (*choose)(void *, const int *, void **, int,
                           int *) = nullptr;
        void *(*create_surface)(void *, void *, const int *) = nullptr;
        void *(*create_context)(void *, void *, void *,
                                const int *) = nullptr;
        unsigned (*make_current)(void *, void *, void *,
                                 void *) = nullptr;
        unsigned (*destroy_context)(void *, void *) = nullptr;
        unsigned (*destroy_surface)(void *, void *) = nullptr;
        unsigned (*terminate)(void *) = nullptr;
    };
} // namespace
namespace native::detail
{
    std::unique_ptr<glsl_context> make_glsl_context() {
        return std::make_unique<egl_context>();
    }
} // namespace native::detail

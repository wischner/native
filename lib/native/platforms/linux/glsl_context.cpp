//
// Owns an offscreen EGL/OpenGL context and preserves the calling toolkit's
// current EGL or GLX context across image rendering.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../glsl_context.h"
#include <dlfcn.h>
#include <cstdio>
#include <stdexcept>
#include <map>
#include <mutex>

namespace
{
    struct display_lease
    {
        unsigned users = 0;
        bool owned = false;
    };
    std::mutex display_mutex;
    std::map<void *, display_lease> displays;

    // EGL/GLX vendors retain process-global dispatch/TLS metadata. Keep
    // loader code resident so later views reuse that metadata and live
    // toolkit callbacks never point into an unloaded vendor module. Each
    // peer still releases its contexts/surfaces and its display lease.
    constexpr int loader_flags = RTLD_NOW | RTLD_LOCAL
#ifdef RTLD_NODELETE
        | RTLD_NODELETE
#endif
        ;

    class egl_context final : public native::detail::glsl_context
    {
      public:
        egl_context() {
            library = dlopen("libEGL.so.1", loader_flags);
            if (!library)
                throw std::runtime_error("GLSL rendering requires libEGL.so.1.");
            try {
                load(get_proc, "eglGetProcAddress");
                load(get_error, "eglGetError");
                load(query_api, "eglQueryAPI");
                load(release_thread, "eglReleaseThread");
                load(get_current_context, "eglGetCurrentContext");
                load(get_current_display, "eglGetCurrentDisplay");
                load(get_current_surface, "eglGetCurrentSurface");
                load(initialize, "eglInitialize");
                load(query_string, "eglQueryString");
                load(bind_api, "eglBindAPI");
                load(choose, "eglChooseConfig");
                load(create_surface, "eglCreatePbufferSurface");
                load(create_context, "eglCreateContext");
                load(make_current, "eglMakeCurrent");
                load(destroy_context, "eglDestroyContext");
                load(destroy_surface, "eglDestroySurface");
                load(terminate, "eglTerminate");
                // GLX is optional: Wayland and headless clients need only EGL.
                glx_library = dlopen("libGLX.so.0", loader_flags);
                if (glx_library) {
                    glx_get_context =
                        reinterpret_cast<void *(*)()>(
                            dlsym(glx_library, "glXGetCurrentContext"));
                    glx_get_display =
                        reinterpret_cast<void *(*)()>(
                            dlsym(glx_library, "glXGetCurrentDisplay"));
                    glx_get_drawable =
                        reinterpret_cast<unsigned long (*)()>(
                            dlsym(glx_library, "glXGetCurrentDrawable"));
                    glx_get_read =
                        reinterpret_cast<unsigned long (*)()>(
                            dlsym(glx_library, "glXGetCurrentReadDrawable"));
                    glx_make_current =
                        reinterpret_cast<int (*)(void *, unsigned long, unsigned long, void *)>(
                            dlsym(glx_library, "glXMakeContextCurrent"));
                }
                auto platform = reinterpret_cast<void *(*)(unsigned, void *, const int *)>(
                        get_proc("eglGetPlatformDisplayEXT"));
                if (!platform)
                    throw std::runtime_error("EGL surfaceless platform unavailable.");
                display = platform(0x31dd, nullptr, nullptr);
                if (!display) fail("get surfaceless display");
                acquire_display();
                const unsigned prior_api = query_api();
                if (!bind_api(0x30a2)) fail("bind OpenGL API");
                try {
                    const int config_attributes[] = {
                        0x3033, 1, 0x3040, 8, 0x3024, 8, 0x3023, 8,
                        0x3022, 8, 0x3021, 8, 0x3038};
                    void *config = nullptr;
                    int count = 0;
                    if (!choose(display, config_attributes, &config, 1, &count) || !count)
                        fail("choose OpenGL config");
                    const int surface_attributes[] = {0x3057, 1, 0x3056, 1, 0x3038};
                    surface = create_surface(display, config, surface_attributes);
                    if (!surface) fail("create pbuffer");
                    const int context_attributes[] = {
                        0x3098, 3, 0x30fb, 3, 0x30fd, 1, 0x3038};
                    context = create_context(display, config, nullptr, context_attributes);
                    if (!context) fail("create OpenGL 3.3 context");
                } catch (...) {
                    bind_api(prior_api);
                    throw;
                }
                if (!bind_api(prior_api)) fail("restore client API");
            } catch (...) {
                cleanup();
                throw;
            }
        }
        ~egl_context() override { cleanup(); }
        void enter() override {
            if (entered)
                throw std::logic_error("Nested OpenGL image context activation.");
            saved_api = query_api();
            saved_context = get_current_context();
            saved_display = get_current_display();
            saved_draw = get_current_surface(0x3059);
            saved_read = get_current_surface(0x305a);
            saved_glx_context = glx_get_context ? glx_get_context() : nullptr;
            saved_glx_display = nullptr;
            if (saved_glx_context) {
                if (!glx_get_display || !glx_get_drawable || !glx_get_read || !glx_make_current)
                    throw std::runtime_error("Cannot preserve the toolkit's GLX context.");
                saved_glx_display = glx_get_display();
                saved_glx_draw = glx_get_drawable();
                saved_glx_read = glx_get_read();
            }
            // EGL and GLX cannot both own the thread's current OpenGL context.
            if (saved_context && !make_current(saved_display, nullptr, nullptr, nullptr))
                fail("release toolkit EGL context");
            if (saved_glx_context && !glx_make_current(saved_glx_display, 0, 0, nullptr)) {
                restore();
                throw std::runtime_error("Cannot release the toolkit's GLX context.");
            }
            if (!bind_api(0x30a2) || !make_current(display, surface, surface, context)) {
                const unsigned error = get_error();
                restore();
                fail("activate OpenGL image context", error);
            }
            entered = true;
        }
        void leave() override {
            if (!entered) return;
            entered = false;
            const bool released = make_current(display, nullptr, nullptr, nullptr);
            const unsigned error = released ? 0 : get_error();
            const bool restored = restore();
            if (!released) fail("release OpenGL image context", error);
            if (!restored)
                throw std::runtime_error("Cannot restore the toolkit's graphics context.");
        }
        void *proc(const char *name) override { return get_proc(name); }

      private:
        template <class type>
        void load(type &function, const char *name) {
            function = reinterpret_cast<type>(dlsym(library, name));
            if (!function)
                throw std::runtime_error("Missing EGL entry point.");
        }
        [[noreturn]] void fail(const char *step, unsigned error = 0) {
            if (!error) error = get_error();
            char message[256];
            std::snprintf(message, sizeof message,
                "OpenGL image context: %s failed (EGL 0x%04x).", step, error);
            throw std::runtime_error(message);
        }
        bool restore() noexcept {
            bool ok = bind_api(saved_api);
            if (saved_context)
                ok = make_current(saved_display, saved_draw, saved_read, saved_context) && ok;
            if (saved_glx_context)
                ok = glx_make_current(saved_glx_display, saved_glx_draw, saved_glx_read, saved_glx_context) && ok;
            return ok;
        }
        // EGL initialization is not reference counted by the driver. Share
        // Native's lease and leave an already initialized external display alone.
        void acquire_display() {
            const std::lock_guard<std::mutex> lock(display_mutex);
            auto [entry, inserted] = displays.try_emplace(display);
            if (inserted) {
                entry->second.owned = query_string(display, 0x3054) == nullptr;
                if (entry->second.owned && !initialize(display, nullptr, nullptr)) {
                    displays.erase(entry);
                    fail("initialize display");
                }
            }
            ++entry->second.users;
            display_acquired = true;
        }
        void release_display() noexcept {
            if (!display_acquired) return;
            const std::lock_guard<std::mutex> lock(display_mutex);
            const auto entry = displays.find(display);
            if (--entry->second.users == 0) {
                if (entry->second.owned) terminate(display);
                displays.erase(entry);
            }
            display_acquired = false;
        }
        void cleanup() noexcept {
            if (entered) {
                try { leave(); } catch (...) {}
            }
            // Never unbind an unrelated toolkit context during destruction.
            if (context && destroy_context) destroy_context(display, context);
            if (surface && destroy_surface) destroy_surface(display, surface);
            // Release driver TLS only when no caller EGL context/API state
            // would be disturbed. This also permits unloading a used driver
            // without retaining its per-thread allocation records.
            if (release_thread && get_current_context && query_api &&
                !get_current_context() && query_api() == 0x30a0)
                release_thread();
            release_display();
            if (glx_library) dlclose(glx_library);
            if (library) dlclose(library);
        }
        void *library = nullptr, *glx_library = nullptr;
        void *display = nullptr, *surface = nullptr, *context = nullptr;
        bool entered = false;
        bool display_acquired = false;
        unsigned saved_api = 0;
        void *saved_context = nullptr, *saved_display = nullptr;
        void *saved_draw = nullptr, *saved_read = nullptr;
        void *saved_glx_context = nullptr, *saved_glx_display = nullptr;
        unsigned long saved_glx_draw = 0, saved_glx_read = 0;
        void *(*glx_get_context)() = nullptr;
        void *(*glx_get_display)() = nullptr;
        unsigned long (*glx_get_drawable)() = nullptr;
        unsigned long (*glx_get_read)() = nullptr;
        int (*glx_make_current)(
            void *, unsigned long, unsigned long, void *) = nullptr;
        void *(*get_proc)(const char *) = nullptr;
        unsigned (*get_error)() = nullptr;
        unsigned (*query_api)() = nullptr;
        unsigned (*release_thread)() = nullptr;
        void *(*get_current_context)() = nullptr;
        void *(*get_current_display)() = nullptr;
        void *(*get_current_surface)(int) = nullptr;
        unsigned (*initialize)(void *, int *, int *) = nullptr;
        const char *(*query_string)(void *, int) = nullptr;
        unsigned (*bind_api)(unsigned) = nullptr;
        unsigned (*choose)(void *, const int *, void **, int, int *) = nullptr;
        void *(*create_surface)(void *, void *, const int *) = nullptr;
        void *(*create_context)(
            void *, void *, void *, const int *) = nullptr;
        unsigned (*make_current)(void *, void *, void *, void *) = nullptr;
        unsigned (*destroy_context)(void *, void *) = nullptr;
        unsigned (*destroy_surface)(void *, void *) = nullptr;
        unsigned (*terminate)(void *) = nullptr;
    };
}
namespace native::detail
{
    std::unique_ptr<glsl_context> make_glsl_context() {
        return std::make_unique<egl_context>();
    }
}

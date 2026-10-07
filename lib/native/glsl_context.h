//
// Declares the private offscreen OpenGL context owned by a shader peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <memory>
namespace native::detail
{
    class glsl_context
    {
      public:
        virtual ~glsl_context() = default;
        virtual void enter() = 0;
        virtual void leave() = 0;
        virtual void *proc(const char *name) = 0;
    };
    // Create an offscreen context without changing any public window
    // handle.
    std::unique_ptr<glsl_context> make_glsl_context();
} // namespace native::detail

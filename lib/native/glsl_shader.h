//
// Declares private GLSL resources and the portable rendering boundary.
// Native graphics types remain isolated from the public C++ interface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "shader_program.h"
#include <istream>

namespace native::detail
{
    class glsl_renderer
    {
      public:
        virtual ~glsl_renderer() = default;
        virtual std::unique_ptr<img>
        render(const img &source,
               const std::map<std::string, shader_value, std::less<>>
                   &parameters,
               unsigned width, unsigned height, double source_time,
               double elapsed, double delta, bool reset) = 0;
        virtual bool accelerated() const = 0;
    };
    // Decode owned shader text, uniform bindings and encoded image
    // textures.
    std::shared_ptr<shader_package_data>
    decode_glsl_package(std::istream &input);
    // Compile programs in a private context. Fail explicitly if
    // unavailable.
    std::unique_ptr<glsl_renderer>
    make_glsl_renderer(const shader_package_data &package);
} // namespace native::detail

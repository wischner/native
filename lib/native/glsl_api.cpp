//
// Loads private OpenGL entry points from the selected platform context.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "glsl_api.h"
namespace native::detail
{
    glsl_api::glsl_api(glsl_context &context) {
#define LOAD(R, N, SDK, A)                                                   \
    N = reinterpret_cast<decltype(N)>(context.proc("gl" #SDK));              \
    if (!N)                                                                  \
        throw std::runtime_error("OpenGL entry point unavailable: gl" #SDK);
        NATIVE_GL_FUNCTIONS(LOAD)
#undef LOAD
    }
} // namespace native::detail

//
// Defines private OpenGL entry points without platform SDK header leakage.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include "glsl_context.h"
#include <cstddef>
#include <stdexcept>
#ifdef _WIN32
#define NATIVE_GL_CALL __stdcall
#else
#define NATIVE_GL_CALL
#endif
namespace native::detail
{
    using gl_uint = unsigned;
    using gl_enum = unsigned;
    using gl_int = int;
    using gl_size = std::ptrdiff_t;
    using gl_bool = unsigned char;
#define NATIVE_GL_FUNCTIONS(F)                                               \
    F(void, gen_textures, GenTextures, (int, unsigned *))                    \
    F(void, delete_textures, DeleteTextures, (int, const unsigned *))        \
    F(void, bind_texture, BindTexture, (unsigned, unsigned))                 \
    F(void, tex_image_2d, TexImage2D,                                        \
      (unsigned, int, int, int, int, int, unsigned, unsigned, const void *)) \
    F(void, tex_parameteri, TexParameteri, (unsigned, unsigned, int))        \
    F(void, active_texture, ActiveTexture, (unsigned))                       \
    F(unsigned, create_shader, CreateShader, (unsigned))                     \
    F(void, shader_source, ShaderSource,                                     \
      (unsigned, int, const char *const *, const int *))                     \
    F(void, compile_shader, CompileShader, (unsigned))                       \
    F(void, get_shaderiv, GetShaderiv, (unsigned, unsigned, int *))          \
    F(void, get_shader_info_log, GetShaderInfoLog,                           \
      (unsigned, int, int *, char *))                                        \
    F(void, delete_shader, DeleteShader, (unsigned))                         \
    F(unsigned, create_program, CreateProgram, ())                           \
    F(void, attach_shader, AttachShader, (unsigned, unsigned))               \
    F(void, bind_attrib_location, BindAttribLocation,                        \
      (unsigned, unsigned, const char *))                                    \
    F(void, link_program, LinkProgram, (unsigned))                           \
    F(void, get_programiv, GetProgramiv, (unsigned, unsigned, int *))        \
    F(void, get_program_info_log, GetProgramInfoLog,                         \
      (unsigned, int, int *, char *))                                        \
    F(void, delete_program, DeleteProgram, (unsigned))                       \
    F(void, use_program, UseProgram, (unsigned))                             \
    F(int, get_uniform_location, GetUniformLocation,                         \
      (unsigned, const char *))                                              \
    F(void, uniform1i, Uniform1i, (int, int))                                \
    F(unsigned, get_uniform_block_index, GetUniformBlockIndex,               \
      (unsigned, const char *))                                              \
    F(void, uniform_block_binding, UniformBlockBinding,                      \
      (unsigned, unsigned, unsigned))                                        \
    F(void, get_active_uniform_blockiv, GetActiveUniformBlockiv,             \
      (unsigned, unsigned, unsigned, int *))                                 \
    F(void, get_uniform_indices, GetUniformIndices,                          \
      (unsigned, int, const char *const *, unsigned *))                      \
    F(void, get_active_uniformsiv, GetActiveUniformsiv,                      \
      (unsigned, int, const unsigned *, unsigned, int *))                    \
    F(void, gen_buffers, GenBuffers, (int, unsigned *))                      \
    F(void, delete_buffers, DeleteBuffers, (int, const unsigned *))          \
    F(void, bind_buffer, BindBuffer, (unsigned, unsigned))                   \
    F(void, buffer_data, BufferData,                                         \
      (unsigned, gl_size, const void *, unsigned))                           \
    F(void, bind_buffer_base, BindBufferBase,                                \
      (unsigned, unsigned, unsigned))                                        \
    F(void, gen_vertex_arrays, GenVertexArrays, (int, unsigned *))           \
    F(void, delete_vertex_arrays, DeleteVertexArrays,                        \
      (int, const unsigned *))                                               \
    F(void, bind_vertex_array, BindVertexArray, (unsigned))                  \
    F(void, enable_vertex_attrib_array, EnableVertexAttribArray, (unsigned)) \
    F(void, vertex_attrib_pointer, VertexAttribPointer,                      \
      (unsigned, int, unsigned, gl_bool, int, const void *))                 \
    F(void, draw_arrays, DrawArrays, (unsigned, int, int))                   \
    F(void, gen_framebuffers, GenFramebuffers, (int, unsigned *))            \
    F(void, delete_framebuffers, DeleteFramebuffers,                         \
      (int, const unsigned *))                                               \
    F(void, bind_framebuffer, BindFramebuffer, (unsigned, unsigned))         \
    F(void, framebuffer_texture_2d, FramebufferTexture2D,                    \
      (unsigned, unsigned, unsigned, unsigned, int))                         \
    F(unsigned, check_framebuffer_status, CheckFramebufferStatus,            \
      (unsigned))                                                            \
    F(void, viewport, Viewport, (int, int, int, int))                        \
    F(void, read_pixels, ReadPixels,                                         \
      (int, int, int, int, unsigned, unsigned, void *))                      \
    F(const unsigned char *, get_string, GetString, (unsigned))              \
    F(void, get_integerv, GetIntegerv, (unsigned, int *))                    \
    F(unsigned, get_error, GetError, ())
    struct glsl_api
    {
#define DECLARE(R, N, SDK, A) R(NATIVE_GL_CALL *N) A = nullptr;
        NATIVE_GL_FUNCTIONS(DECLARE)
#undef DECLARE
        // Load the required OpenGL 3.3 entry points or fail explicitly.
        explicit glsl_api(glsl_context &context);
    };
} // namespace native::detail
#undef NATIVE_GL_CALL

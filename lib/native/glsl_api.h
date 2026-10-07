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
    using gl_uint=unsigned;using gl_enum=unsigned;using gl_int=int;
    using gl_size=std::ptrdiff_t;using gl_bool=unsigned char;
#define NATIVE_GL_FUNCTIONS(F) \
    F(void,GenTextures,(int,unsigned*)) \
    F(void,DeleteTextures,(int,const unsigned*)) \
    F(void,BindTexture,(unsigned,unsigned)) \
    F(void,TexImage2D,(unsigned,int,int,int,int,int,unsigned,unsigned,const void*)) \
    F(void,TexParameteri,(unsigned,unsigned,int)) \
    F(void,ActiveTexture,(unsigned)) \
    F(unsigned,CreateShader,(unsigned)) \
    F(void,ShaderSource,(unsigned,int,const char*const*,const int*)) \
    F(void,CompileShader,(unsigned)) \
    F(void,GetShaderiv,(unsigned,unsigned,int*)) \
    F(void,GetShaderInfoLog,(unsigned,int,int*,char*)) \
    F(void,DeleteShader,(unsigned)) \
    F(unsigned,CreateProgram,()) \
    F(void,AttachShader,(unsigned,unsigned)) \
    F(void,BindAttribLocation,(unsigned,unsigned,const char*)) \
    F(void,LinkProgram,(unsigned)) \
    F(void,GetProgramiv,(unsigned,unsigned,int*)) \
    F(void,GetProgramInfoLog,(unsigned,int,int*,char*)) \
    F(void,DeleteProgram,(unsigned)) \
    F(void,UseProgram,(unsigned)) \
    F(int,GetUniformLocation,(unsigned,const char*)) \
    F(void,Uniform1i,(int,int)) \
    F(unsigned,GetUniformBlockIndex,(unsigned,const char*)) \
    F(void,UniformBlockBinding,(unsigned,unsigned,unsigned)) \
    F(void,GetActiveUniformBlockiv,(unsigned,unsigned,unsigned,int*)) \
    F(void,GetUniformIndices,(unsigned,int,const char*const*,unsigned*)) \
    F(void,GetActiveUniformsiv,(unsigned,int,const unsigned*,unsigned,int*)) \
    F(void,GenBuffers,(int,unsigned*)) \
    F(void,DeleteBuffers,(int,const unsigned*)) \
    F(void,BindBuffer,(unsigned,unsigned)) \
    F(void,BufferData,(unsigned,gl_size,const void*,unsigned)) \
    F(void,BindBufferBase,(unsigned,unsigned,unsigned)) \
    F(void,GenVertexArrays,(int,unsigned*)) \
    F(void,DeleteVertexArrays,(int,const unsigned*)) \
    F(void,BindVertexArray,(unsigned)) \
    F(void,EnableVertexAttribArray,(unsigned)) \
    F(void,VertexAttribPointer,(unsigned,int,unsigned,gl_bool,int,const void*)) \
    F(void,DrawArrays,(unsigned,int,int)) \
    F(void,GenFramebuffers,(int,unsigned*)) \
    F(void,DeleteFramebuffers,(int,const unsigned*)) \
    F(void,BindFramebuffer,(unsigned,unsigned)) \
    F(void,FramebufferTexture2D,(unsigned,unsigned,unsigned,unsigned,int)) \
    F(unsigned,CheckFramebufferStatus,(unsigned)) \
    F(void,Viewport,(int,int,int,int)) \
    F(void,ReadPixels,(int,int,int,int,unsigned,unsigned,void*)) \
    F(const unsigned char*,GetString,(unsigned)) \
    F(void,GetIntegerv,(unsigned,int*)) \
    F(unsigned,GetError,())
    struct glsl_api {
#define DECLARE(R,N,A) R (NATIVE_GL_CALL *N) A = nullptr;
        NATIVE_GL_FUNCTIONS(DECLARE)
#undef DECLARE
        explicit glsl_api(glsl_context &context) {
#define LOAD(R,N,A) N=reinterpret_cast<decltype(N)>(context.proc("gl" #N)); if(!N)throw std::runtime_error("OpenGL entry point unavailable: gl" #N);
            NATIVE_GL_FUNCTIONS(LOAD)
#undef LOAD
        }
    };
}
#undef NATIVE_GL_CALL

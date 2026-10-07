//
// Owns an offscreen macOS CGL core context for original GLSL image shaders.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../glsl_context.h"
#include <OpenGL/OpenGL.h>
#include <dlfcn.h>
#include <stdexcept>

namespace
{
    class cgl_context final : public native::detail::glsl_context {
    public:
        cgl_context() {
            const CGLPixelFormatAttribute attributes[]={kCGLPFAOpenGLProfile,
                static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_4_1_Core),
                kCGLPFAColorSize,static_cast<CGLPixelFormatAttribute>(32),
                static_cast<CGLPixelFormatAttribute>(0)};
            CGLPixelFormatObj format=nullptr;GLint count=0;
            if(CGLChoosePixelFormat(attributes,&format,&count)!=kCGLNoError||!format)fail();
            const auto error=CGLCreateContext(format,nullptr,&context);
            CGLDestroyPixelFormat(format);if(error!=kCGLNoError||!context)fail();
            library=dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL",RTLD_NOW|RTLD_LOCAL);
            if(!library){CGLDestroyContext(context);fail();}
        }
        ~cgl_context() override {
            if(CGLGetCurrentContext()==context)CGLSetCurrentContext(previous);
            if(context)CGLDestroyContext(context);if(library)dlclose(library);
        }
        void enter() override {
            previous=CGLGetCurrentContext();if(CGLSetCurrentContext(context)!=kCGLNoError)fail();
        }
        void leave() override {CGLSetCurrentContext(previous);}
        void *proc(const char *name) override {return dlsym(library,name);}
    private:
        [[noreturn]] void fail(){throw std::runtime_error("macOS OpenGL core image context unavailable.");}
        CGLContextObj context=nullptr,previous=nullptr;void *library=nullptr;
    };
}
namespace native::detail
{
    std::unique_ptr<glsl_context> make_glsl_context(){return std::make_unique<cgl_context>();}
}

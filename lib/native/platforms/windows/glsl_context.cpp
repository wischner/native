//
// Owns a hidden Win32 OpenGL context for portable GLSL image programs.
// Context resources stay in the shader peer and never reach public headers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../../glsl_context.h"
#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <stdexcept>

namespace
{
    class wgl_context final : public native::detail::glsl_context {
    public:
        wgl_context() {
            try {
                library=LoadLibraryW(L"opengl32.dll");
                if(!library)fail();
                WNDCLASSW type{};type.lpfnWndProc=DefWindowProcW;
                type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"NativeGLSLImageContext";
                if(!RegisterClassW(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)fail();
                window=CreateWindowW(type.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,type.hInstance,nullptr);
                if(!window)fail();dc=GetDC(window);if(!dc)fail();
                PIXELFORMATDESCRIPTOR format{};format.nSize=sizeof(format);format.nVersion=1;
                format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
                format.iPixelType=PFD_TYPE_RGBA;format.cColorBits=32;format.cAlphaBits=8;
                const int index=ChoosePixelFormat(dc,&format);
                if(!index||!SetPixelFormat(dc,index,&format))fail();
                context=wglCreateContext(dc);if(!context)fail();enter();
                using create_type=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
                auto create=reinterpret_cast<create_type>(wglGetProcAddress("wglCreateContextAttribsARB"));
                if(!create)fail();
                const int attributes[]={0x2091,3,0x2092,3,0x9126,1,0};
                HGLRC modern=create(dc,nullptr,attributes);if(!modern)fail();
                leave();wglDeleteContext(context);context=modern;
            }catch(...){cleanup();throw;}
        }
        ~wgl_context() override {cleanup();}
        void enter() override {
            previous_context=wglGetCurrentContext();previous_dc=wglGetCurrentDC();
            if(!wglMakeCurrent(dc,context))fail();
        }
        void leave() override {wglMakeCurrent(previous_dc,previous_context);}
        void *proc(const char *name) override {
            auto pointer=wglGetProcAddress(name);
            const auto value=reinterpret_cast<std::intptr_t>(pointer);
            if(!pointer||value==1||value==2||value==3||value==-1)pointer=GetProcAddress(library,name);
            return reinterpret_cast<void*>(pointer);
        }
    private:
        [[noreturn]] void fail(){throw std::runtime_error("OpenGL 3.3 is unavailable on this Windows desktop.");}
        void cleanup() noexcept {
            if(wglGetCurrentContext()==context)wglMakeCurrent(previous_dc,previous_context);
            if(context)wglDeleteContext(context);
            if(dc&&window)ReleaseDC(window,dc);if(window)DestroyWindow(window);
            if(library)FreeLibrary(library);
        }
        HWND window=nullptr;HDC dc=nullptr,previous_dc=nullptr;
        HGLRC context=nullptr,previous_context=nullptr;HMODULE library=nullptr;
    };
}
namespace native::detail
{
    std::unique_ptr<glsl_context> make_glsl_context(){return std::make_unique<wgl_context>();}
}

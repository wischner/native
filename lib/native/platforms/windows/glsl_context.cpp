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
                if(!window)fail();
                dc=GetDC(window);
                if(!dc)fail();
                PIXELFORMATDESCRIPTOR format{};format.nSize=sizeof(format);format.nVersion=1;
                format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
                format.iPixelType=PFD_TYPE_RGBA;format.cColorBits=32;format.cAlphaBits=8;
                const int index=ChoosePixelFormat(dc,&format);
                if(!index||!SetPixelFormat(dc,index,&format))fail();
                context=wglCreateContext(dc);
                if(!context)fail();
                enter();
                using create_type=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
                auto create=reinterpret_cast<create_type>(proc("wglCreateContextAttribsARB"));
                if(!create)fail();
                const int attributes[]={0x2091,3,0x2092,3,0x9126,1,0};
                HGLRC modern=create(dc,nullptr,attributes);
                if(!modern)fail();
                try { leave(); } catch (...) {
                    wglDeleteContext(modern);
                    throw;
                }
                wglDeleteContext(context);context=modern;
            }catch(...){cleanup();throw;}
        }
        ~wgl_context() override {cleanup();}
        void enter() override {
            if (entered)
                throw std::logic_error("Nested OpenGL image context activation.");
            previous_context=wglGetCurrentContext();previous_dc=wglGetCurrentDC();
            if (!wglMakeCurrent(dc, context)) {
                // WGL may clear the previous binding on activation failure.
                if (!wglMakeCurrent(previous_dc, previous_context))
                    throw std::runtime_error(
                        "Cannot restore WGL after image activation failed.");
                fail();
            }
            entered = true;
        }
        void leave() override {
            if (!entered) return;
            if (!wglMakeCurrent(previous_dc, previous_context))
                throw std::runtime_error("Cannot restore the toolkit WGL context.");
            entered = false;
        }
        void *proc(const char *name) override {
            auto pointer=wglGetProcAddress(name);
            const auto value=reinterpret_cast<std::intptr_t>(pointer);
            if(!pointer||value==1||value==2||value==3||value==-1)pointer=GetProcAddress(library,name);
            return reinterpret_cast<void*>(pointer);
        }
    private:
        [[noreturn]] void fail(){throw std::runtime_error("OpenGL 3.3 is unavailable on this Windows desktop.");}
        void cleanup() noexcept {
            if (entered) {
                try { leave(); } catch (...) {
                    if (wglGetCurrentContext() == context)
                        wglMakeCurrent(nullptr, nullptr);
                }
            }
            if(context)wglDeleteContext(context);
            if(dc&&window)ReleaseDC(window,dc);
            if(window)DestroyWindow(window);
            if(library)FreeLibrary(library);
        }
        bool entered = false;
        HWND window=nullptr;HDC dc=nullptr,previous_dc=nullptr;
        HGLRC context=nullptr,previous_context=nullptr;HMODULE library=nullptr;
    };
}
namespace native::detail
{
    std::unique_ptr<glsl_context> make_glsl_context(){return std::make_unique<wgl_context>();}
}

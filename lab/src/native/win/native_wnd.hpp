//
// native_wnd.hpp
// 
// Native window for MS Windows.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 09.05.2021   tstih
// 
#ifndef _NATIVE_WND_H
#define _NATIVE_WND_H

namespace nice {

//{{BEGIN.DEC}}
    class wnd; // Forward declaration.
    class native_wnd {
    public:
        // Ctor and dtor.
        native_wnd(wnd *window);
        virtual ~native_wnd();
        // Method(s).
        void destroy(void);
        void repaint(void);
        std::string get_title();
        void set_title(std::string s); 
        size get_wsize();
        void set_wsize(size sz);
        pt get_location();
        void set_location(pt location);
        rct get_paint_area();
    protected:
        // Window variables.
        HWND hwnd_;
        WNDCLASSEX wcex_;
        std::string class_;
        // Global and local window procedures.
        static LRESULT CALLBACK global_wnd_proc(
            HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
        virtual LRESULT local_wnd_proc(
            UINT msg, WPARAM wparam, LPARAM lparam);
        wnd* window_;
    };
//{{END.DEC}}

} // namespace nice

#endif // _NATIVE_WND_H 
/*
 * nice.hpp
 * 
 * (Single) header file for the nice GUI library.
 * 
 * (c) 2020 - 2021 Tomaz Stih
 * This code is licensed under MIT license (see LICENSE.txt for details).
 * 
 * 02.06.2021   tstih
 * 
 */
#ifndef _NICE_HPP
#define _NICE_HPP

#ifdef __WIN__
extern "C" {
#include <windows.h>
#include <windowsx.h>
}

#elif __X11__
extern "C" {
#define nice unix_nice
#include <unistd.h>
#undef nice
#include <stdint.h>
#include <fcntl.h>
#include <sys/file.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
}

#elif __SDL__
extern "C" {
#define nice unix_nice
#include <unistd.h>
#undef nice
#include <stdint.h>
#include <fcntl.h>
#include <sys/file.h>

#include <SDL2/SDL.h>
}

#endif

#include <exception>
#include <string>
#include <sstream>
#include <functional>
#include <map>
#include <filesystem>


namespace nice {

#ifdef __WIN__
    // Mapped to Win32 process id.
    typedef DWORD  app_id;

    // Mapped to Win32 application instace (passed to WinMain)
    typedef HINSTANCE app_instance;

    // Screen coordinate for all geometry functions.
    typedef LONG coord;

    // 8 bit integer.
    typedef BYTE byte;

    // Mapped to device context.
    typedef HDC canvas;

#elif __X11__
    // Unix process id.
    typedef pid_t app_id;

    // Basic X11 stuff.
    typedef struct x11_app_instance {
        Display* display;
    } app_instance;

    // X11 coordinate.
    typedef int coord;

    // 8 bit integer.
    typedef uint8_t byte;

    // X11 GC and required stuff.
    typedef struct x11_canvas {
        Display* d;
        Window w;
        GC gc;
    } canvas;

#elif __SDL__
    // Unix process id.
    typedef pid_t app_id;

    // Basic X11 stuff.
    typedef int app_instance;

    // X11 coordinate.
    typedef int coord;

    // 8 bit integer.
    typedef uint8_t byte;

    // X11 GC and required stuff.
    typedef SDL_Renderer* canvas;

#endif

#define throw_ex(ex, what) \
        throw ex(what, __FILE__,__FUNCTION__,__LINE__);

    class nice_exception : public std::exception {
    public:
        nice_exception(
            std::string what,
            std::string file = nullptr,
            std::string func = nullptr,
            int line = 0) : what_(what), file_(file), func_(func), line_(line) {};
        std::string what() { return what_; }
    protected:
        std::string what_;
        std::string file_; // __FILE__
        std::string func_; // __FUNCTION__
        int line_; // __LINE__
    };

    template <typename... Args>
    class signal {
    public:
        signal() : current_id_(0) {}
        signal(std::function<void()> init) : signal() { init_ = init; }
        template <typename T> int connect(T* inst, bool (T::* func)(Args...)) {
            return connect([=](Args... args) {
                return (inst->*func)(args...);
                });
        }

        template <typename T> int connect(T* inst, bool (T::* func)(Args...) const) {
            return connect([=](Args... args) {
                return (inst->*func)(args...);
                });
        }

        int connect(std::function<bool(Args...)> const& slot) const {
            if (!initialized_ && init_ != nullptr) { init_(); initialized_ = true; }
            slots_.insert(std::make_pair(++current_id_, slot));
            return current_id_;
        }

        void disconnect(int id) const {
            slots_.erase(id);
        }

        void disconnect_all() const {
            slots_.clear();
        }

        void emit(Args... p) {
            // Iterate in reverse order to first emit to last connections.
            for (auto it = slots_.rbegin(); it != slots_.rend(); ++it) {
                if (it->second(std::forward<Args>(p)...)) break;
            }
        }
    private:
        mutable std::map<int, std::function<bool(Args...)>> slots_;
        mutable int current_id_;
        mutable bool initialized_{ false };
        std::function<void()> init_{ nullptr };
    };

   class percent
    {
        double percent_;
    public:
        class pc {};
        explicit constexpr percent(pc, double dpc) : percent_{ dpc } {}
    };

    class pixel
    {
        int pixel_;
    public:
        class px {};
        explicit constexpr pixel(px, int ipx) : pixel_{ ipx } {}
        int value() { return pixel_; }
    };


    template<typename T>
    class ro_property {
    public:
        ro_property(
            std::function<T()> getter) :
            getter_(getter) { }
        operator T() const { return getter_(); }
    private:
        std::function<T()> getter_;
    };

    template<typename T>
    class property {
    public:
        property(
            std::function<void(T)> setter,
            std::function<T()> getter) :
            setter_(setter), getter_(getter) { }
        operator T() const { return getter_(); }
        property<T>& operator= (const T& value) { setter_(value); return *this; }
    private:
        std::function<void(T)> setter_;
        std::function<T()> getter_;
    };

    typedef struct size_s {
        union { coord width; coord w; };
        union { coord height; coord h; };
    } size;

    typedef struct color_s {
        byte r;
        byte g;
        byte b;
        byte a;
    } color;

    typedef struct pt_s {
        union { coord left; coord x; };
        union { coord top; coord y; };
    } pt;

    typedef struct rct_s {
        union { coord left; coord x; coord x1; };
        union { coord top; coord y;  coord y1; };
        union { coord width; coord w; };
        union { coord height; coord h; };
        coord x2() { return left + width; }
        coord y2() { return top + height; }
    } rct;

#ifdef __WIN__
    class native_raster {
    public:
        // Construct a raster from resource.
        native_raster(int width, int height, const uint8_t *bgra);
        // Allocate resource.
        native_raster(int width, int height);  
        virtual ~native_raster();
        // Width.
        int width() const;
        // Height.
        int height() const;
        // Pointer to raw data.
        uint8_t* raw() const;
    private:
        int width_, height_, len_;
        std::unique_ptr<uint8_t[]> raw_; // We own this!
    };

#elif __X11__
    class native_raster {
    public:
        // Construct a raster from resource.
        native_raster(int width, int height, const uint8_t *bgra);
        // Allocate resource.
        native_raster(int width, int height);  
        virtual ~native_raster();
        // Width.
        int width() const;
        // Height.
        int height() const;
        // Pointer to raw data.
        uint8_t* raw() const;
    private:
        int width_, height_, len_;
        std::unique_ptr<uint8_t[]> raw_; // We own this!
    };

#elif __SDL__
    class native_raster {
    public:
        // Construct a raster from resource.
        native_raster(int width, int height, const uint8_t *bgra);
        // Allocate resource.
        native_raster(int width, int height);  
        virtual ~native_raster();
        // Width.
        int width() const;
        // Height.
        int height() const;
        // Pointer to raw data.
        uint8_t* raw() const;
    private:
        int width_, height_, len_;
        std::unique_ptr<uint8_t[]> raw_; // We own this!
    };

#endif
    class raster {
    public:
        // Constructs a new raster.        
        raster(int width, int height) {
            native_=std::make_unique<native_raster>(width, height);
        }
        // Construct a raster from resource.
        raster(int width, int height, const uint8_t * argb) {
            native_=std::make_unique<native_raster>(width, height, argb);
        }
        // Destructs the raster.
        virtual ~raster() {};
        // Width.
        int width() const { return native_->width(); }
        // Height.
        int height() const { return native_->height(); }
        // Pointer to raw data.
        uint8_t* raw() const { return native_->raw(); }
    private:
        // PIMPL.
        std::unique_ptr<native_raster> native_;
    };

    struct resized_info {
        coord width;
        coord height;
    };

    // Buton status: true=down, false=up.
    struct mouse_info {
        pt location;
        bool left_button;       
        bool middle_button;
        bool right_button;
        bool ctrl;
        bool shift;
    };

    class artist {
    public:
        // Pass canvas instance, don't own it.
        artist(const canvas& canvas) {
            canvas_ = canvas;
        }
        // Methods.
        void draw_line(color c, pt p1, pt p2) const;
        void draw_rect(color c, rct r) const;
        void fill_rect(color c, rct r) const;
        void draw_raster(const raster& rst, pt p) const;
    private:
        // Passed canvas.
        canvas canvas_;
    };

#ifdef __WIN__
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

    class app_wnd; // Forward declaration.
    class native_app_wnd : public native_wnd {
    public:
        native_app_wnd(
            app_wnd *window,
            std::string title,
            size size
        );
        virtual ~native_app_wnd();
        void show() const;
    };

#elif __X11__
    class wnd; // Forward declaration.
    class native_wnd {
    public:
        // Ctor creates X11 window. 
        native_wnd(wnd *window);
        // Dtor.
        virtual ~native_wnd();
        // Destroy native window.
        void destroy(void);
        // Invalidate native window.
        void repaint(void);
        // Get window title.
        std::string get_title();
        // Set window title.
        void set_title(std::string s);
        // Get window size (not client size). 
        size get_wsize();
        // Set window size.
        void set_wsize(size sz);
        // Get window relative location (to parent)
        // or absolute location if parent is screen. 
        pt get_location();
        // Set window location.
        void set_location(pt location);
        // Get window paint rectangle.
        rct get_paint_area();
        // Global window procedure (static)
        static bool global_wnd_proc(const XEvent& e);
    protected:
        // X11 window structure.
        Window winst_; 
        // X11 display.
        Display* display_;
        // A map from X11 window to native_wnd.
        static std::map<Window,native_wnd*> wmap_;
        // Local window procdure.
        virtual bool local_wnd_proc(const XEvent& e);
        // Pointer to related non-native window struct.
        wnd* window_;
        // Cached size.
        size cached_wsize_;
        GC cached_gc_ {0};
    };

    class app_wnd; // Forward declaration.
    class native_app_wnd : public native_wnd {
    public:
        native_app_wnd(
            app_wnd *window,
            std::string title,
            size size
        );
        virtual ~native_app_wnd();
        void show() const;
    };

#elif __SDL__
    class wnd; // Forward declaration.
    class native_wnd {
    public:
        // Ctor creates X11 window. 
        native_wnd(wnd *window);
        // Dtor.
        virtual ~native_wnd();
        // Destroy native window.
        void destroy(void);
        // Invalidate native window.
        void repaint(void);
        // Get window title.
        std::string get_title();
        // Set window title.
        void set_title(std::string s);
        // Get window size (not client size). 
        size get_wsize();
        // Set window size.
        void set_wsize(size sz);
        // Get window relative location (to parent)
        // or absolute location if parent is screen. 
        pt get_location();
        // Set window location.
        void set_location(pt location);
        // Get window paint rectangle.
        rct get_paint_area();
        // Global window procedure (static)
        static bool global_wnd_proc(const SDL_Event& e);
    protected:
        // Native SDL window structure.
        SDL_Window* winst_; 
        // Window surface.
        SDL_Renderer *wrenderer_;
        // A map from X11 window to native_wnd.
        static std::map<SDL_Window *,native_wnd*> wmap_;
        // Local window procdure.
        virtual bool local_wnd_proc(const SDL_Event& e);
        // Pointer to related non-native window struct.
        wnd* window_;
    };

    class app_wnd; // Forward declaration.
    class native_app_wnd : public native_wnd {
    public:
        native_app_wnd(
            app_wnd *window,
            std::string title,
            size size
        );
        virtual ~native_app_wnd();
        void show() const;
    };

#endif
    class wnd  {
    public:
        // Methods.
        void repaint(void);

        // Properties.
        property<std::string> title {
            [this](std::string s) { this->set_title(s); },
            [this]() -> std::string {  return this->get_title(); }
        };

        property<size> wsize {
            [this](size sz) { this->set_wsize(sz); },
            [this]() -> size {  return this->get_wsize(); }
        };

        property<pt> location {
            [this](pt p) { this->set_location(p); },
            [this]() -> pt {  return this->get_location(); }
        };

        ro_property<rct> paint_area{
            [this]() -> rct { return this->get_paint_area(); }
        };

        // Signals.
        signal<> created;
        signal<> destroyed;
        signal<const artist&> paint;
        signal<const resized_info&> resized;
        signal<const mouse_info&> mouse_move;
        signal<const mouse_info&> mouse_down;
        signal<const mouse_info&> mouse_up;

    protected:
        // Setters and getters.
        virtual std::string get_title();
        virtual void set_title(std::string s);
        virtual size get_wsize();
        virtual void set_wsize(size sz);
        virtual pt get_location();
        virtual void set_location(pt location);
        virtual rct get_paint_area();

        // Pimpl. Concrete window must implement this!
        virtual native_wnd* native() = 0;
    };

    class app_wnd : public wnd {
    public:
        app_wnd(std::string title, size size) : native_(nullptr) {
            // Store parameters.
            title_ = title; size_ = size;
            // Subscribe to destroy signal.
            destroyed.connect(this, &app_wnd::on_destroy);
        }
        void show();
        
    protected:
        // Destroyed handler...
        bool on_destroy();         
        // Pimpl implementation.
        virtual native_app_wnd* native() override;
    private:
        std::string title_;
        size size_;
        std::unique_ptr<native_app_wnd> native_;
    };

    class app {
    public:
        // Cmd line arguments.
        static int argc;
        static char **argv;

        // Return code.
        static int ret_code;

        // Application (process) id.
        static app_id id();

        // Application name. First cmd line arg without extension.
        static std::string name();

        // Application instance get and set.
        static app_instance instance();
        static void instance(app_instance instance);

        // Is another instance already running?
        static bool is_primary_instance();

        // Main desktop application loop.
        static void run(const app_wnd& w);

    private:
        static bool primary_;
        static app_instance instance_;     
    };

    class wave {
    public:
        // Construct an audio class.
        wave(const uint8_t *wav) {
            // Get wave header.
            phdr = (header_s *)wav;
        }
        // Duration in seconds.
        float duration_in_seconds() const {
            return (float)phdr->overall_size / (float)phdr->byterate;
        }
        // Get overall size.
        uint32_t len() const {
            return phdr->overall_size;
        }
        // Get raw wave.
        void* raw() const { return (void*)phdr; };

    private:
        // WAVE file header format. We are not the owner of the resource
        // so we'll just store the pointer.
        struct header_s {
            uint8_t riff[4];                // RIFF string
            uint32_t overall_size;		    // overall size of file in bytes
            uint8_t wave[4];			    // WAVE string
            uint8_t fmt_chunk_marker[4];    // fmt string with trailing null char
            uint32_t length_of_fmt;		    // length of the format data
            uint16_t format_type;		    // format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
            uint16_t channels;			    // no.of channels
            uint32_t sample_rate;		    // sampling rate (blocks per second)
            uint32_t byterate;			    // SampleRate * NumChannels * BitsPerSample/8
            uint16_t block_align;		    // NumChannels * BitsPerSample/8
            uint16_t bits_per_sample;	    // bits per sample, 8- 8bits, 16- 16 bits etc
            uint8_t data_chunk_header [4];	// DATA string or FLLR string
            uint32_t data_size;				// NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
            uint8_t data[0];                // Pointer to data.
        } *phdr;
    };

#ifdef __WIN__
    class native_audio {
    public:
        // Construct an audio class.
        native_audio();
        // Destructs the audio class.
        virtual ~native_audio();
        // Play wave.
        void play_wave_async(const wave& w);
    };

#elif __X11__
    class native_audio {
    public:
        // Construct an audio class.
        native_audio();
        // Destructs the audio class.
        virtual ~native_audio();
        // Play wave.
        void play_wave_async(const wave& w);
    };

#elif __SDL__
    class native_audio {
    public:
        // Construct an audio class.
        native_audio();
        // Destructs the audio class.
        virtual ~native_audio();
        // Play wave.
        void play_wave_async(const wave& w);
    };

#endif
    class audio {
    public:
        // Construct an audio class.
        audio() : pimpl_(std::make_unique<native_audio>()) {}
        // Destructs the audio class.
        virtual ~audio() {}
        // Play wave.
        void play_wave_async(const wave& w) {
            pimpl_->play_wave_async(w);
        }
    private:
        std::unique_ptr<native_audio> pimpl_;
    };


    constexpr percent operator "" _pc(long double dpc)
    {
        return percent{ percent::pc{}, static_cast<double>(dpc) };
    }

    constexpr pixel operator "" _px(unsigned long long ipx)
    {
        return pixel{ pixel::px{}, static_cast<int>(ipx) };
    }


    int app::ret_code = 0;
    int app::argc = 0;
    char **app::argv = nullptr;
    bool app::primary_ = false;
    app_instance app::instance_;

    app_instance app::instance() {
        return instance_;
    }

    void app::instance(app_instance instance) {
        instance_ = instance;
    }

    std::string app::name() {
        return std::filesystem::path(argv[0]).stem().string();
    }
    void wnd::repaint(void) { native()->repaint(); }
    
    std::string wnd::get_title() { return native()->get_title(); }
    
    void wnd::set_title(std::string s) { native()->set_title(s); } 
    
    size wnd::get_wsize() { return native()->get_wsize(); }
    
    void wnd::set_wsize(size sz) { native()->set_wsize(sz); } 
    
    pt wnd::get_location() { return native()->get_location(); }
    
    void wnd::set_location(pt location) { native()->set_location(location); } 

    rct wnd::get_paint_area() { return native()->get_paint_area(); };
    bool app_wnd::on_destroy() {
        // Destroy native window.
        native()->destroy();
        // And tell the world we handled it.
        return true;
    }   

    native_app_wnd* app_wnd::native() {
        if (native_==nullptr) native_=
            std::make_unique<native_app_wnd>(this, title_, size_);
        return native_.get();
    }

    void app_wnd::show() {
        native()->show();
    }


#ifdef __WIN__

    void artist::draw_line(color c, pt p1, pt p2) const {
        HPEN pen = ::CreatePen(PS_SOLID, 1, RGB(c.r, c.g, c.b));
        ::SelectObject(canvas_, pen);
        POINT pt;
        ::MoveToEx(canvas_, p1.x, p1.y, &pt);
        ::LineTo(canvas_, p2.x, p2.y);
        ::DeleteObject(pen);
    }

    void artist::draw_rect(color c, rct r) const {
        RECT rect{ r.left, r.top, r.x2(), r.y2() };
        HBRUSH brush = ::CreateSolidBrush(RGB(c.r, c.g, c.b));
        ::FrameRect(canvas_, &rect, brush);
        ::DeleteObject(brush);
    }

    void artist::fill_rect(color c, rct r) const {   
        RECT rect{ r.left, r.top, r.x2(), r.y2() };
        HBRUSH brush = ::CreateSolidBrush(RGB(c.r, c.g, c.b));
        ::FillRect(canvas_, &rect, brush);
        ::DeleteObject(brush);
    }

    // Know how from: https://www-user.tu-chemnitz.de/~heha/petzold/ch14e.htm
    // http://www.winprog.org/tutorial/bitmaps.html
    // http://www.fengyuan.com/article/alphablend.html
    void artist::draw_raster(const raster& rst, pt p) const {
        BITMAPINFO bmi;
        ::ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = rst.width();
        bmi.bmiHeader.biHeight = -(rst.height()); // Windows magic. Rasters are bottom up.
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB; // But it is really BGR!
        ::SetDIBitsToDevice(canvas_,
            p.x, p.y, rst.width(), rst.height(),
            0, 0,
            0, rst.height(), 
            rst.raw(),
            &bmi,
            DIB_RGB_COLORS
        );
    }
    native_app_wnd::native_app_wnd(
        app_wnd *window,
        std::string title,
        size size
    ) : native_wnd(window) {

        // Create app window.
        class_ = app::name();

        // Register window.
        ::ZeroMemory(&wcex_, sizeof(WNDCLASSEX));
        wcex_.cbSize = sizeof(WNDCLASSEX);
        wcex_.lpfnWndProc = global_wnd_proc;
        wcex_.hInstance = app::instance();
        wcex_.lpszClassName = class_.c_str();
        wcex_.hCursor = ::LoadCursor(NULL, IDC_ARROW);

        if (!::RegisterClassEx(&wcex_)) 
            throw_ex(nice_exception,"Unable to register class.");

        // Create it.
        hwnd_ = ::CreateWindowEx(
            0,
            class_.c_str(),
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            size.width, size.height,
            NULL,
            NULL,
            app::instance(),
            this);

        if (!hwnd_)
            throw_ex(nice_exception,"Unable to create window.");
    }

    native_app_wnd::~native_app_wnd() {}

    void native_app_wnd::show() const { 
        ::ShowWindow(hwnd_, SW_SHOWNORMAL); 
    }
    void native_wnd::destroy(void) {
        ::PostQuitMessage(0);
    }

    native_wnd::native_wnd(wnd *window) {
        window_=window;
    }

    native_wnd::~native_wnd() {
        ::DestroyWindow(hwnd_);
    }

    void native_wnd::repaint(void) {
         ::InvalidateRect(hwnd_, NULL, TRUE);
    }

    std::string native_wnd::get_title() {
        TCHAR szTitle[1024];
        ::GetWindowTextA(hwnd_, szTitle, 1024);
        return std::string(szTitle);
    }

    void native_wnd::set_title(std::string s) {
        ::SetWindowText(hwnd_,s.c_str());
    }

    size native_wnd::get_wsize() {
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        return size{ wr.right-wr.left+1, wr.bottom-wr.top+1 };
    }

    void native_wnd::set_wsize(size sz) {
         // Use move.
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        ::MoveWindow(hwnd_, wr.left, wr.top, sz.w, sz.h, TRUE);
    }

    pt native_wnd::get_location() {
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        return pt{ wr.left, wr.top };
    }

    void native_wnd::set_location(pt location) {
        // We need to keep the position and just change the size.
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        ::MoveWindow(hwnd_, 
            location.left, 
            location.top, 
            wr.right-wr.left+1, 
            wr.bottom-wr.top+1, TRUE);
    }

    rct native_wnd::get_paint_area() {
        RECT client;
        ::GetClientRect(hwnd_, &client);
        return { client.left, client.top, client.right, client.bottom };
    }

    LRESULT CALLBACK native_wnd::global_wnd_proc(
        HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        
        // Is it the very first message? Only on WM_NCCREATE.
        // TODO: Why does Windows 10 send WM_GETMINMAXINFO first?!
        native_wnd* self = nullptr;
        if (message == WM_NCCREATE) {
            LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            auto self = static_cast<native_wnd*>(lpcs->lpCreateParams);
            self->hwnd_=hWnd; // save the window handle too!
            ::SetWindowLongPtr(
                hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        else
            self = reinterpret_cast<native_wnd*>
                (::GetWindowLongPtr(hWnd, GWLP_USERDATA));

        // Chain...
        if (self != nullptr)
            return (self->local_wnd_proc(message, wParam, lParam));
        else
            return ::DefWindowProc(hWnd, message, wParam, lParam);
    }
    
    LRESULT native_wnd::local_wnd_proc(
        UINT msg, WPARAM wparam, LPARAM lparam) {

        switch (msg) {
            case WM_CREATE:
                window_->created.emit();
                break;
            case WM_DESTROY:
                window_->destroyed.emit();
                break;
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd_, &ps);
                artist a(hdc);
                window_->paint.emit(a);
                EndPaint(hwnd_, &ps);
            }
            break;
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            {
                // Populate the mouse info structure.
                mouse_info mi = {
                    {GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}, // point
                    (bool)(wparam & MK_LBUTTON),
                    (bool)(wparam & MK_MBUTTON),
                    (bool)(wparam & MK_RBUTTON),
                    (bool)(wparam & MK_CONTROL),
                    (bool)(wparam & MK_SHIFT)
                };
                if (msg == WM_MOUSEMOVE)
                    window_->mouse_move.emit(mi);
                else if (msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_RBUTTONDOWN)
                    window_->mouse_down.emit(mi);
                else
                    window_->mouse_up.emit(mi);
            }
            break;
            case WM_SIZE:
            {
                rct r = rct{ 0, 0, LOWORD(lparam), HIWORD(lparam) };
                window_->resized.emit(
                    {
                        LOWORD(lparam),
                        HIWORD(lparam)
                    }
                );
            }
                break;
            default:
                return ::DefWindowProc(hwnd_, msg, wparam, lparam);
            }
            return 0;

    }
    app_id app::id() {
        return ::GetCurrentProcessId();
    }

    bool app::is_primary_instance() {
        // Are we already primary instance? If not, try to become one.
        if (!primary_) {
            std::string aname = app::name();
            // Create local mutex.
            std::ostringstream name;
            name << "Local\\" << aname;
            ::CreateMutex(0, FALSE, name.str().c_str());
            // We are primary instance.
            primary_ = !(::GetLastError() == ERROR_ALREADY_EXISTS);
        }
        return primary_;
    }

    void app::run(const app_wnd& w) {

        // We have to cast the constness away to 
        // call non-const functions on window.
        auto& main_wnd=const_cast<app_wnd &>(w);
        main_wnd.show();

        // Message loop.
        MSG msg;
        while (::GetMessage(&msg, NULL, 0, 0))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        // Finally, set the return code.
        ret_code = (int)msg.wParam;
    }
    native_raster::native_raster(int width, int height, const uint8_t *bgra) :
        native_raster(width,height) {
        // For windows we need 24 bit BGR array.
        uint8_t *dst=raw_.get();
        for (int i=0; i<width*height; i++) {
            int srci=4*i, dsti=3*i;
            dst[dsti] = bgra[srci];
            dst[dsti+1] = bgra[srci+1];
            dst[dsti+2] = bgra[srci+2];
        }
    }
    
    native_raster::native_raster(int width, int height) :
        width_(width), 
        height_(height) {

        // Calculate raster length.
        len_ = width * height * 3; // BGRA!
        // Allocate memory.
        raw_=std::make_unique<uint8_t[]>(len_);
    }  

    native_raster::~native_raster() {

    }

    int native_raster::width() const {
        return width_;
    }

    int native_raster::height() const {
        return height_;
    }

    uint8_t* native_raster::raw() const {
        return raw_.get();
    }

    native_audio::native_audio()
    {
    }

    native_audio::~native_audio()
    {
    }

    void native_audio::play_wave_async(const wave& w)
    {
        
    }


#elif __X11__

    void artist::draw_line(color c, pt p1, pt p2) const {
    }

    void artist::draw_rect(color c, rct r) const {   
    }

    void artist::fill_rect(color c, rct r) const {   
        // 1000 mile walk to create a simple RGB color.
        Colormap cmap=DefaultColormap(canvas_.d,DefaultScreen(canvas_.d));    
        XColor xc;
        xc.red=c.r * 0xff; 
        xc.green=c.g * 0xff; 
        xc.blue=c.b * 0xff;
        xc.flags = DoRed | DoGreen | DoBlue;
        XAllocColor(canvas_.d, cmap, &xc);
        // Set pen.
        XSetForeground(canvas_.d, canvas_.gc, xc.pixel);
        // And fill rect.
        XFillRectangle( canvas_.d, canvas_.w, canvas_.gc, r.x, r.y, r.w, r.h );
    }

    void artist::draw_raster(const raster& rst, pt p) const {
        // Get the visual.
        Visual *visual=DefaultVisual(canvas_.d, DefaultScreen(canvas_.d));
        // Create the iage.
        XImage* img=XCreateImage(
            canvas_.d, 
            visual, 
            24, 
            ZPixmap, 
            0, 
            (char*)rst.raw(),
            rst.width(),
            rst.height(),
            32,
            0);
        // Draw it!
        XPutImage(
            canvas_.d, 
            canvas_.w, 
            canvas_.gc, 
            img, 
            0, 0, p.x, p.y, 
            rst.width(), rst.height());
        // We don't want our raster object wildly released by XDestroyImage.
        img->data=NULL;
        // Destroy the image.
        XDestroyImage(img);
    }
    native_app_wnd::native_app_wnd(
        app_wnd *window,
        std::string title,
        size size
    ) : native_wnd(window) {

        int s = DefaultScreen(display_);
        winst_ = ::XCreateSimpleWindow(
            display_, 
            RootWindow(display_, s), 
            10, // x 
            10, // y
            size.width, 
            size.height, 
            1, // border width
            BlackPixel(display_, s), // border color
            WhitePixel(display_, s)  // background color
        );
        // Store window to window list.
        wmap_.insert(std::pair<Window,native_wnd*>(winst_, this));
        // Set initial title.
        ::XSetStandardProperties(display_,winst_,title.c_str(),NULL,None,NULL,0,NULL);

        // Rather strange handling of close window by X11.
        Atom atom = XInternAtom ( display_,"WM_DELETE_WINDOW", false );
        ::XSetWMProtocols(display_, winst_, &atom, 1);

        // TODO: Implement lazy subscription (somday)
        ::XSelectInput (display_, winst_,
			ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | 
            LeaveWindowMask | PointerMotionMask | FocusChangeMask | KeyPressMask |
            KeyReleaseMask | SubstructureNotifyMask | StructureNotifyMask | 
            SubstructureRedirectMask);
    }

    native_app_wnd::~native_app_wnd() {}

    void native_app_wnd::show() const { 
        ::XMapWindow(display_, winst_);
    }
    // Static variable.
    std::map<Window,native_wnd*> native_wnd::wmap_;

    native_wnd::native_wnd(wnd *window) {
        window_=window;
        display_=app::instance().display;
        // TODO: pass size.
        cached_wsize_={0,0}; // Used to recognize resize events.
    }

    native_wnd::~native_wnd() {
        // And lazy destroy. We could do this in the destroy() function.
        if (cached_gc_!=0) XFreeGC(display_,cached_gc_);
        XDestroyWindow(display_, winst_); winst_=0;
    }

    void native_wnd::destroy() {
        // Remove me from windows map.
        wmap_.erase (winst_); 
    }

    void native_wnd::repaint() {
        XClearArea(display_, winst_, 0, 0, 1, 1, true);
    }

    void native_wnd::set_title(std::string s) {
        ::XStoreName(display_, winst_, s.c_str());
    };
     
    std::string native_wnd::get_title() {
        char * name;
        ::XFetchName(display_,winst_, &name);
        std::string wname=name;
        ::XFree(name);
        return wname;
    }

    size native_wnd::get_wsize() {
        XWindowAttributes wattr;
        ::XGetWindowAttributes(display_,winst_,&wattr);
        // TODO: I think this is just the client area?
        return { wattr.width, wattr.height };
    }

    void native_wnd::set_wsize(size sz) {
        XResizeWindow(display_,winst_, sz.w, sz.h);
    }

    pt native_wnd::get_location() {
        XWindowAttributes wattr;
        ::XGetWindowAttributes(display_,winst_,&wattr);
        // TODO: I think this is just the client area?
        return { wattr.x, wattr.y };
    }

    void native_wnd::set_location(pt location) {
        XMoveWindow(display_,winst_, location.left, location.top);
    }

    // TODO: Implement.
    rct native_wnd::get_paint_area() {
        XWindowAttributes wattr;
        ::XGetWindowAttributes(display_,winst_,&wattr);
        return { 0, 0, wattr.width, wattr.height };
    }

    // Static (global) window proc. For all classes -
    // Remaps the call to local window proc.
    bool native_wnd::global_wnd_proc(const XEvent& e) {
        Window xw = e.xany.window;
        native_wnd* nw = wmap_[xw];
        return nw->local_wnd_proc(e);
    }

    // Local (per window) window proc.
    bool native_wnd::local_wnd_proc(const XEvent& e) {
        bool quit=false;
        switch ( e.type )
        {
        case CreateNotify:
            window_->created.emit();
            break;
        case ClientMessage:
            {
                Atom atom = XInternAtom ( display_,
                            "WM_DELETE_WINDOW",
                            false );
                if ( atom == e.xclient.data.l[0] )
                    window_->destroyed.emit();
                    quit=true;
            }
            break;
        case Expose:
            {
                // Need to create the gc?
                if (cached_gc_==0)
                    cached_gc_= XCreateGC(display_, winst_, 0, NULL); 
                canvas c { display_, winst_, cached_gc_};
                artist a(c);
                window_->paint.emit(a);
            }
		    break;
        case ButtonPress: // https://tronche.com/gui/x/xlib/events/keyboard-pointer/keyboard-pointer.html
        case ButtonRelease:
            {
            mouse_info mi = {
                { e.xmotion.x, e.xmotion.y },
                (bool)(e.xbutton.button&Button1), 
                (bool)(e.xbutton.button&Button2),
                (bool)(e.xbutton.button&Button3),
                (bool)(e.xbutton.state&ControlMask),
                (bool)(e.xbutton.state&ShiftMask)
            };
            if (e.type==ButtonPress)
                window_->mouse_down.emit(mi);
            else
                window_->mouse_up.emit(mi);
            }
            break;
        case MotionNotify:
            {
            mouse_info mi = {
                { e.xmotion.x, e.xmotion.y },
                (bool)(e.xmotion.state&Button1Mask), 
                (bool)(e.xmotion.state&Button2Mask),
                (bool)(e.xmotion.state&Button3Mask),
                (bool)(e.xmotion.state&ControlMask),
                (bool)(e.xmotion.state&ShiftMask)
            };
            window_->mouse_move.emit(mi);
            }
            break;
        case ConfigureNotify:
            {
            XConfigureEvent xce = e.xconfigure;
            // Size change?
            if (xce.width!=cached_wsize_.w || xce.height!=cached_wsize_.h) {
                cached_wsize_.w=xce.width;
                cached_wsize_.h=xce.height;
                window_->resized.emit({xce.width,xce.height});
            }
            }
            break;
        // TODO: KeyPress, KeyRelease
        } // switch
        return quit;
    }
    app_id app::id() {
        return ::getpid();
    }

    bool app::is_primary_instance() {
        // Are we already primary instance? If not, try to become one.
        if (!primary_) {
            std::string aname = app::name();

            // Pid file needs to go to /var/run
            std::ostringstream pfname, pid;
            pfname << "/tmp/" << aname << ".pid";
            pid << nice::app::id() << std::endl;

            // Open, lock, and forget. Let the OS close and unlock.
            int pfd = ::open(pfname.str().c_str(), O_CREAT | O_RDWR, 0666);
            int rc = ::flock(pfd, LOCK_EX | LOCK_NB);
            primary_ = !(rc && EWOULDBLOCK == errno);
            if (primary_) {
                // Write our process id into the file.
                ::write(pfd, pid.str().c_str(), pid.str().length());
                return false;
            }
        }
        return primary_;
    }

    void app::run(const app_wnd& w) {

        // We have to cast the constness away to 
        // call non-const functions on window.
        auto& main_wnd=const_cast<app_wnd &>(w);

        // Show the window.
        main_wnd.show();

        // Flush it all.
        ::XFlush(instance_.display);

        // Main event loop.
        XEvent e;
        bool quit=false;
	    while ( !quit ) // Will be interrupted by the OS.
	    {
	      ::XNextEvent ( instance_.display,&e );
	      quit = native_wnd::global_wnd_proc(e);
	    }
    }
    native_raster::native_raster(int width, int height, const uint8_t *bgra) :
        native_raster(width,height) {
        // Copy complete BGRA array.
        std::copy(bgra, bgra+len_, raw_.get());
    }
    
    native_raster::native_raster(int width, int height) :
        width_(width), 
        height_(height) {

        // Calculate raster length.
        len_ = width * height * 4; // BGRA!
        // Allocate memory.
        raw_=std::make_unique<uint8_t[]>(len_);
    }  

    native_raster::~native_raster() {

    }

    int native_raster::width() const {
        return width_;
    }

    int native_raster::height() const {
        return height_;
    }

    uint8_t* native_raster::raw() const {
        return raw_.get();
    }

    native_audio::native_audio()
    {
    }

    native_audio::~native_audio()
    {
    }

    void native_audio::play_wave_async(const wave& w)
    {
        
    }


#elif __SDL__

    void artist::draw_line(color c, pt p1, pt p2) const {
        ::SDL_SetRenderDrawColor(canvas_, c.r, c.g, c.b, c.a);
        ::SDL_RenderDrawLine(canvas_,p1.x, p1.y, p2.x, p2.y);
    }

    void artist::draw_rect(color c, rct r) const {
        SDL_Rect rdst={ r.x, r.y, r.w, r.h};
        ::SDL_SetRenderDrawColor(canvas_, c.r, c.g, c.b, c.a);
        ::SDL_RenderDrawRect(canvas_,&rdst);
    }

    void artist::fill_rect(color c, rct r) const {
        SDL_Rect rdst={ r.x, r.y, r.w, r.h};
        ::SDL_SetRenderDrawColor(canvas_, c.r, c.g, c.b, c.a);
        ::SDL_RenderFillRect(canvas_,&rdst);
    }

    void artist::draw_raster(const raster& rst, pt p) const {
        
        // Create a surface.
        SDL_Surface *surface=SDL_CreateRGBSurfaceFrom(
            rst.raw(),
            rst.width(),
            rst.height(),
            32,
            4*rst.width(),
            0,0,0,0
        );

        // Get surface to texture.
        SDL_Texture *texture = SDL_CreateTextureFromSurface(canvas_, surface);

        // Draw on window.
        SDL_Rect rsrc={ 0, 0, rst.width(), rst.height() };
        SDL_Rect rdst={ p.x, p.y, rst.width(), rst.height()};
        SDL_RenderCopy( 
            canvas_, 
            texture, 
            &rsrc, 
            &rdst
        );

        // And free surface and texture.
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
    native_app_wnd::native_app_wnd(
        app_wnd *window,
        std::string title,
        size size
    ) : native_wnd(window) {
        /* Create window. In screen coordinates. */
        winst_ = ::SDL_CreateWindow(title.c_str(), 
            SDL_WINDOWPOS_UNDEFINED, 
            SDL_WINDOWPOS_UNDEFINED, 
            size.w, 
            size.h, 
            SDL_WINDOW_HIDDEN);

        // Store window to window list.
        wmap_.insert(std::pair<SDL_Window*,native_wnd*>(winst_, this));

        // Get window surface.
        wrenderer_=::SDL_CreateRenderer( winst_, -1, SDL_RENDERER_ACCELERATED);
    }

    native_app_wnd::~native_app_wnd() {
        ::SDL_DestroyRenderer(wrenderer_);
    }

    void native_app_wnd::show() const { 
        ::SDL_ShowWindow(winst_);
    }
    // Static variable.
    std::map<SDL_Window*,native_wnd*> native_wnd::wmap_;

    native_wnd::native_wnd(wnd *window) {
        window_=window;
    }

    native_wnd::~native_wnd() {
        // And lazy destroy. 
        ::SDL_DestroyWindow(winst_);
    }

    void native_wnd::destroy() {
        // Remove me from windows map.
        wmap_.erase (winst_); 
    }

    void native_wnd::repaint() {
        // TODO: Whatever.
    }

    void native_wnd::set_title(std::string s) {
        ::SDL_SetWindowTitle(winst_, s.c_str());
    };
    
    std::string native_wnd::get_title() {
        return SDL_GetWindowTitle(winst_);
    }

    size native_wnd::get_wsize() {
        int w,h;
        ::SDL_GetWindowSize(winst_, &w, &h);
        return { w, h };
    }

    void native_wnd::set_wsize(size sz) {
        // TODO: Check SDL_GetRendererOutputSize
        ::SDL_SetWindowSize(winst_, sz.w, sz.h);
    }

    pt native_wnd::get_location() {
        int x,y;
        SDL_GetWindowPosition(winst_, &x, &y);
        return { x, y };
    }

    void native_wnd::set_location(pt location) {
        SDL_SetWindowPosition(winst_,location.x, location.y);
    }

    rct native_wnd::get_paint_area() {
        int w,h;
        ::SDL_GetWindowSize(winst_, &w, &h);
        return { 0,0,w,h };
    }

    // TODO:for now SDL only has one window so we're
    // assuming the first entry in the map, but we're
    // ready for more!
    bool native_wnd::global_wnd_proc(const SDL_Event& e) {
        if (wmap_.size()==0) return false;
        native_wnd* nw = wmap_.begin()->second;
        return nw->local_wnd_proc(e);
    }

    // Local (per window) window proc.
    bool native_wnd::local_wnd_proc(const SDL_Event& e) {
        bool quit=false;
        switch(e.type) {
            case SDL_WINDOWEVENT:
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_EXPOSED:
                    {
                        // Paint event.
                        artist a(wrenderer_);
                        window_->paint.emit(a);
                        ::SDL_RenderPresent( wrenderer_ );
                    }
                    break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    {
                        // Resize event.
                        int width = e.window.data1;
                        int height = e.window.data2;
                        window_->resized.emit({width, height});
                    }
                    break;
                    case SDL_WINDOWEVENT_CLOSE:
                        // Destroy, I guess...
                        quit=true;
                        break;
                }
            break;
            case SDL_MOUSEBUTTONDOWN:
            break;
            case SDL_MOUSEBUTTONUP:
            break;
            case SDL_MOUSEMOTION:
            {
                mouse_info mi = {
                    {e.motion.x, e.motion.y}, // point
                    (bool)(e.motion.state & SDL_BUTTON_LMASK),
                    (bool)(e.motion.state & SDL_BUTTON_MMASK),
                    (bool)(e.motion.state & SDL_BUTTON_RMASK),
                    false, // TODO: Shift and Ctrl status.
                    false
                };
            }
            break;
        }
        return quit;
    }
    app_id app::id() {
        return ::getpid();
    }

    bool app::is_primary_instance() {
        // Are we already primary instance? If not, try to become one.
        if (!primary_) {
            std::string aname = app::name();

            // Pid file needs to go to /var/run
            std::ostringstream pfname, pid;
            pfname << "/tmp/" << aname << ".pid";
            pid << nice::app::id() << std::endl;

            // Open, lock, and forget. Let the OS close and unlock.
            int pfd = ::open(pfname.str().c_str(), O_CREAT | O_RDWR, 0666);
            int rc = ::flock(pfd, LOCK_EX | LOCK_NB);
            primary_ = !(rc && EWOULDBLOCK == errno);
            if (primary_) {
                // Write our process id into the file.
                ::write(pfd, pid.str().c_str(), pid.str().length());
                return false;
            }
        }
        return primary_;
    }

    void app::run(const app_wnd& w) {

        // Show the main window.
        auto& main_wnd=const_cast<app_wnd &>(w);
        main_wnd.show();

        // Main event loop.
        SDL_Event e;
        /* Clean the queue */
        bool quit=false;
        while (!quit) { 
            // Wait for something to happen.
            SDL_WaitEvent(&e);
            quit = native_wnd::global_wnd_proc(e);
        }
    }
    native_raster::native_raster(int width, int height, const uint8_t *bgra) :
        native_raster(width,height) {
        // Copy complete BGRA array.
        std::copy(bgra, bgra+len_, raw_.get());
    }
    
    native_raster::native_raster(int width, int height) :
        width_(width), 
        height_(height) {

        // Calculate raster length.
        len_ = width * height * 4; // BGRA!
        // Allocate memory.
        raw_=std::make_unique<uint8_t[]>(len_);
    }  

    native_raster::~native_raster() {}

    int native_raster::width() const {
        return width_;
    }

    int native_raster::height() const {
        return height_;
    }

    uint8_t* native_raster::raw() const {
        return raw_.get();
    }

    native_audio::native_audio()
    {
    }

    native_audio::~native_audio()
    {
    }

    void native_audio::play_wave_async(const wave& w)
    {
        // Make SDL_RWops pointer from our wave memory
        // so that it can be treated as a stream. 
        SDL_RWops *rw_ops = SDL_RWFromMem(w.raw(), w.len());
        if (rw_ops == nullptr)
            return; // Something went wrong. 
        // We'll map wave to SDL_AudioSpec struct. 
        SDL_AudioSpec wav_spec;
        Uint32 wav_length;
        Uint8 *wav_buffer;
        // Now load these structures with data from wave. 
        // 1 means rw_ops will be released after this
        if (!SDL_LoadWAV_RW(rw_ops, 1, &wav_spec, &wav_buffer, &wav_length))
            return;
        // Grab our default audio device.
        SDL_AudioDeviceID did = SDL_OpenAudioDevice(
            NULL,
            0,
            &wav_spec,
            NULL, // Not interested in the changes you've made to play. 
            SDL_AUDIO_ALLOW_ANY_CHANGE);

        int success = SDL_QueueAudio(did, wav_buffer, wav_length);
        SDL_PauseAudioDevice(did, 0);

        // Sync.
        SDL_Delay(1000 * w.duration_in_seconds());

        // And now ...
        SDL_CloseAudioDevice(did);
        SDL_FreeWAV(wav_buffer);
    }


#endif

}

#ifdef __WIN__
extern void program();

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    // Store cmd line arguments.
    nice::app::argc = __argc;
    nice::app::argv = __argv;

    // Store application instance.
    nice::app::instance(hInstance);

    // Try becoming primary instance...
    nice::app::is_primary_instance();
    
    // Run program.
    program();
    
    // And return return code;
    return nice::app::ret_code;
}

#elif __X11__
extern void program();

int main(int argc, char* argv[]) {
    // X Windows initialization code.
    nice::app_instance inst;
    inst.display=::XOpenDisplay(NULL);
    nice::app::instance(inst);

    // Copy cmd line arguments.
    nice::app::argc = argc;
    nice::app::argv = argv;

    // Try becoming primary instance...
    nice::app::is_primary_instance();
    
    // Run program.
    program();
    
    // Close display.
    ::XCloseDisplay(inst.display);

    // And return return code;
    return nice::app::ret_code;
}

#elif __SDL__
extern void program();

int main(int argc, char* argv[]) {

    // Init SDL.
    if (::SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0) {
        throw_ex(nice::nice_exception,"SDL could not initialize.");
    }

    // TODO: Initialize application.
    nice::app_instance inst=0;
    nice::app::instance(inst);

    // Copy cmd line arguments.
    nice::app::argc = argc;
    nice::app::argv = argv;

    // Try becoming primary instance...
    nice::app::is_primary_instance();
    
    // Run program.
    program();
    
    // Exit SDL.
    ::SDL_Quit();

    // And return return code;
    return nice::app::ret_code;
}

#endif

#endif // _NICE_HPP

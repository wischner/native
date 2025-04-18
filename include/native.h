#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <utility>
#include <cstdint>

extern int program(int argc, char **argv);

namespace native
{

    // --- Geometry. -------------------------------------------------
    using coord = int;
    using rgba = uint32_t;

    struct point
    {
        coord x = 0;
        coord y = 0;

        point() = default;
        point(coord x_, coord y_);
    };

    struct line
    {
        point a;
        point b;

        line() = default;
        line(point p1, point p2);
        line(coord x1, coord y1, coord x2, coord y2);

        bool contains(point pt) const;
    };

    struct size
    {
        coord w = 0;
        coord h = 0;

        size() = default;
        size(coord w_, coord h_);
    };

    struct rect
    {
        point p;
        size d;

        rect() = default;
        rect(point p_, size d_);
        rect(coord x, coord y, coord w, coord h);

        coord x1() const;
        coord y1() const;
        coord x2() const;
        coord y2() const;

        coord width() const;
        coord height() const;

        bool contains(point pt) const;
    };

    // --- Signals. --------------------------------------------------
    template <typename... Args>
    class signal
    {
    public:
        signal() : current_id(0), initialized(false), initializer(nullptr) {}

        explicit signal(std::function<void()> init)
            : current_id(0), initialized(false), initializer(std::move(init)) {}

        int connect(const std::function<bool(Args...)> &slot) const
        {
            ensure_init();
            slots[++current_id] = slot;
            return current_id;
        }

        template <typename T>
        int connect(T *instance, bool (T::*method)(Args...))
        {
            return connect([=](Args... args)
                           { return (instance->*method)(std::forward<Args>(args)...); });
        }

        template <typename T>
        int connect(const T *instance, bool (T::*method)(Args...) const)
        {
            return connect([=](Args... args)
                           { return (instance->*method)(std::forward<Args>(args)...); });
        }

        void disconnect(int id) const
        {
            slots.erase(id);
        }

        void disconnect_all() const
        {
            slots.clear();
        }

        void emit(Args... args)
        {
            ensure_init();
            for (auto it = slots.rbegin(); it != slots.rend(); ++it)
            {
                if (it->second(std::forward<Args>(args)...))
                    break;
            }
        }

    private:
        void ensure_init() const
        {
            if (!initialized && initializer)
            {
                initializer();
                initialized = true;
            }
        }

        mutable std::map<int, std::function<bool(Args...)>> slots;
        mutable int current_id;
        mutable bool initialized;
        std::function<void()> initializer;
    };

    // --- Events. ---------------------------------------------------
    enum class mouse_button
    {
        none = 0,
        left,
        right,
        middle,
        x1,
        x2
    };

    struct mouse_event
    {
        mouse_button button;
        point position;

        mouse_event() = default;
        mouse_event(mouse_button b, point pos)
            : button(b), position(pos) {}
    };

    enum class wheel_direction
    {
        vertical,
        horizontal
    };

    struct mouse_wheel_event
    {
        point position; // Optional, where mouse is during scroll
        coord delta;    // Positive = up/right, negative = down/left
        wheel_direction direction;

        mouse_wheel_event() = default;
        mouse_wheel_event(point pos, coord d, wheel_direction dir)
            : position(pos), delta(d), direction(dir) {}
    };

    // --- Graphics --------------------------------------------------
    class pen
    {
    public:
        pen(rgba color, coord thickness);
        ~pen();

        pen(const pen &) = delete;
        pen &operator=(const pen &) = delete;

        pen(pen &&) noexcept;
        pen &operator=(pen &&) noexcept;

        rgba color() const;
        coord thickness() const;

    private:
        rgba _color;
        coord _thickness;
    };

    class img; // forward declare

    class gpx
    {
    public:
        virtual ~gpx() = default;

        virtual void set_pen(const pen &p) = 0;
        virtual pen get_pen() const = 0;

        virtual void clear(rgba color) = 0;
        virtual void draw_line(point from, point to) = 0;
        virtual void draw_rect(rect r, bool filled = false) = 0;
        virtual void draw_text(const std::string &text, point p) = 0;
        virtual void draw_img(const img &src, point dst) = 0;

        virtual void set_clip(const rect &r) = 0;
        virtual rect get_clip() const = 0;
    };

    // --- Image. ----------------------------------------------------
    class img
    {
    public:
        img(coord w, coord h);
        ~img();

        coord width() const { return _w; }
        coord height() const { return _h; }

        rgba *pixels() { return _data; }
        const rgba *pixels() const { return _data; }

        gpx &get_gpx() const;

    private:
        coord _w, _h;
        rgba *_data;
        mutable gpx *_gpx = nullptr;
    };

    // --- Screen. ---------------------------------------------------
    class screen final
    {
    public:
        screen(int index, const rect &bounds, const rect &work_area, bool is_primary);

        int index() const;
        bool is_primary() const;
        const rect &bounds() const;
        const rect &work_area() const;

        static const std::vector<screen> &detect();
        static int count();
        static screen *at(int index);
        static screen *primary();
        static rect virtual_bounds();

    private:
        int _index;
        rect _bounds;
        rect _work_area;
        bool _is_primary;

        static std::vector<screen> _screens;
    };

    // --- Application. ----------------------------------------------
    class app_wnd;
    class app final
    {
    public:
        app() = delete;

        static int run(const app_wnd &wnd);
        static int main_loop();

        static app_wnd *main_wnd(); // Expose current main window

        // Static arguments and environment
        static inline int argc = 0;
        static inline char **argv = nullptr;
        static inline char **envp = nullptr;

    private:
        static inline app_wnd *_main_wnd = nullptr;
    };

    // --- Windows. --------------------------------------------------
    class wnd
    {
    public:
        wnd(int x = 100, int y = 100, int width = 640, int height = 480);
        virtual ~wnd() = default;

        int x() const;
        int y() const;
        int width() const;
        int height() const;

        virtual void create() const = 0;
        virtual void show() const = 0;

        gpx &get_gpx() const; // New method to obtain gpx for this window

        signal<> on_wnd_create;
        signal<point> on_wnd_move;
        signal<size> on_wnd_resize;

        signal<point> on_mouse_move;
        signal<mouse_event> on_mouse_click;
        signal<mouse_wheel_event> on_mouse_wheel;

    protected:
        int _x, _y, _width, _height;
    };

    class app_wnd : public wnd
    {
    public:
        app_wnd(std::string title,
                int x = 100, int y = 100,
                int width = 640, int height = 480);

        virtual ~app_wnd() = default;

        const std::string &title() const;

        virtual void create() const override;
        virtual void show() const override;

    private:
        std::string _title;
    };

}
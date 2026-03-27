#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <utility>
#include <cstdint>

extern int program(int argc, char **argv);

namespace native
{

    // --- Geometry. -------------------------------------------------
    using coord = int16_t;
    using dim = uint16_t;

    union rgba
    {
        uint32_t value;
        struct
        {
            /* TODO: endian handling */
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };

        constexpr rgba() : value(0) {}
        constexpr rgba(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
            : r(red), g(green), b(blue), a(alpha) {}

        constexpr rgba(uint32_t val) : value(val) {}

        operator uint32_t() const { return value; }
    };

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
        dim w = 0;
        dim h = 0;

        size() = default;
        size(dim w_, dim h_);
    };

    struct rect
    {
        point p;
        size d;

        rect() = default;
        rect(point p_, size d_);
        rect(coord x, coord y, dim w, dim h);

        coord x1() const;
        coord y1() const;
        coord x2() const;
        coord y2() const;

        dim w() const;
        dim h() const;

        bool contains(point pt) const;
        rect intersect(const rect &other) const;
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

    enum class mouse_action
    {
        press,
        release
    };

    struct mouse_event
    {
        mouse_button button;
        mouse_action action;
        point position;

        mouse_event() = default;
        mouse_event(mouse_button b, mouse_action a, point pos)
            : button(b), action(a), position(pos) {}
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

    // --- Fonts. ----------------------------------------------------
    enum class font_role
    {
        system,   // General UI controls and text
        fixed,    // Monospace / terminal / code
        title,    // Window title bars
        small_,   // Icon labels and small annotations
        control   // Menus and buttons
    };

    struct font_spec
    {
        std::string name;   // Family name, file path, or native spec (XLFD for X11)
        int size = 0;       // Point size; 0 = platform default for that role
        bool bold = false;
        bool italic = false;
    };

    class font_t
    {
    public:
        font_t();
        ~font_t();

        font_t(font_t &&other) noexcept;
        font_t &operator=(font_t &&other) noexcept;

        font_t(const font_t &) = delete;
        font_t &operator=(const font_t &) = delete;

        bool valid() const { return _id != 0; }
        const font_spec &spec() const { return _spec; }

        // Opaque identifier used internally to look up platform bindings.
        // Stable across moves. Not meaningful to library users.
        uint32_t id() const { return _id; }

        // Create a font from a cross-platform description.
        // Platforms that support TTF accept a family name or file path.
        // X11-based toolkits also accept XLFD strings.
        static font_t create(const font_spec &spec);

        // Returns a stock platform font for the given role.
        // Fonts are sourced from system APIs where possible — not hard-coded.
        // The returned reference is valid for the lifetime of the application.
        static const font_t &stock(font_role role);

    private:
        uint32_t _id = 0;   // 0 = not registered; stable key into platform font_bindings
        font_spec _spec;
    };

    // --- Image. ----------------------------------------------------
    class gpx; // forward declare
    class img
    {
    public:
        img(dim w, dim h);
        ~img();

        coord w() const { return _w; }
        coord h() const { return _h; }

        rgba *pixels() { return _data.get(); }
        const rgba *pixels() const { return _data.get(); }

        gpx &get_gpx() const;

    private:
        coord _w, _h;
        std::unique_ptr<rgba[]> _data;
        mutable std::unique_ptr<gpx> _gpx;
    };

    // --- Graphics --------------------------------------------------
    class gpx
    {
    public:
        virtual ~gpx() = default;

        gpx &set_ink(const rgba c);
        rgba ink() const;

        gpx &set_paper(const rgba c);
        rgba paper() const;

        gpx &set_pen(const uint8_t thickness);
        uint8_t pen() const;

        gpx &set_font(const font_t &f);
        const font_t &font() const;  // returns set font, or stock(system) if none set

        virtual gpx &set_clip(const rect &r) = 0;
        virtual rect clip() const = 0;

        virtual gpx &clear(rgba color) = 0;
        virtual gpx &draw_line(point from, point to) = 0;
        virtual gpx &draw_rect(rect r, bool filled = false) = 0;
        virtual gpx &draw_text(const std::string &text, point p) = 0;
        virtual gpx &draw_img(const img &src, point dst) = 0;

    protected:
        rgba _ink    = rgba(0, 0, 0, 255);      // black
        rgba _paper  = rgba(255, 255, 255, 255); // white
        uint8_t _thickness = 1;
        const font_t *_font = nullptr;           // non-owning; nullptr = use stock system
    };

    // --- Screen. ---------------------------------------------------
    class screen final
    {
    public:
        screen(int index, const rect &bounds, const rect &work_area, bool is_primary);

        int index() const;
        bool is_primary() const;
        bool is_landscape() const;
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
    class app_wnd; // forward declaration for menu
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

    // --- Events. ---------------------------------------------------
    struct wnd_paint_event
    {
        rect r;
        gpx &g;

        wnd_paint_event(const rect &rect, gpx &gpx)
            : r(rect), g(gpx) {}
    };

    // --- Menu. -----------------------------------------------------
    class menu_items_proxy
    {
    public:
        struct entry { int id = 0; std::string label; };
        std::vector<entry> entries;

        menu_items_proxy &operator<<(const std::string &label)
        {
            entries.push_back({0, label});
            return *this;
        }
        menu_items_proxy &operator<<(std::pair<int, std::string> kv)
        {
            entries.push_back({kv.first, std::move(kv.second)});
            return *this;
        }
    };

    inline menu_items_proxy menu_items(const std::string &first)
    {
        menu_items_proxy p;
        p << first;
        return p;
    }

    class main_menu
    {
    public:
        main_menu() = default;
        ~main_menu();
        main_menu(const main_menu &) = delete;
        main_menu &operator=(const main_menu &) = delete;

        // Called by app_wnd::create() immediately after _created = true
        void attach(app_wnd &owner);

        // --- DSL builder ---
        class builder
        {
        public:
            explicit builder(main_menu &m) : _menu(m) {}
            builder &operator<<(const std::string &top_title)
            {
                _menu.add_top(top_title);
                return *this;
            }
            builder &operator<<(const menu_items_proxy &proxy)
            {
                for (const auto &e : proxy.entries)
                    _menu.add_item(e.id, e.label);
                return *this;
            }
        private:
            main_menu &_menu;
        };

        builder operator<<(const std::string &top_title)
        {
            add_top(top_title);
            return builder(*this);
        }

        uint32_t id() const { return _id; }

        // Expose internal structure for platform implementations.
        struct menu_entry { int id = 0; std::string label; };
        struct top_entry  { std::string title; std::vector<menu_entry> items; };
        const std::vector<top_entry> &tops() const { return _tops; }

    private:
        std::vector<top_entry> _tops;
        app_wnd *_owner = nullptr;
        uint32_t _id    = 0;

        void add_top(const std::string &title)   { _tops.push_back({title, {}}); }
        int next_auto_item_id() const
        {
            // Keep auto-generated IDs in a high range to avoid collisions with
            // common hand-authored command IDs like 1, 2, 100.
            int candidate = 10000;
            while (true)
            {
                bool used = false;
                for (const auto &top : _tops)
                {
                    for (const auto &item : top.items)
                    {
                        if (item.id == candidate)
                        {
                            used = true;
                            break;
                        }
                    }
                    if (used)
                        break;
                }
                if (!used)
                    return candidate;
                ++candidate;
            }
        }
        void add_item(int id, const std::string &label)
        {
            if (id == 0)
                id = next_auto_item_id();
            if (!_tops.empty())
                _tops.back().items.push_back({id, label});
        }

        friend class builder;
    };

    // --- Windows. --------------------------------------------------
    class layout_manager;
    class wnd
    {
    public:
        wnd(coord x = 100, coord y = 100, dim w = 640, dim h = 480);
        wnd(const point &pos, const size &dim);
        wnd(const rect &bounds);

        virtual ~wnd();

        virtual point position() const;
        virtual wnd &set_position(const point &p);

        virtual size dimensions() const;
        virtual wnd &set_dimensions(const size &s);

        virtual rect bounds() const;
        virtual wnd &set_bounds(const rect &r);

        virtual wnd *parent() const;
        virtual wnd &set_parent(wnd *parent);

        virtual wnd &invalidate() const;
        virtual wnd &invalidate(const rect &r) const;

        virtual void show() const = 0;
        virtual void create() const = 0;
        virtual void destroy() const = 0;

        // Layout
        void set_layout(std::unique_ptr<layout_manager> layout);
        layout_manager *layout() const;

        gpx &get_gpx() const;

        signal<> on_wnd_create;
        signal<point> on_wnd_move;
        signal<size> on_wnd_resize;
        signal<wnd_paint_event> on_wnd_paint;

        signal<point> on_mouse_move;
        signal<mouse_event> on_mouse_click;
        signal<mouse_wheel_event> on_mouse_wheel;

    protected:

        // Utility function to

        // Has create() been called?
        mutable bool _created;

        rect _bounds;
        std::unique_ptr<layout_manager> _layout;
        mutable gpx *_gpx = nullptr;
        wnd *_parent;
        std::vector<wnd *> _children;
    };

    class app_wnd : public wnd
    {
    public:
        app_wnd(std::string title,
                coord x = 100, coord y = 100,
                dim w = 640, dim h = 480);

        app_wnd(const std::string &title, const point &pos, const size &dim);
        app_wnd(const std::string &title, const rect &bounds);

        virtual ~app_wnd() = default;

        const std::string &title() const;
        app_wnd &set_title(const std::string &title);

        virtual void create() const override;
        virtual void destroy() const override;
        virtual void show() const override;

        main_menu menu;
        signal<int> on_menu;

    private:
        std::string _title;
    };

    // --- Layout manager. -------------------------------------------
    class layout_manager
    {
    public:
        virtual ~layout_manager() = default;

        // Called by container when laying out children
        virtual void relayout(wnd *parent, const rect &bounds) = 0;

        // Add/remove children
        virtual void add_child(wnd *child) = 0;
        virtual void remove_child(wnd *child) = 0;

        // Access child list (optional)
        virtual const std::vector<wnd *> &children() const = 0;
    };

}

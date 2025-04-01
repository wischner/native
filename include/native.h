#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <utility>

extern int program(int argc, char **argv);

namespace native
{
    template <typename... Args>
    class signal
    {
    public:
        signal();
        explicit signal(std::function<void()> init);

        int connect(const std::function<bool(Args...)> &slot) const;

        template <typename T>
        int connect(T *instance, bool (T::*method)(Args...));

        template <typename T>
        int connect(const T *instance, bool (T::*method)(Args...) const);

        void disconnect(int id) const;
        void disconnect_all() const;
        void emit(Args... args);

    private:
        void ensure_init() const;

        mutable std::map<int, std::function<bool(Args...)>> slots;
        mutable int current_id;
        mutable bool initialized;
        std::function<void()> initializer;
    };

    // Inline templated member functions
    template <typename... Args>
    template <typename T>
    int signal<Args...>::connect(T *instance, bool (T::*method)(Args...))
    {
        return connect([=](Args... args)
                       { return (instance->*method)(std::forward<Args>(args)...); });
    }

    template <typename... Args>
    template <typename T>
    int signal<Args...>::connect(const T *instance, bool (T::*method)(Args...) const)
    {
        return connect([=](Args... args)
                       { return (instance->*method)(std::forward<Args>(args)...); });
    }

    class app_wnd;

    class app final
    {
    public:
        app() = delete; // disallow instantiation

        static int run(const app_wnd &wnd);
        static int main_loop();
    };

    class wnd
    {
    public:
        wnd(int x = 100, int y = 100, int width = 640, int height = 480);
        virtual ~wnd() = default;

        int x() const;
        int y() const;
        int width() const;
        int height() const;

        void create() const;
        void show() const;

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

    private:
        std::string _title;
    };
}
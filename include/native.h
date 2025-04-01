#pragma once
#include <string>
#include <vector>

extern int program(int argc, char **argv);

namespace native
{
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
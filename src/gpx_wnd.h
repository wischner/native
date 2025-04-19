#include <native.h>

namespace native
{

    class gpx_wnd : public gpx
    {
    public:
        gpx_wnd(const wnd *wnd);
        ~gpx_wnd() override;

        void set_pen(const pen &p) override;
        pen get_pen() const override;

        void set_clip(const rect &r) override;
        rect get_clip() const override;

        void clear(rgba color) override;
        void draw_line(point from, point to) override;
        void draw_rect(rect r, bool filled = false) override;
        void draw_text(const std::string &text, point p) override;
        void draw_img(const img &src, point dst) override;

    private:
        wnd *_wnd;
        pen _pen;
        rect _clip;
    };

}
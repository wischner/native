#include <native.h>

namespace native
{

    class gpx_wnd : public gpx
    {
    public:
        gpx_wnd(const wnd *wnd, point offset = {0, 0});
        virtual ~gpx_wnd();

        gpx &set_clip(const rect &r) override;
        rect clip() const override;

        gpx &clear(rgba color) override;
        gpx &draw_line(point from, point to) override;
        gpx &draw_rect(rect r, bool filled = false) override;
        gpx &draw_text(const std::string &text, point p) override;
        gpx &draw_img(const img &src, point dst) override;

    private:
        wnd *_wnd;
        rect _clip;
        point _offset;
    };

}
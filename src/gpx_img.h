#pragma once

#include <native.h>

namespace native
{

    class gpx_img : public gpx
    {
    public:
        explicit gpx_img(const img &image); // Guaranteed non-null
        ~gpx_img() override = default;

        gpx &set_clip(const rect &r) override;
        rect clip() const override;

        // Drawing primitives
        gpx &clear(rgba color) override;
        gpx &draw_line(point from, point to) override;
        gpx &draw_rect(rect r, bool filled = false) override;
        gpx &draw_text(const std::string &text, point p) override;
        gpx &draw_img(const img &src, point dst) override;

    private:
        const img &_img; // Non-null reference to parent image
        rect _clip;
    };

} // namespace native

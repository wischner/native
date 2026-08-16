//
// Declares reusable control composition for toolkits without a native
// painter. Concrete toolkit themes supply palette and text metrics.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native.h>

namespace linux
{
    class emulated_theme : public native::theme
    {
    public:
        explicit emulated_theme(native::gpx &graphics);

        theme &draw_button(const native::rect &bounds,
                           const std::string &text,
                           const state &element_state) override;

        theme &draw_menu_bar(const native::rect &bounds) override;

        theme &draw_menu_title(const native::rect &bounds,
                               const std::string &text,
                               const state &element_state) override;

        theme &draw_menu_item(const native::rect &bounds,
                              const std::string &text,
                              const state &element_state) override;

        theme &draw_popup_frame(const native::rect &bounds) override;

        theme &draw_list_item(const native::rect &bounds,
                              const std::string &text,
                              const state &element_state) override;

        theme &draw_check(const native::rect &bounds,
                          const std::string &text,
                          const state &element_state) override;

        theme &draw_radio(const native::rect &bounds,
                          const std::string &text,
                          const state &element_state) override;

        theme &draw_list(const native::rect &bounds,
                         const std::vector<std::string> &items,
                         int selected_index,
                         const state &element_state) override;

    protected:
        virtual int text_width(const std::string &text) const = 0;
        virtual int text_height() const = 0;
        virtual bool text_uses_baseline() const = 0;

        int text_y(const native::rect &bounds) const;

    private:
        class saved_state
        {
        public:
            explicit saved_state(native::gpx &graphics);
            ~saved_state();

        private:
            native::gpx &_graphics;
            native::rgba _ink;
            native::rgba _paper;
            std::uint8_t _pen;
            const native::font_t &_font;
            native::rect _clip;
        };

        void draw_bevel(const native::rect &bounds,
                        bool inset,
                        const palette &colors);

        void draw_indicator_box(const native::rect &bounds,
                                bool inset,
                                const palette &colors);

        void draw_control_label(const native::rect &bounds,
                                int x,
                                const std::string &text,
                                const state &element_state,
                                const palette &colors);

        theme &draw_menu_entry(const native::rect &bounds,
                               const std::string &text,
                               const state &element_state,
                               bool title);
    };
} // namespace linux

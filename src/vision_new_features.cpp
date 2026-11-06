//
// Exercises resources, composed canvas text, mouse annotations, hosted
// toolbar controls, ruler gestures and em fonts using only public APIs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "vision_new_features.h"
#include <native.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
    using namespace native;
    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }
    // Deliberately distinct, pixel-aligned color and monochrome artwork.
    std::vector<std::uint8_t> icon_bytes(bool mono) {
        img image(16, 16);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const bool mark = x == 3 || x == 12 || x == y;
                image.pixels()[y * 16 + x] = mark
                    ? rgba(0, 0, 0, 255)
                    : mono ? rgba(255, 255, 255, 0)
                           : rgba(80, 170, 245, 255);
            }
        return image.encode(image_format::png);
    }
    class feature_window final : public app_wnd
    {
    public:
        feature_window(const std::filesystem::path &font_path, bool smoke)
            : app_wnd("Native feature acceptance", 40, 40, 760, 480),
              _bar(*this, window_edge::top, 34),
              _ruler(_canvas, window_edge::top, 28), _status(*this),
              _line_font(font_t::from_file(font_path, 24)),
              _em_font(font_t::from_file(font_path, 24, 0, font_size_basis::em)),
              _smoke(smoke) {
            _canvas.set_parent(this);
            _choice.set_selected_index(0);
            _canvas.set_horizontal_scrollbar_policy(scrollbar_policy::never);
            _canvas.set_vertical_scrollbar_policy(scrollbar_policy::never);
            resources source;
            source.add({"assets.icons.platform-icon", "icon", {}, icon_bytes(false)});
            source.add({"assets.icons.platform-icon", "icon",
                        {"", "", "monochrome", "", 16}, icon_bytes(true)});
            _resources = resources::decode(source.encode());
            const auto &entry = _resources.get("assets.icons.platform-icon");
            _icon.reset(new img(img::decode(entry.data.data(), entry.data.size())));
            const auto context = resources::current_context();
            _profile = context.platform + "/" + context.toolkit + ": " +
                context.color + "; selected " +
                (entry.when.color.empty() ? "default color" : entry.when.color);
            _profile += app::get_unicode_text_input_supported()
                ? "; Unicode keyboard text" : "; ASCII keyboard text";
            _ruler.set_units_per_pixel(0.01).set_minor_tick(0.1)
                .set_major_tick(1).set_marker_snap(0.1).set_active_range(1, 5);
            _ruler.set_markers({{1, 1, ruler_marker_shape::down_triangle, true},
                               {2, 2, ruler_marker_shape::up_triangle, true},
                               {3, 5, ruler_marker_shape::block, false},
                               {4, 3, ruler_marker_shape::tab, true}});
            menu_items_proxy actions;
            actions << std::pair<int, std::string>{1, "&Focus canvas\tCtrl+F"}
                << std::pair<int, std::string>{2, "Toggle &hosted controls"}
                << std::pair<int, std::string>{3, "&Clear text"}
                << std::pair<int, std::string>{4, "&Quit"};
            menu << "&Test" << actions;
            on_menu.connect([this](int id) {
                if (id == 1) _canvas.focus();
                if (id == 2) configure_toolbar(!_hosted);
                if (id == 3) { _text.clear(); _canvas.invalidate(); }
                if (id == 4) destroy();
                return true;
            });
            on_wnd_create.connect([this] {
                configure_toolbar(true);
                _canvas.create();
                _canvas.set_bounds(get_client_bounds());
                _canvas.show();
                _canvas.focus();
                report("Type here, Shift-click, double-click, drag a ruler marker.");
                if (_smoke) app::post([this] { check(); });
                return true;
            });
            _canvas.on_wnd_paint.connect([this](wnd_paint_event e) {
                paint(e); return true;
            });
            _canvas.on_text_input.connect([this](std::string text) {
                if (_text.size() + text.size() <= 2048) _text += text;
                _canvas.invalidate();
                return true;
            });
            _canvas.on_key.connect([this](key_event e) {
                if (e.key == key_code::escape && e.action == key_action::press) {
                    _text.clear(); _canvas.invalidate(); return true;
                }
                return false;
            });
            _canvas.on_mouse_click.connect([this](mouse_event e) {
                report("Clicks=" + std::to_string(e.clicks) +
                    " modifiers=" + std::to_string(unsigned(e.modifiers)));
                return true;
            });
            _ruler.on_marker_move.connect([this](ruler_marker_move e) {
                report("Marker " + std::to_string(e.id) + " = " +
                    std::to_string(e.value) + (e.finished ? " released" : " dragging"));
                return true;
            });
            _choice.on_selection_change.connect([this](int index) {
                report("Hosted choice: " + std::to_string(index));
                return true;
            });
            _bar.on_command.connect([this](tool_command) {
                _canvas.focus(); return true;
            });
        }
        // Tear down native children while their callback state is alive.
        ~feature_window() override { destroy(); }

        // Return smoke assertion status after the event loop ends.
        bool failed() const { return _failed; }
    protected:
        // Keep the canvas clear of all top-level chrome when resized.
        void on_bounds_changed() override {
            app_wnd::on_bounds_changed();
            if (_canvas.get_created()) _canvas.set_bounds(get_client_bounds());
        }
    private:
        // Mirror feedback on the canvas so it remains visible on small desktops.
        void report(const std::string &message) {
            _status.set_text(message);
            if (_canvas.get_created()) _canvas.invalidate();
        }
        void configure_toolbar(bool hosted) {
            tool_item icon;
            icon.id = "focus"; icon.icon = _icon;
            std::vector<tool_item> items{icon};
            if (hosted) {
                tool_item choice;
                choice.id = "choice"; choice.kind = tool_kind::control;
                choice.control = &_choice; choice.length = 160;
                items.push_back(choice);
            }
            _bar.set_items(std::move(items));
            _hosted = hosted;
        }
        void paint(wnd_paint_event e) {
            auto saved = e.g.save_state();
            const auto area = _canvas.get_client_bounds();
            e.g.set_ink(rgba(255, 255, 255, 255)).draw_rect(area, true);
            e.g.set_ink(rgba(0, 0, 0, 255)).set_font(font_t::stock(font_role::control));
            const int x = area.x1() + 12, y = area.y1() + 12;
            auto line = [&](const std::string &text, int offset) {
                e.g.draw_text(text, rect(x, y + offset,
                    std::max(1, int(area.w()) - 24), 30),
                    {text_align::start, text_valign::center, text_overflow::ellipsis, true});
            };
            line(_profile, 0);
            line("Type composed text; Escape clears. Test menu returns focus from the combo.", 32);
            line("Drag ruler triangles/tabs; the block is fixed. Toggle hosted controls in Test.", 64);
            line("Input: " + _text, 100);
            if (_line_font.valid() && _em_font.valid()) {
                e.g.set_font(_line_font); line("24 pixel line-height: Native", 150);
                e.g.set_font(_em_font); line("24 pixel em: Native", 195);
            } else line("Font fixture unavailable: deploy retro-terminal/terminal.ttf beside Vision.", 150);
            e.g.draw_img(*_icon, rect(x, y + 245, 32, 32));
            e.g.set_font(font_t::stock(font_role::control));
            line(_status.get_text(), 290);
        }
        void check() {
            try {
                expect(_line_font.valid() && _em_font.valid(), "font fixture missing");
                // Some faces have an em exactly equal to their line box.
                // The font API regression uses other installed faces to
                // check the differing-scale case.
                expect(_line_font.measure_text("Native").advance > 0 &&
                       _em_font.measure_text("Native").advance > 0,
                       "portable font measurement failed");
                expect(_choice.get_created(), "hosted combo was not created");
                _choice.set_selected_index(1);
                configure_toolbar(false);
                expect(!_choice.get_created() && !_choice.get_parent(), "removed combo remains attached");
                configure_toolbar(true);
                expect(_choice.get_created() && _choice.get_selected_index() == 1,
                       "combo state did not survive toolbar transition");
                _canvas.focus();
                expect(_canvas.get_focused(), "canvas focus missing");
                _canvas.on_native_text_input("Native \xc5\xbe");
                expect(_text == "Native \xc5\xbe", "UTF-8 text delivery failed");
                const auto icon = _bar.get_item_bounds("focus");
                expect(icon.w() == icon.h() && icon.w() > 0, ("icon tool is not square: " + std::to_string(icon.w()) + "x" +
                       std::to_string(icon.h()) + " strip=" + std::to_string(_bar.get_bounds().w())).c_str());
                std::cout << "New feature smoke passed: " << _profile << '\n';
            } catch (const std::exception &error) {
                std::cerr << "Feature smoke: " << error.what() << '\n';
                _failed = true;
            }
            destroy();
        }
        canvas _canvas;
        combo_box _choice{{"First", "Second", "Third"}};
        toolbar _bar;
        ruler _ruler;
        status_bar _status;
        resources _resources;
        std::shared_ptr<const img> _icon;
        font_t _line_font, _em_font;
        std::string _profile, _text;
        bool _smoke, _failed = false, _hosted = false;
    };
}
namespace vision
{
    int run_new_features(int argc, char **argv) {
        try {
            bool smoke = false;
            for (int i = 1; i < argc; ++i)
                smoke |= std::string_view(argv[i]) == "--features-smoke";
            const auto font = std::filesystem::absolute(argv[0]).parent_path() /
                "retro-terminal" / "terminal.ttf";
            feature_window window(font, smoke);
            const int result = app::run(window);
            return window.failed() ? 1 : result;
        } catch (const std::exception &error) {
            std::cerr << "New features: " << error.what() << '\n';
            return 1;
        }
    }
}

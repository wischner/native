//
// Tests incremental ANSI screen state and portable terminal shader
// pixels. It validates the application fixture without requiring a
// desktop session.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "terminal_screen.h"
#include "shader_program.h"
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace
int main(int argc, char **argv) {
    try {
        vision::terminal_screen screen(8, 3);
        screen.feed("abc\rZ\n\x1b[3");
        screen.feed("1mRED\x1b[0m");
        require(screen.get_cell(0, 0).glyph == 'Z' &&
                    screen.get_cell(1, 0).glyph == 'b',
                "Carriage return erased the row.");
        require(screen.get_cell(0, 1).glyph == 'R' &&
                    screen.get_cell(0, 1).ink == 1,
                "Split SGR lost text/color.");
        screen.feed("\x1b[3;8HX\x1b[s\x1b[1;1H!\x1b[uY");
        require(screen.get_cell(0, 0).glyph == 'R' &&
                    screen.get_cell(0, 2).glyph == 'Y',
                "Wrapping did not scroll the bounded grid.");
        screen.feed("\x1b[2J\x1b[H\x1b]0;hidden title\x1b\\OK");
        require(screen.get_cell(0, 0).glyph == 'O' &&
                    screen.get_cell(1, 0).glyph == 'K' &&
                    screen.get_cell(0, 1).glyph == ' ',
                "Erase/OSC handling failed.");
        vision::terminal_screen narrow(3, 2);
        narrow.feed("\t");
        auto base = argc > 1
                        ? std::filesystem::path(argv[1])
                        : std::filesystem::path(NATIVE_TERMINAL_ASSETS);
        auto font = native::font_t::from_file(base / "terminal.ttf", 8);
        require(font.valid(), "Portable terminal font failed to load.");
        auto image = screen.render(font);
        bool ink = false;
        for (unsigned i = 0; i < unsigned(image->w()) * image->h(); ++i)
            ink |= image->pixels()[i].r != 0;
        require(ink, "Terminal raster contains no glyph pixels.");
        // Edge glyphs stay inside the padded raster used by the CRT demo.
        auto padded = screen.render(font, 32, 16);
        require(padded->w() == image->w() + 64 &&
                    padded->h() == image->h() + 32,
                "Terminal margins changed the grid extent.");
        for (unsigned y = 0; y < padded->h(); ++y) {
            for (unsigned x = 0; x < padded->w(); ++x) {
                const auto pixel = padded->pixels()[y * padded->w() + x];
                if (x < 32 || x >= padded->w() - 32 ||
                    y < 16 || y >= padded->h() - 16)
                    require(pixel.r == 0 && pixel.g == 0 && pixel.b == 0,
                            "Terminal glyphs leaked into the CRT margin.");
                else {
                    const auto original = image->pixels()[
                        (y - 16) * image->w() + x - 32];
                    require(pixel.r == original.r && pixel.g == original.g &&
                                pixel.b == original.b && pixel.a == original.a,
                            "Adding margins changed terminal content.");
                }
            }
        }
        vision::terminal_screen monitor_screen;
        monitor_screen.feed("CRT shader pixels");
        image = monitor_screen.render(font);
        auto effect =
            native::shader_package::load(base / "terminal.nshader");
        std::map<std::string, native::shader_value, std::less<>>
            parameters;
        for (const auto &parameter : effect.get_parameters())
            parameters.emplace(parameter.name, parameter.default_value);
        std::vector<native::detail::shader_frame> history;
        auto output = native::detail::execute_shader(
            native::detail::shader_package_data::get(effect), *image,
            parameters, image->w(), image->h(), 1, 0, 0, 0, history);
        require(output->w() == image->w() &&
                    output->h() == image->h() && history.size() == 8 &&
                    !history[0].pixels.empty(),
                "CRT pass extent/history failed.");
        ink = false;
        for (unsigned i = 0; i < unsigned(output->w()) * output->h();
             ++i) {
            const auto pixel = output->pixels()[i];
            require(pixel.a == 255, "Terminal output is not opaque.");
            ink |= pixel.r != 0;
        }
        require(ink, "CRT output lost the terminal glyphs.");
        // Material geometry and glass are effect output, not window
        // chrome.
        auto pixel = [&](double x, double y) {
            return output
                ->pixels()[unsigned(y * output->h()) * output->w() +
                           unsigned(x * output->w())];
        };
        require(pixel(0.5, 0.04).r > pixel(0.01, 0.01).r + 50,
                "The shader did not render a monitor bezel.");
        require(pixel(0.5, 0.08).r < pixel(0.5, 0.04).r,
                "The glass recess is not darker than the bezel.");
        const auto led = pixel(0.81, 0.889);
        require(led.g > led.r && led.g > led.b,
                "The monitor power indicator is missing.");
        require(pixel(0.5, 0.25).b > pixel(0.5, 0.65).b,
                "The glass reflection did not reach the image.");
        const auto bezel = pixel(0.5, 0.04);
        parameters["curvature"] = 1.0f;
        output = native::detail::execute_shader(
            native::detail::shader_package_data::get(effect), *image,
            parameters, image->w(), image->h(), 2, 0.02, 0.02, 0.02,
            history);
        require(pixel(0.5, 0.04) == bezel,
                "Curvature distorted the monitor housing.");
        if (argc > 2)
            output->save(argv[2]);

    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

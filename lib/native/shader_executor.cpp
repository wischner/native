//
// Executes bounded vector image shaders in linear premultiplied RGBA.
// Each tick builds new pass outputs before committing previous-frame history.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "shader_program.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace
{
    using native::detail::shader_frame;
    using native::detail::shader_vector;
    using native::detail::shader_opcode;
    constexpr std::uint64_t memory_budget = 256u * 1024u * 1024u;
    // Convert encoded straight RGB to linear before premultiplying.
    float linear(float value) {
        return value <= 0.04045f ? value / 12.92f
            : std::pow((value + 0.055f) / 1.055f, 2.4f);
    }
    std::uint8_t encoded(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        const float encoded_value = value <= 0.0031308f ? value * 12.92f
            : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        return static_cast<std::uint8_t>(std::lround(encoded_value * 255));
    }
    std::pair<unsigned, unsigned> extent(
        const native::detail::shader_pass &pass,
        unsigned sw, unsigned sh, unsigned vw, unsigned vh) {
        const unsigned w = pass.viewport ? vw : sw;
        const unsigned h = pass.viewport ? vh : sh;
        return {std::max(1u, (w + pass.reduction - 1) / pass.reduction),
                std::max(1u, (h + pass.reduction - 1) / pass.reduction)};
    }
    shader_vector sample(const shader_frame &frame, shader_vector uv, bool filtered) {
        if (frame.pixels.empty()) return {};
        const float u = std::clamp(uv[0], 0.0f, 1.0f);
        const float v = std::clamp(uv[1], 0.0f, 1.0f);
        auto pixel = [&](int x, int y) -> const shader_vector & {
            x = std::clamp(x, 0, static_cast<int>(frame.width) - 1);
            y = std::clamp(y, 0, static_cast<int>(frame.height) - 1);
            return frame.pixels[std::size_t(y) * frame.width + x];
        };
        if (!filtered) return pixel(static_cast<int>(u * frame.width),
                                   static_cast<int>(v * frame.height));
        const float x = u * frame.width - 0.5f, y = v * frame.height - 0.5f;
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const float tx = x - x0, ty = y - y0;
        shader_vector result{};
        for (unsigned channel = 0; channel < 4; ++channel)
            result[channel] =
                (pixel(x0, y0)[channel] * (1 - tx) + pixel(x0 + 1, y0)[channel] * tx) * (1 - ty) +
                (pixel(x0, y0 + 1)[channel] * (1 - tx) + pixel(x0 + 1, y0 + 1)[channel] * tx) * ty;
        return result;
    }
    // Invalid arithmetic has the defined result zero, never undefined conversion.
    float component(shader_opcode op, float a, float b, float c) {
        switch (op) {
        case shader_opcode::step: return b < a ? 0.0f : 1.0f;
        case shader_opcode::add: return a + b;
        case shader_opcode::subtract: return a - b;
        case shader_opcode::multiply: return a * b;
        case shader_opcode::divide: return b == 0 ? 0 : a / b;
        case shader_opcode::minimum: return std::min(a, b);
        case shader_opcode::maximum: return std::max(a, b);
        case shader_opcode::power: return std::pow(a, b);
        case shader_opcode::sine: return std::sin(a);
        case shader_opcode::cosine: return std::cos(a);
        case shader_opcode::exponential: return std::exp(a);
        case shader_opcode::floor: return std::floor(a);
        case shader_opcode::absolute: return std::abs(a);
        case shader_opcode::square_root: return a < 0 ? 0 : std::sqrt(a);
        case shader_opcode::saturate: return std::clamp(a, 0.0f, 1.0f);
        case shader_opcode::mix: return a * (1 - c) + b * c;
        default: return 0;
        }
    }
}
namespace native::detail
{
    shader_vector shader_components(const shader_value &value) {
        shader_vector result{};
        std::visit([&](const auto &v) {
            using type = std::decay_t<decltype(v)>;
            if constexpr (std::is_arithmetic_v<type>) result.fill(static_cast<float>(v));
            else for (unsigned i = 0; i < v.size(); ++i) result[i] = v[i];
        }, value);
        for (float v : result)
            if (!std::isfinite(v)) throw std::invalid_argument("Shader value must be finite.");
        return result;
    }
    void validate_shader_extent(const shader_package_data &package,
        unsigned sw, unsigned sh, unsigned vw, unsigned vh) {
        if (!sw || !sh || !vw || !vh || sw > 4096 || sh > 4096 ||
            vw > 4096 || vh > 4096)
            throw std::invalid_argument("Shader extents must be between 1 and 4096 pixels.");
        std::uint64_t bytes = std::uint64_t(sw) * sh * sizeof(shader_vector);
        std::uint64_t operations = 0;
        for (const auto &pass : package.passes) {
            const auto [w, h] = extent(pass, sw, sh, vw, vh);
            const std::uint64_t pixels = std::uint64_t(w) * h;
            // New targets, old history, new retained history, and displayed images.
            bytes += pixels * (sizeof(shader_vector) * (pass.history ? 3 : 1) + 8);
            operations += pixels * pass.instructions.size();
        }
        if (bytes > memory_budget || operations > 128u * 1024u * 1024u)
            throw std::invalid_argument("Shader exceeds the per-view memory or work budget.");
    }
    std::unique_ptr<img> execute_shader(const shader_package_data &package,
        const img &source,
        const std::map<std::string, shader_value, std::less<>> &parameters,
        unsigned vw, unsigned vh, std::uint64_t sequence, double source_time,
        double elapsed, double delta, std::vector<shader_frame> &history) {
        validate_shader_extent(package, source.w(), source.h(), vw, vh);
        shader_frame original;
        original.width = source.w(); original.height = source.h();
        original.pixels.resize(std::size_t(original.width) * original.height);
        for (std::size_t i = 0; i < original.pixels.size(); ++i) {
            const rgba pixel = source.pixels()[i];
            const float alpha = pixel.a / 255.0f;
            original.pixels[i] = {linear(pixel.r / 255.0f) * alpha,
                linear(pixel.g / 255.0f) * alpha,
                linear(pixel.b / 255.0f) * alpha, alpha};
        }
        const shader_frame empty;
        std::array<shader_vector, 64> builtins{};
        builtins[1] = {float(source.w()), float(source.h()), float(sequence), float(source_time)};
        builtins[2] = {float(elapsed), float(delta), float(vw), float(vh)};
        for (unsigned i = 0; i < package.parameters.size(); ++i)
            builtins[3 + i] = shader_components(parameters.at(package.parameters[i].name));
        std::vector<shader_frame> outputs;
        for (const auto &pass : package.passes) {
            auto [w, h] = extent(pass, source.w(), source.h(), vw, vh);
            shader_frame output{w, h, std::vector<shader_vector>(std::size_t(w) * h)};
            auto registers = builtins;
            for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
                registers[0] = {(x + 0.5f) / w, (y + 0.5f) / h, 1.0f / w, 1.0f / h};
                for (const auto &op : pass.instructions) {
                    shader_vector result{};
                    if (op.opcode == shader_opcode::constant) result = op.literal;
                    else if (op.opcode == shader_opcode::sample_nearest ||
                             op.opcode == shader_opcode::sample_linear) {
                        const shader_frame *input = &original;
                        if (op.input >= 0) input = &outputs[op.input];
                        else if (op.input < -1) {
                            const auto index = -std::int64_t(op.input) - 2;
                            input = index < static_cast<std::int64_t>(history.size())
                                ? &history[index] : &empty;
                        }
                        result = sample(*input, registers[op.a],
                            op.opcode == shader_opcode::sample_linear);
                    } else if (op.opcode == shader_opcode::swizzle) {
                        for (unsigned c = 0; c < 4; ++c)
                            result[c] = registers[op.a][op.channels[c]];
                    } else if (op.opcode == shader_opcode::dot) {
                        float dot = 0;
                        for (unsigned c = 0; c < 4; ++c)
                            dot += registers[op.a][c] * registers[op.b][c];
                        result.fill(dot);
                    } else {
                        for (unsigned c = 0; c < 4; ++c)
                            result[c] = component(op.opcode, registers[op.a][c],
                                registers[op.b][c], registers[op.c][c]);
                    }
                    for (float &v : result) if (!std::isfinite(v)) v = 0;
                    registers[op.destination] = result;
                }
                output.pixels[std::size_t(y) * w + x] = registers[pass.output];
            }
            outputs.push_back(std::move(output));
        }
        const auto &last = outputs.back();
        auto image = std::make_unique<img>(last.width, last.height);
        for (std::size_t i = 0; i < last.pixels.size(); ++i) {
            const auto &pixel = last.pixels[i];
            const float alpha = std::clamp(pixel[3], 0.0f, 1.0f);
            image->pixels()[i] = alpha > 0 ? rgba(encoded(pixel[0] / alpha),
                encoded(pixel[1] / alpha), encoded(pixel[2] / alpha),
                static_cast<std::uint8_t>(std::lround(alpha * 255))) : rgba(0, 0, 0, 0);
        }
        std::vector<shader_frame> next_history(package.passes.size());
        for (unsigned i = 0; i < outputs.size(); ++i)
            if (package.passes[i].history) next_history[i] = std::move(outputs[i]);
        history.swap(next_history);
        return image;
    }
}

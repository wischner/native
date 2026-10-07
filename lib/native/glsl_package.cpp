//
// Decodes immutable native-glsl-1 programs and their portable resource
// graph. File contents are data; shader compilation belongs to the
// private renderer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "glsl_shader.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <stdexcept>

namespace
{
    [[noreturn]] void invalid() {
        throw std::invalid_argument("Malformed native-glsl-1 package.");
    }
    template <class value> value read(std::istream &in) {
        value result{};
        if (!(in >> result))
            invalid();
        return result;
    }
    void word(std::istream &in, const char *name) {
        if (read<std::string>(in) != name)
            invalid();
    }
    std::string quoted(std::istream &in) {
        std::string text;
        if (!(in >> std::quoted(text)))
            invalid();
        if (text.size() > 4 * 1024 * 1024)
            invalid();
        return text;
    }
    bool identifier(const std::string &s) {
        if (s.empty() || s.size() > 64)
            return false;
        if (!(std::isalpha(static_cast<unsigned char>(s[0])) ||
              s[0] == '_'))
            return false;
        for (unsigned char c : s)
            if (!(std::isalnum(c) || c == '_'))
                return false;
        return true;
    }
    unsigned bounded(std::istream &in, unsigned maximum) {
        auto n = read<unsigned>(in);
        if (n > maximum)
            invalid();
        return n;
    }
    native::detail::shader_vector vector(std::istream &in) {
        native::detail::shader_vector v{};
        for (auto &x : v) {
            x = read<float>(in);
            if (!std::isfinite(x))
                invalid();
        }
        return v;
    }
} // namespace
namespace native::detail
{
    std::shared_ptr<shader_package_data>
    decode_glsl_package(std::istream &in) {
        if (read<unsigned>(in) != 1)
            invalid();
        auto data = std::make_shared<shader_package_data>();
        std::set<std::string> parameter_names;
        word(in, "parameters");
        unsigned count = bounded(in, 16);
        for (unsigned i = 0; i < count; ++i) {
            word(in, "parameter");
            shader_parameter p;
            p.name = read<std::string>(in);
            if (!identifier(p.name) ||
                !parameter_names.insert(p.name).second)
                invalid();
            const auto type = bounded(in, 4);
            if (!type)
                invalid();
            p.minimum = read<float>(in);
            p.maximum = read<float>(in);
            if (!std::isfinite(p.minimum) ||
                !std::isfinite(p.maximum) || p.minimum > p.maximum)
                invalid();
            auto v = vector(in);
            for (unsigned j = 0; j < type; ++j)
                if (v[j] < p.minimum || v[j] > p.maximum)
                    invalid();
            switch (type) {
            case 1:
                p.default_value = v[0];
                break;
            case 2:
                p.default_value = std::array<float, 2>{v[0], v[1]};
                break;
            case 3:
                p.default_value =
                    std::array<float, 3>{v[0], v[1], v[2]};
                break;
            default:
                p.default_value = v;
            }
            data->parameters.push_back(std::move(p));
        }
        word(in, "textures");
        count = bounded(in, 4);
        std::uint64_t texture_bytes = 0;
        for (unsigned i = 0; i < count; ++i) {
            word(in, "texture");
            glsl_texture t;
            t.width = bounded(in, 2048);
            t.height = bounded(in, 2048);
            if (!t.width || !t.height)
                invalid();
            texture_bytes += std::uint64_t(t.width) * t.height * 4;
            if (texture_bytes > 4 * 1024 * 1024)
                invalid();
            const auto hex = quoted(in);
            if (hex.size() != std::uint64_t(t.width) * t.height * 8)
                invalid();
            t.pixels.resize(std::size_t(t.width) * t.height);
            auto nibble = [](char c) -> unsigned {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                invalid();
            };
            auto *bytes =
                reinterpret_cast<unsigned char *>(t.pixels.data());
            for (std::size_t j = 0; j < hex.size() / 2; ++j)
                bytes[j] =
                    (nibble(hex[j * 2]) << 4) | nibble(hex[j * 2 + 1]);
            data->glsl_textures.push_back(std::move(t));
        }
        word(in, "passes");
        count = bounded(in, 16);
        if (!count)
            invalid();
        std::set<std::string> pass_names;
        for (unsigned i = 0; i < count; ++i) {
            word(in, "pass");
            glsl_pass p;
            p.name = read<std::string>(in);
            if (!identifier(p.name) ||
                !pass_names.insert(p.name).second)
                invalid();
            const auto extent = read<std::string>(in);
            if (extent != "source" && extent != "viewport")
                invalid();
            p.viewport = extent == "viewport";
            p.reduction = bounded(in, 128);
            if (!p.reduction)
                invalid();
            p.history = bounded(in, 1) != 0;
            p.padding = bounded(in, 128);
            p.alignment = bounded(in, 32);
            if (!p.alignment)
                invalid();
            p.uv_mapping = bounded(in, 2);
            p.source_rect = vector(in);
            for (float x : p.source_rect)
                if (std::abs(x) > 64)
                    invalid();
            word(in, "vertex");
            p.vertex = quoted(in);
            word(in, "fragment");
            p.fragment = quoted(in);
            if (p.vertex.empty() || p.fragment.empty())
                invalid();
            word(in, "uniforms");
            unsigned n = bounded(in, 64);
            std::set<std::string> uniform_names;
            for (unsigned j = 0; j < n; ++j) {
                word(in, "uniform");
                glsl_uniform u;
                u.name = read<std::string>(in);
                u.binding = read<std::string>(in);
                u.components = bounded(in, 4);
                u.value = vector(in);
                if (!u.components || !identifier(u.name) ||
                    !uniform_names.insert(u.name).second)
                    invalid();
                if (u.binding != "value" && u.binding != "elapsed" &&
                    u.binding != "delta" &&
                    u.binding != "source_time" &&
                    u.binding != "previous_time" &&
                    u.binding != "source_size" &&
                    u.binding != "viewport_size" &&
                    u.binding != "pass_texel" &&
                    !parameter_names.contains(u.binding))
                    invalid();
                p.uniforms.push_back(std::move(u));
            }
            word(in, "samplers");
            n = bounded(in, 8);
            for (unsigned j = 0; j < n; ++j) {
                word(in, "sampler");
                glsl_sampler t;
                t.name = read<std::string>(in);
                t.input = read<int>(in);
                t.wrap = bounded(in, 2);
                if (!identifier(t.name))
                    invalid();
                if (t.input >= 100) {
                    if (unsigned(t.input - 100) >=
                        data->glsl_textures.size())
                        invalid();
                } else if (t.input >= 0 && unsigned(t.input) >= i)
                    invalid();
                else if (t.input < -1 &&
                         (-std::int64_t(t.input) - 2) >= count)
                    invalid();
                p.samplers.push_back(std::move(t));
            }
            data->glsl_passes.push_back(std::move(p));
        }
        for (const auto &p : data->glsl_passes)
            for (const auto &s : p.samplers)
                if (s.input < -1 &&
                    !data->glsl_passes[-std::int64_t(s.input) - 2]
                         .history)
                    invalid();
        word(in, "end");
        in >> std::ws;
        if (!in.eof())
            invalid();
        return data;
    }
} // namespace native::detail

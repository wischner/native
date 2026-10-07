//
// Validates immutable native-image-1 packages with bounded vector programs.
// Package parsing has no toolkit dependencies and never executes caller code.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "shader_program.h"
#include "glsl_shader.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr std::size_t maximum_package_bytes = 8 * 1024 * 1024;
    [[noreturn]] void invalid() {
        throw std::invalid_argument("Malformed or unsupported native-image-1 package.");
    }
    // Only identifier text enters parameter lookup and package labels.
    bool identifier(const std::string &name) {
        if (name.empty() || name.size() > 64 ||
            !((name[0] >= 'a' && name[0] <= 'z') ||
              (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_')) return false;
        for (unsigned char c : name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_')) return false;
        return true;
    }
    template <typename value_type>
    value_type read(std::istream &input) {
        value_type value{};
        if (!(input >> value)) invalid();
        return value;
    }
    void keyword(std::istream &input, const char *expected) {
        if (read<std::string>(input) != expected) invalid();
    }
    native::shader_value typed_value(unsigned type,
                                    native::detail::shader_vector v) {
        switch (type) {
        case 0:
            if (v[0] < -2147483648.0 || v[0] > 2147483647.0 ||
                std::floor(v[0]) != v[0]) invalid();
            return static_cast<int>(v[0]);
        case 1: return v[0];
        case 2: return std::array<float, 2>{v[0], v[1]};
        case 3: return std::array<float, 3>{v[0], v[1], v[2]};
        case 4: return v;
        default: invalid();
        }
    }
    native::detail::shader_instruction instruction(
        std::istream &input, std::array<bool, 64> &initialized) {
        using native::detail::shader_opcode;
        native::detail::shader_instruction op;
        const auto name = read<std::string>(input);
        op.destination = read<unsigned>(input);
        if (op.destination < 20 || op.destination >= 64) invalid();
        auto reg = [&]() {
            const auto index = read<unsigned>(input);
            if (index >= 64 || !initialized[index]) invalid();
            return index;
        };
        if (name == "const") {
            op.opcode = shader_opcode::constant;
            for (auto &component : op.literal) {
                component = read<float>(input);
                if (!std::isfinite(component)) invalid();
            }
        } else if (name == "sample" || name == "linear") {
            op.opcode = name == "sample" ? shader_opcode::sample_nearest
                                         : shader_opcode::sample_linear;
            op.input = read<int>(input);
            op.a = reg();
        } else if (name == "swizzle") {
            op.opcode = shader_opcode::swizzle;
            op.a = reg();
            for (auto &channel : op.channels) {
                channel = read<unsigned>(input);
                if (channel > 3) invalid();
            }
        } else {
            static const std::pair<const char *, shader_opcode> names[] = {
                {"step", shader_opcode::step}, {"add", shader_opcode::add}, {"sub", shader_opcode::subtract},
                {"mul", shader_opcode::multiply}, {"div", shader_opcode::divide},
                {"min", shader_opcode::minimum}, {"max", shader_opcode::maximum},
                {"pow", shader_opcode::power}, {"sin", shader_opcode::sine},
                {"cos", shader_opcode::cosine}, {"exp", shader_opcode::exponential},
                {"floor", shader_opcode::floor}, {"abs", shader_opcode::absolute},
                {"sqrt", shader_opcode::square_root}, {"clamp", shader_opcode::saturate},
                {"mix", shader_opcode::mix}, {"dot", shader_opcode::dot}
            };
            const auto match = std::find_if(std::begin(names), std::end(names),
                [&](const auto &entry) { return name == entry.first; });
            if (match == std::end(names)) invalid();
            op.opcode = match->second;
            op.a = reg();
            const bool unary = op.opcode >= shader_opcode::sine &&
                               op.opcode <= shader_opcode::saturate;
            if (!unary) op.b = reg();
            if (op.opcode == shader_opcode::mix) op.c = reg();
        }
        initialized[op.destination] = true;
        return op;
    }
}
namespace native
{
    shader_package::shader_package() = default;
    bool shader_package::get_valid() const { return static_cast<bool>(_data); }
    const std::vector<shader_parameter> &shader_package::get_parameters() const {
        static const std::vector<shader_parameter> empty;
        return _data ? _data->parameters : empty;
    }
    shader_package shader_package::load(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) throw std::runtime_error("Cannot open shader package.");
        const auto length = stream.tellg();
        if (length <= 0 || length > std::streamoff(maximum_package_bytes)) invalid();
        std::vector<std::byte> bytes(static_cast<std::size_t>(length));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char *>(bytes.data()), length))
            throw std::runtime_error("Cannot read shader package.");
        return decode(bytes);
    }
    shader_package shader_package::decode(std::span<const std::byte> bytes) {
        if (bytes.empty() || bytes.size() > maximum_package_bytes) invalid();
        std::string text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        for (unsigned char c : text)
            if ((c < 32 && c != '\n' && c != '\r' && c != '\t') || c > 126) invalid();
        std::istringstream input(text);
        const auto profile = read<std::string>(input);
        if (profile == "native-glsl-1") {
            shader_package package;
            package._data = detail::decode_glsl_package(input);
            return package;
        }
        if (profile != "native-image-1" || bytes.size() > 1024 * 1024) invalid();
        if (read<unsigned>(input) != 1) invalid();
        keyword(input, "parameters");
        const unsigned count = read<unsigned>(input);
        if (count > 16) invalid();
        auto data = std::make_shared<detail::shader_package_data>();
        for (unsigned i = 0; i < count; ++i) {
            keyword(input, "parameter");
            shader_parameter parameter;
            parameter.name = read<std::string>(input);
            if (!identifier(parameter.name) || parameter.name == "uv" ||
                parameter.name == "source_info" || parameter.name == "clock" || std::any_of(
                data->parameters.begin(), data->parameters.end(),
                [&](const auto &p) { return p.name == parameter.name; })) invalid();
            const unsigned type = read<unsigned>(input);
            if (type > 4) invalid();
            parameter.minimum = read<float>(input);
            parameter.maximum = read<float>(input);
            if (!std::isfinite(parameter.minimum) || !std::isfinite(parameter.maximum) ||
                parameter.minimum > parameter.maximum) invalid();
            if (type == 0) {
                const double value = read<double>(input);
                if (!std::isfinite(value) || std::floor(value) != value ||
                    value < -2147483648.0 || value > 2147483647.0 ||
                    value < parameter.minimum || value > parameter.maximum) invalid();
                parameter.default_value = static_cast<int>(value);
            } else {
                detail::shader_vector value{};
                for (unsigned j = 0; j < std::max(1u, type); ++j) {
                    value[j] = read<float>(input);
                    if (!std::isfinite(value[j]) || value[j] < parameter.minimum ||
                        value[j] > parameter.maximum) invalid();
                }
                parameter.default_value = typed_value(type, value);
            }
            data->parameters.push_back(std::move(parameter));
        }
        keyword(input, "passes");
        const unsigned pass_count = read<unsigned>(input);
        if (!pass_count || pass_count > 8) invalid();
        for (unsigned i = 0; i < pass_count; ++i) {
            keyword(input, "pass");
            detail::shader_pass pass;
            pass.name = read<std::string>(input);
            if (!identifier(pass.name) || std::any_of(
                data->passes.begin(), data->passes.end(),
                [&](const auto &p) { return p.name == pass.name; })) invalid();
            auto extent = read<std::string>(input);
            if (extent != "source" && extent != "viewport") invalid();
            pass.viewport = extent == "viewport";
            pass.reduction = read<unsigned>(input);
            unsigned retained = read<unsigned>(input);
            if (!pass.reduction || pass.reduction > 16 || retained > 1) invalid();
            pass.history = retained != 0;
            pass.output = read<unsigned>(input);
            const unsigned operations = read<unsigned>(input);
            if (!operations || operations > 128) invalid();
            std::array<bool, 64> initialized{};
            for (unsigned reg = 0; reg < 3 + count; ++reg) initialized[reg] = true;
            for (unsigned j = 0; j < operations; ++j)
                pass.instructions.push_back(instruction(input, initialized));
            if (pass.output >= 64 || !initialized[pass.output]) invalid();
            data->passes.push_back(std::move(pass));
        }
        for (unsigned i = 0; i < pass_count; ++i)
            for (const auto &op : data->passes[i].instructions) {
                if (op.opcode != detail::shader_opcode::sample_nearest &&
                    op.opcode != detail::shader_opcode::sample_linear) continue;
                // -1 is source, -(pass+2) is previous completed pass output.
                if (op.input >= 0 && unsigned(op.input) >= i) invalid();
                if (op.input < -1) {
                    const auto previous = -static_cast<std::int64_t>(op.input) - 2;
                    if (previous >= pass_count || !data->passes[previous].history) invalid();
                }
            }
        keyword(input, "end");
        input >> std::ws;
        if (!input.eof()) invalid();
        shader_package package;
        package._data = std::move(data);
        return package;
    }
}

namespace native::detail
{
    shader_package_data::~shader_package_data() = default;
    const shader_package_data &shader_package_data::get(const shader_package &package) {
        if (!package._data) throw std::invalid_argument("Empty shader package.");
        return *package._data;
    }
}

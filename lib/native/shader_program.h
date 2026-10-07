//
// Describes the private bounded vector instruction profile and pass storage.
// The same linear premultiplied pixel executor runs on every UI backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <native/shader.h>
#include <native/graphics.h>
#include <map>
#include <vector>

namespace native::detail
{
    using shader_vector = std::array<float, 4>;
    enum class shader_opcode
    {
        constant, add, subtract, multiply, divide, minimum, maximum,
        power, sine, cosine, exponential, floor, absolute, square_root,
        saturate, step, mix, dot, swizzle, sample_nearest, sample_linear
    };
    struct shader_instruction
    {
        shader_opcode opcode;
        unsigned destination = 0, a = 0, b = 0, c = 0;
        int input = -1;
        shader_vector literal{};
        std::array<unsigned, 4> channels{};
    };
    struct shader_pass
    {
        std::string name;
        bool viewport = false;
        bool history = false;
        unsigned reduction = 1;
        unsigned output = 0;
        std::vector<shader_instruction> instructions;
    };
    struct glsl_uniform {
        std::string name, binding;
        unsigned components = 1;
        shader_vector value{};
    };
    struct glsl_sampler {
        std::string name;
        int input = -1;
        unsigned wrap = 0;
    };
    struct glsl_pass {
        std::string name, vertex, fragment;
        bool viewport = false, history = false;
        unsigned reduction = 1, padding = 0, alignment = 1, uv_mapping = 0;
        shader_vector source_rect{0,0,1,1};
        std::vector<glsl_uniform> uniforms;
        std::vector<glsl_sampler> samplers;
    };
    struct glsl_texture {
        unsigned width = 0, height = 0;
        std::vector<rgba> pixels;
    };
    struct shader_package_data
    {
        // Return immutable validated data to private executors and conformance tests.
        static const shader_package_data &get(const shader_package &package);
        std::vector<shader_parameter> parameters;
        std::vector<shader_pass> passes;
        std::vector<glsl_pass> glsl_passes;
        std::vector<glsl_texture> glsl_textures;
        ~shader_package_data();
    };
    struct shader_frame
    {
        unsigned width = 0, height = 0;
        std::vector<shader_vector> pixels;
    };
    // Checked memory bound includes source, outputs and previous-frame storage.
    void validate_shader_extent(const shader_package_data &package,
        unsigned source_width, unsigned source_height,
        unsigned viewport_width, unsigned viewport_height);
    // Run all passes transactionally. History advances only on success.
    std::unique_ptr<img> execute_shader(const shader_package_data &package,
        const img &source,
        const std::map<std::string, shader_value, std::less<>> &parameters,
        unsigned viewport_width, unsigned viewport_height,
        std::uint64_t sequence, double source_time, double elapsed, double delta,
        std::vector<shader_frame> &history);
    // Validate finite typed components before committing a property.
    shader_vector shader_components(const shader_value &value);
}

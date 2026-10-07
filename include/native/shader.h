//
// Declares immutable portable image-shader packages and typed parameters.
// Packages contain bounded image programs, not platform graphics handles.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace native
{
    namespace detail { struct shader_package_data; }
    using shader_value = std::variant<int, float, std::array<float, 2>,
        std::array<float, 3>, std::array<float, 4>>;
    enum class shader_animation { source_updates, continuous };
    enum class shader_fallback { original_image, require_effect };
    enum class shader_fit { fit, fill, stretch };
    enum class shader_status { no_effect, pending, active, unavailable };
    struct shader_capabilities
    {
        bool image_profile_1 = false;
        bool history = false;
        bool linear_float_targets = false;
        bool accelerated = false;
        unsigned max_texture_size = 0;
        unsigned max_passes = 0;
        bool glsl_profile_1 = false;
    };
    struct shader_parameter
    {
        std::string name;
        shader_value default_value = 0.0f;
        float minimum = 0, maximum = 1;
    };
    // Immutable, owned package; decoding is safe away from the UI thread.
    class shader_package
    {
    public:
        // Construct an empty package, which cannot be installed.
        shader_package();
        // Load a bounded package using C++ filesystem paths and streams.
        static shader_package load(const std::filesystem::path &path);
        // Decode/validate portable native-image-1 or native-glsl-1 data and reflection.
        // Throws invalid_argument for malformed/unsupported packages.
        static shader_package decode(std::span<const std::byte> bytes);
        // Return whether an immutable validated program exists.
        bool get_valid() const;
        // Return parameter names, exact types, defaults and accepted ranges.
        const std::vector<shader_parameter> &get_parameters() const;
    private:
        friend class shader_view;
        friend struct detail::shader_package_data;
        std::shared_ptr<const detail::shader_package_data> _data;
    };
}

//
// Declares cross-platform font descriptions and owning font handles.
// Backends translate these values to their native font resources.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace native
{
    // Describes vertical and horizontal metrics for a font face.
    struct font_metrics
    {
        int ascent = 0;
        int descent = 0;
        int leading = 0;
        int height = 0;
        int max_advance = 0;
    };

    // Describes the layout size and cursor advance of text.
    struct text_metrics
    {
        int width = 0;
        int height = 0;
        int advance = 0;
    };

    // Identifies the semantic purpose of a stock font.
    enum class font_role
    {
        system,
        fixed,
        icon_label,
        title,
        small,
        control
    };

    // Identifies how a font resource entered the portable API.
    enum class font_source
    {
        invalid,
        stock,
        file,
        memory
    };

    // Describes one installed face without exposing a native handle.
    struct font_description
    {
        std::string family;
        std::string style;
        std::string face_name;
        std::filesystem::path path;
        int weight = 400;
        std::uint32_t face_index = 0;
        bool italic = false;
        bool fixed_pitch = false;
    };

    // Describes a stock or portable font using backend-neutral values.
    struct font_spec
    {
        // Family and style reported by the selected face.
        std::string family;
        std::string style;

        // File path for file-backed fonts; empty for memory fonts.
        std::filesystem::path resource;

        // Pixel line size, or zero for a backend-selected stock size.
        int size = 0;

        // CSS-compatible weight reported by the selected face.
        int weight = 400;

        // Whether the selected face reports an italic style.
        bool italic = false;

        // Face selected from a TrueType collection.
        std::uint32_t face_index = 0;

        // Resource category represented by this description.
        font_source source = font_source::invalid;
    };

    // Owns a backend font registration through an opaque identifier.
    class font_t
    {
    public:
        // Construct an invalid font handle.
        font_t();

        // Release the registered backend font, if any.
        ~font_t();

        // Move a font registration from another handle.
        font_t(font_t &&other) noexcept;

        // Replace this handle with another font registration.
        font_t &operator=(font_t &&other) noexcept;

        // Font handles cannot be copied because they own registrations.
        font_t(const font_t &) = delete;

        // Font handles cannot be copy-assigned.
        font_t &operator=(const font_t &) = delete;

        // Determine whether this handle has a backend registration.
        bool valid() const;

        // Return the cross-platform description used for this font.
        const font_spec &spec() const;

        //
        // Return the opaque identifier used by backend registries.
        //
        // Notes:
        //      The identifier is stable across moves and has no public
        //      meaning beyond identifying the font.
        //
        std::uint32_t id() const;

        //
        // Create a portable TrueType font by reading a complete file.
        //
        // Parameters:
        //      path        - TrueType/OpenType file or collection.
        //      size        - Positive pixel line size.
        //      face_index  - Zero-based face within a collection.
        //
        // Returns:
        //      An invalid font when validation or creation fails.
        //
        static font_t from_file(
            const std::filesystem::path &path,
            int size,
            std::uint32_t face_index = 0);

        //
        // Create a portable TrueType font by copying encoded bytes.
        //
        // Parameters:
        //      data        - Borrowed encoded bytes copied before
        //                    return.
        //      data_size   - Number of accessible bytes.
        //      size        - Positive pixel line size.
        //      face_index  - Zero-based face within a collection.
        //
        // Returns:
        //      An invalid font when validation or creation fails.
        //
        static font_t from_memory(
            const std::uint8_t *data,
            std::size_t data_size,
            int size,
            std::uint32_t face_index = 0);

        // Enumerate installed TrueType/OpenType faces on this machine.
        static std::vector<font_description> enumerate_installed();

        // Return the process-lifetime stock font for a semantic role.
        static const font_t &stock(font_role role);

        // Return this face's editor-oriented font metrics.
        font_metrics get_metrics() const;

        // Measure a UTF-8 string without drawing it.
        text_metrics measure_text(const std::string &text) const;

        // Measure one Unicode character without drawing it.
        text_metrics measure_character(char32_t character) const;

    private:
        std::uint32_t _id = 0;
        font_spec _spec;
    };
} // namespace native

//
// Declares cross-platform font descriptions and owning font handles.
// Backends translate these values to their native font resources.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <string>

namespace native
{
    // Identifies the semantic purpose of a stock font.
    enum class font_role
    {
        system,
        fixed,
        title,
        small_,
        control
    };

    // Describes a requested font independently of a native toolkit.
    struct font_spec
    {
        // Family name, file path, or backend-native description.
        std::string name;

        // Point size, or zero to use the backend default.
        int size = 0;

        // Request a bold face when the backend supports one.
        bool bold = false;

        // Request an italic face when the backend supports one.
        bool italic = false;
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
        // Create a font from a cross-platform description.
        //
        // Returns:
        //      A font handle, which may be invalid if creation fails.
        //
        static font_t create(const font_spec &spec);

        // Return the process-lifetime stock font for a semantic role.
        static const font_t &stock(font_role role);

    private:
        std::uint32_t _id = 0;
        font_spec _spec;
    };
}

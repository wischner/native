//
// Declares portable filesystem resources backed by the C++ standard library
// and platform icon and known-directory services.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "geometry.h"

namespace native
{
    // Identifies where the pixels of a file icon came from.
    enum class file_icon_source
    {
        native,
        generic_file,
        generic_directory
    };

    // Owns one exact-size file or directory icon encoded as PNG.
    class file_icon final
    {
    public:
        // Obtain the native icon for an existing path, with a generic
        // fallback selected from its standard filesystem type.
        static file_icon from_path(const std::filesystem::path &path,
                                   dim size);

        // Obtain a file icon, allowing a native type icon for a path which
        // does not exist and falling back to the generic file image.
        static file_icon for_file(const std::filesystem::path &path,
                                  dim size);

        // Obtain a directory icon, falling back to the generic folder image.
        static file_icon for_directory(
            const std::filesystem::path &path,
            dim size);

        // Return the width and height of the square PNG in pixels.
        dim get_size() const;

        // Return whether this is a native or generic file/folder icon.
        file_icon_source get_source() const;

        // Determine whether native icon lookup used a generic fallback.
        bool is_generic() const;

        // Return the complete encoded PNG byte sequence.
        const std::vector<std::uint8_t> &get_png() const;

    private:
        file_icon(dim size,
                  file_icon_source source,
                  std::vector<std::uint8_t> png);

        static file_icon obtain(const std::filesystem::path &path,
                                dim size,
                                bool directory);

        dim _size;
        file_icon_source _source;
        std::vector<std::uint8_t> _png;
    };

    // Identifies a conventional location supplied by the operating system.
    enum class special_directory_kind
    {
        home,
        desktop,
        documents,
        downloads,
        music,
        pictures,
        videos,
        public_share,
        templates,
        applications,
        fonts,
        configuration,
        application_data,
        cache,
        temporary
    };

    // Describes one detected special directory as a standard C++ path.
    class special_directory final
    {
    public:
        // Return the semantic role of this directory.
        special_directory_kind get_kind() const;

        // Return a stable, non-localized label for the directory role.
        const std::string &get_name() const;

        // Return the path supplied by the current operating system.
        const std::filesystem::path &get_path() const;

        // Obtain this directory's exact-size native or generic PNG icon.
        file_icon get_icon(dim size) const;

        // Refresh and return all special directories known to the system.
        static const std::vector<special_directory> &detect();

        // Return the number of entries from the latest detection.
        static int count();

        // Return one entry by detection index, or null when out of range.
        static special_directory *at(int index);

        // Return an entry by semantic role, or null when unavailable.
        static special_directory *find(special_directory_kind kind);

    private:
        special_directory(special_directory_kind kind,
                          std::string name,
                          std::filesystem::path path);

        special_directory_kind _kind;
        std::string _name;
        std::filesystem::path _path;

        static std::vector<special_directory> _directories;
    };
} // namespace native

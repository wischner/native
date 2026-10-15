//
// Declares private platform font-location discovery for portable enumeration.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <filesystem>
#include <vector>

namespace native::detail
{
    // Return platform font directories for standard C++ traversal.
    std::vector<std::filesystem::path> font_roots();
    // Return additional files advertised by the native font registry.
    std::vector<std::filesystem::path> registered_font_files();
} // namespace native::detail

//
// Declares portable file-dialog wildcard matching without POSIX helpers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

namespace native::detail
{
    // Match a file name against case-insensitive '*' and '?' wildcards.
    bool matches_file_pattern(std::string pattern, std::string value);
} // namespace native::detail

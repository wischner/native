//
// Declares private UTF-8 validation and navigation helpers shared by
// clipboard and text-edit implementations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace native::detail
{
    // Return whether text is canonical, scalar-value UTF-8.
    bool valid_utf8(const std::string &text);

    // Convert an ISO-8859-1 byte string to canonical UTF-8.
    std::string latin1_to_utf8(const std::uint8_t *data,
                               std::size_t size);

    // Return the preceding UTF-8 scalar boundary.
    std::size_t previous_utf8(const std::string &text,
                              std::size_t offset);

    // Return the following UTF-8 scalar boundary.
    std::size_t next_utf8(const std::string &text,
                          std::size_t offset);
} // namespace native::detail

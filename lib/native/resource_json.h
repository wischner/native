//
// Parses the bounded JSON subset used by resource manifests. Values are
// objects, arrays, UTF-8 strings and unsigned 32-bit integers only.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace native::detail
{
    struct resource_json
    {
        using object = std::map<std::string, resource_json>;
        using array = std::vector<resource_json>;
        std::variant<object, array, std::string, std::uint32_t> value;
    };
    // Reject duplicate keys, invalid Unicode, trailing data and depth > 16.
    resource_json parse_resource_json(std::string_view text);
}

//
// Declares backend-neutral UTF-8 table text matching shared by the
// default virtual model scan and table controller.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native/table_model.h>

namespace native::detail
{
    // Match UTF-8 display text using portable table search semantics.
    bool table_text_matches(const std::string &value,
                            const std::string &query,
                            table_search_match match,
                            table_search_case case_mode);
} // namespace native::detail

//
// Removes the legacy XView coord macro before portable C++ headers are
// parsed. Native uses coord as a scoped fixed-width type name.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#ifdef coord
#undef coord
#endif

//
// Implements X11/Athena file dialogs through an installed desktop
// chooser because Athena does not provide a standard file chooser.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/file_dialog.h>

namespace native
{
    void file_dialog::cancel_native_dialog() const {}
} // namespace native

//
// Presents the AppKit standard open panel as an owner-modal sheet and
// translates its asynchronous completion into the portable result.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include <stdexcept>
#include <vector>

#import <AppKit/AppKit.h>

#include "file_dialog_common.h"
#include "globals.h"

namespace native
{
    void open_file_dialog::show() const {
        if (!begin_dialog())
            return;

        app_wnd *owner = get_owner();
        NSWindow *owner_window = owner
                                     ? mac::wnd_bindings
                                           .handle_from_object(owner)
                                     : nil;
        if (!owner_window) {
            const_cast<open_file_dialog *>(this)->on_native_cancel();
            throw std::runtime_error(
                "macOS: Missing owner window for an open panel.");
        }

        NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];
        mac::configure_file_panel(panel, *this);
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setResolvesAliases:YES];
        [panel setAllowsMultipleSelection:get_allow_multiple() ? YES
                                                               : NO];

        auto *dialog = const_cast<open_file_dialog *>(this);
        try {
            mac::file_dialog_bindings.register_pair(dialog, panel);
        } catch (...) {
            [panel release];
            dialog->on_native_cancel();
            throw;
        }
        [panel beginSheetModalForWindow:owner_window
                     completionHandler:^(NSModalResponse response) {
            native::file_dialog *active =
                mac::file_dialog_bindings.handle_from_object(panel);
            if (active != dialog)
                return;

            mac::file_dialog_bindings.unregister_by_handle(dialog);
            if (response == NSModalResponseOK) {
                std::vector<std::string> paths;
                for (NSURL *url in [panel URLs]) {
                    std::string path = mac::path_from_url(url);
                    if (!path.empty())
                        paths.push_back(path);
                }
                dialog->on_native_accept(paths);
            } else {
                dialog->on_native_cancel();
            }
            [panel release];
        }];
    }
} // namespace native

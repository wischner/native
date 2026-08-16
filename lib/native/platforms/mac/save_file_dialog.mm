//
// Presents the AppKit standard save panel as an owner-modal sheet and
// translates its asynchronous completion into the portable result.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

#include <stdexcept>
#include <string>
#include <vector>

#import <AppKit/AppKit.h>

#include "file_dialog_common.h"
#include "globals.h"

namespace native
{
    void save_file_dialog::show() const {
        if (!begin_dialog())
            return;

        app_wnd *owner = get_owner();
        NSWindow *owner_window = owner
                                     ? mac::wnd_bindings
                                           .handle_from_object(owner)
                                     : nil;
        if (!owner_window) {
            const_cast<save_file_dialog *>(this)->on_native_cancel();
            throw std::runtime_error(
                "macOS: Missing owner window for a save panel.");
        }

        NSSavePanel *panel = [[NSSavePanel savePanel] retain];
        mac::configure_file_panel(panel, *this);
        if (!get_suggested_name().empty()) {
            [panel setNameFieldStringValue:[NSString
                stringWithUTF8String:get_suggested_name().c_str()]];
        }

        auto *dialog = const_cast<save_file_dialog *>(this);
        try {
            mac::file_dialog_bindings.register_pair(dialog, panel);
        } catch (...) {
            [panel release];
            dialog->on_native_cancel();
            throw;
        }
        if (mac::global_app)
            [mac::global_app activateIgnoringOtherApps:YES];
        [owner_window makeKeyAndOrderFront:nil];
        [panel beginSheetModalForWindow:owner_window
                     completionHandler:^(NSModalResponse response) {
            native::file_dialog *active =
                mac::file_dialog_bindings.handle_from_object(panel);
            if (active != dialog)
                return;

            mac::file_dialog_bindings.unregister_by_handle(dialog);
            if (response == NSModalResponseOK) {
                std::string path = mac::add_default_extension(
                    mac::path_from_url([panel URL]),
                    dialog->get_default_extension());
                dialog->on_native_accept({path});
            } else {
                dialog->on_native_cancel();
            }
            [panel release];
        }];
    }
} // namespace native

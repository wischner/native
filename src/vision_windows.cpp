//
// Implements the independent Vision modeless and modal child windows.
// Native controls remain real windows while theme samples are painted.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "vision_window.h"

#include <memory>

namespace vision
{
    feature_inspector::feature_inspector(native::app_wnd &owner)
        : native::modeless_wnd(owner, "Vision Inspector",
                               150, 140, 390, 280) {
        on_wnd_paint.connect(this, &feature_inspector::on_paint);
    }

    bool feature_inspector::on_paint(
        native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text("This is an independent modeless window.",
                          native::point(18, 18));
        event.g.draw_text("The main window remains interactive.",
                          native::point(18, 42));

        std::unique_ptr<native::theme> appearance =
            native::theme::create(event.g);
        native::theme::state selected;
        selected.selected = true;
        appearance->draw_button(native::rect(18, 78, 150, 30),
                                "Theme button");
        appearance->draw_check(native::rect(18, 120, 170, 24),
                               "Theme check", selected);
        appearance->draw_radio(native::rect(18, 152, 170, 24),
                               "Theme radio", selected);
        appearance->draw_list(
            native::rect(210, 78, 155, 104),
            {"Controls", "Images", "Fonts", "Clipboard"}, 2,
            selected);
        return true;
    }

    feature_dialog::feature_dialog(native::app_wnd &owner)
        : native::modal_wnd(owner, "Vision Modal Dialog",
                            190, 180, 380, 210)
        , _accept("Accept", 92, 148, 88, 30)
        , _cancel("Cancel", 196, 148, 88, 30) {
        on_wnd_create.connect(this, &feature_dialog::on_create);
        on_wnd_paint.connect(this, &feature_dialog::on_paint);
        _accept.on_click.connect(this, &feature_dialog::on_accept);
        _cancel.on_click.connect(this, &feature_dialog::on_cancel);
    }

    bool feature_dialog::on_create() {
        _accept.set_parent(this);
        _accept.create();
        _accept.show();
        _cancel.set_parent(this);
        _cancel.create();
        _cancel.show();
        return true;
    }

    bool feature_dialog::on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "This dialog owns focus and blocks its owner.",
                          native::point(24, 28));
        event.g.draw_text(
            "Closing it produces a portable dialog_result.",
                          native::point(24, 56));
        event.g.draw_text(
            "Paint and lifecycle dispatch continue normally.",
                          native::point(24, 84));
        return true;
    }

    bool feature_dialog::on_accept() {
        close(native::dialog_result::accepted);
        return true;
    }

    bool feature_dialog::on_cancel() {
        close(native::dialog_result::cancelled);
        return true;
    }
} // namespace vision

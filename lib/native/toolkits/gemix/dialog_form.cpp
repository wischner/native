//
// Implements positioned AES forms without modifying the pinned GEM SDK.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "dialog_form.h"
#include "globals.h"
#include <algorithm>
#include <cstdint>

namespace linux::gemix
{
    dialog_form::dialog_form(const native::app_wnd &owner,
                             int width,
                             int height) {
        const auto desktop = desktop_rect();
        const auto parent = work_rect(wnd_bindings.handle_from_object(
            const_cast<native::app_wnd *>(&owner)));
        width = std::min(width, int(desktop.d.w) - 16);
        height = std::min(height, int(desktop.d.h) - 16);
        _bounds = native::rect(
            std::clamp(int(parent.p.x) + (int(parent.d.w) - width) / 2,
                       int(desktop.p.x) + 8,
                       desktop.x2() - width - 8),
            std::clamp(int(parent.p.y) + (int(parent.d.h) - height) / 2,
                       int(desktop.p.y) + 8,
                       desktop.y2() - height - 8),
            width,
            height);
        _objects.push_back(OBJECT{-1,
                                  -1,
                                  -1,
                                  G_BOX,
                                  0,
                                  OUTLINED,
                                  0x00011100L,
                                  WORD(_bounds.p.x),
                                  WORD(_bounds.p.y),
                                  WORD(width),
                                  WORD(height)});
    }

    dialog_form::~dialog_form() {
        if (!_started)
            return;
        form_dial(FMD_FINISH,
                  0,
                  0,
                  0,
                  0,
                  _bounds.p.x,
                  _bounds.p.y,
                  _bounds.d.w,
                  _bounds.d.h);
        for (auto *window : windows)
            request_repaint(window);
        flush_repaints();
    }

    LONG dialog_form::store(std::string text) {
        _strings.push_back(std::move(text));
        return reinterpret_cast<LONG>(_strings.back().data());
    }

    int dialog_form::add(UWORD type,
                         UWORD flags,
                         LONG spec,
                         int x,
                         int y,
                         int width,
                         int height) {
        const int index = int(_objects.size());
        if (index > 1) {
            _objects.back().ob_next = WORD(index);
            _objects.back().ob_flags &= ~LASTOB;
        }
        _objects.push_back(OBJECT{0,
                                  -1,
                                  -1,
                                  type,
                                  UWORD(flags | LASTOB),
                                  NORMAL,
                                  spec,
                                  WORD(x),
                                  WORD(y),
                                  WORD(width),
                                  WORD(height)});
        if (index == 1)
            _objects[0].ob_head = 1;
        _objects[0].ob_tail = WORD(index);
        return index;
    }

    int dialog_form::box(int x, int y, int width, int height) {
        return add(G_BOX, 0, 0x00011100L, x, y, width, height);
    }

    int dialog_form::choice(
        std::string text, int x, int y, int width, int height) {
        return add(G_STRING,
                   SELECTABLE | EXIT,
                   store(std::move(text)),
                   x,
                   y,
                   width,
                   height);
    }

    int dialog_form::label(
        std::string text, int x, int y, int width, int height) {
        return add(
            G_STRING, 0, store(std::move(text)), x, y, width, height);
    }

    int dialog_form::button(std::string text,
                            int x,
                            int y,
                            int width,
                            int height,
                            bool primary) {
        const bool cancel = text == "Cancel";
        const int index =
            add(G_BUTTON,
                UWORD(SELECTABLE | EXIT | (primary ? DEFAULT : 0)),
                store(std::move(text)),
                x,
                y,
                width,
                height);
        if (primary)
            _primary = index;
        if (cancel)
            _cancel = index;
        return index;
    }

    int dialog_form::entry(char *buffer,
                           int capacity,
                           int x,
                           int y,
                           int width,
                           int height) {
        _editors.push_back(TEDINFO{reinterpret_cast<LONG>(buffer),
                                   store(""),
                                   store("X"),
                                   IBM,
                                   0,
                                   TE_LEFT,
                                   0x1100,
                                   0,
                                   1,
                                   WORD(capacity),
                                   1});
        return add(G_BOXTEXT,
                   EDITABLE,
                   reinterpret_cast<LONG>(&_editors.back()),
                   x,
                   y,
                   width,
                   height);
    }

    void dialog_form::set_label(int object, std::string text) {
        auto &spec = _objects.at(object).ob_spec;
        for (auto &caption : _strings) {
            if (spec != reinterpret_cast<LONG>(caption.data()))
                continue;
            caption = std::move(text);
            spec = reinterpret_cast<LONG>(caption.data());
            return;
        }
    }

    void dialog_form::set_enabled(int object, bool enabled) {
        _objects.at(object).ob_state = enabled ? NORMAL : DISABLED;
    }

    int dialog_form::run(int edit) {
        flush_repaints();
        if (!_started) {
            form_dial(FMD_START,
                      0,
                      0,
                      0,
                      0,
                      _bounds.p.x,
                      _bounds.p.y,
                      _bounds.d.w,
                      _bounds.d.h);
            _started = true;
        }
        for (auto &object : _objects)
            object.ob_state &= ~SELECTED;
        objc_draw(_objects.data(),
                  0,
                  8,
                  _bounds.p.x - 3,
                  _bounds.p.y - 3,
                  _bounds.d.w + 6,
                  _bounds.d.h + 6);
        // The pinned form_do maps Escape to the default command.
        // Dispatch the two commands explicitly, retaining AES editing
        // and tracking.
        WORD caret = 0;
        if (edit)
            objc_edit(_objects.data(), WORD(edit), 0, &caret, EDINIT);
        int selected = -1;
        for (;;) {
            WORD mx = 0, my = 0, buttons = 0, modifiers = 0, key = 0,
                 clicks = 0;
            const WORD events = evnt_multi(MU_BUTTON | MU_KEYBD,
                                           1,
                                           1,
                                           1,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           nullptr,
                                           0,
                                           0,
                                           &mx,
                                           &my,
                                           &buttons,
                                           &modifiers,
                                           &key,
                                           &clicks);
            if (events & MU_KEYBD) {
                const int character = key & 0xff,
                          scan = UWORD(key) >> 8;
                if (character == 27 || (!character && scan == 0x29)) {
                    selected = _cancel;
                    break;
                }
                if (character == 13 || character == 10 ||
                    (!character && (scan == 0x1c || scan == 0x28))) {
                    selected = _primary;
                    break;
                }
                if (edit)
                    objc_edit(_objects.data(),
                              WORD(edit),
                              key,
                              &caret,
                              EDCHAR);
            }
            if (events & MU_BUTTON) {
                const int object =
                    objc_find(_objects.data(), 0, 8, mx, my);
                if (object <= 0 || _objects[object].ob_state & DISABLED)
                    continue;
                if (_objects[object].ob_flags & EDITABLE)
                    continue;
                if (_objects[object].ob_flags & EXIT &&
                    graf_watchbox(_objects.data(),
                                  WORD(object),
                                  SELECTED,
                                  NORMAL)) {
                    selected = object;
                    break;
                }
            }
        }
        if (edit)
            objc_edit(_objects.data(), WORD(edit), 0, &caret, EDEND);
        return selected;
    }

    native::rect dialog_form::bounds() const {
        return _bounds;
    }
} // namespace linux::gemix

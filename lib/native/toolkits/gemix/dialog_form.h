//
// Composes owner-centered dialogs from GEM AES form objects. Native
// objects own field editing and pressed feedback; the adapter
// dispatches modal input.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <deque>
#include <string>
#include <vector>
#include <gem.h>
#include <native/app_wnd.h>

namespace linux::gemix
{
    class dialog_form
    {
    public:
        // Center a form over its owner's client area, within the
        // desktop.
        dialog_form(const native::app_wnd &owner,
                    int width,
                    int height);
        // Release AES form backing and repaint exposed Native windows.
        ~dialog_form();
        // Append a native label, button or boxed editable field.
        // Frame native list rows without giving each row button chrome.
        int box(int x, int y, int width, int height);
        int
        choice(std::string text, int x, int y, int width, int height);
        int
        label(std::string text, int x, int y, int width, int height);
        int button(std::string text,
                   int x,
                   int y,
                   int width,
                   int height,
                   bool primary = false);
        int entry(char *buffer,
                  int capacity,
                  int x,
                  int y,
                  int width,
                  int height);
        // Replace a label/button caption or enable its input.
        void set_label(int object, std::string text);
        void set_enabled(int object, bool enabled);
        // Draw and run the form; return the selected exit object or -1.
        int run(int edit = 0);
        // Return the exact desktop rectangle used for placement.
        native::rect bounds() const;

    private:
        std::vector<OBJECT> _objects;
        std::deque<std::string> _strings;
        std::deque<TEDINFO> _editors;
        native::rect _bounds;
        bool _started = false;
        int _primary = -1, _cancel = -1;
        // Append one direct child and preserve AES sibling linkage.
        int add(UWORD type,
                UWORD flags,
                LONG spec,
                int x,
                int y,
                int width,
                int height);
        // Keep pointer-bearing captions stable for direct and gemd
        // calls.
        LONG store(std::string text);
    };
} // namespace linux::gemix

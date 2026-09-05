//
// Implements an Athena text/menu combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    constexpr unsigned int bitmap_arrow_width = 9;
    constexpr unsigned int bitmap_arrow_height = 5;
    constexpr char bitmap_arrow_bits[] = {
        static_cast<char>(0xff), 0x01,
        static_cast<char>(0xfe), 0x00,
        0x7c, 0x00,
        0x38, 0x00,
        0x10, 0x00
    };

    linux::x11::xaw_combo_box *state(native::combo_box *owner) {
        return linux::x11::combo_box_bindings.object_from_handle(owner);
    }

    void fit_children(linux::x11::xaw_combo_box *binding) {
        Dimension width = 1, height = 1;
        XtVaGetValues(binding->root, XtNwidth, &width, XtNheight, &height, nullptr);
        const int button_width = std::min<int>(width, height + 4);
        const int child_height = std::max(1, int(height) - 2);
        XFontStruct *font = nullptr;
        XtVaGetValues(XawTextGetSink(binding->text), XtNfont, &font, nullptr);
        const int text_height = font ? font->ascent + font->descent : 13;
        const int margin = std::max(0, (child_height - text_height) / 2);
        XtVaSetValues(binding->text,
            XtNwidth, std::max(1, int(width) - button_width - 1),
            XtNtopMargin, margin, XtNbottomMargin, margin,
            XtNheight, child_height, nullptr);
        XtVaSetValues(binding->button,
            XtNwidth, std::max(1, button_width - 2),
            XtNheight, child_height, nullptr);
    }

    void resized(Widget, XtPointer data, XEvent *event, Boolean *) {
        auto *binding = state(static_cast<native::combo_box *>(data));
        if (binding && event && event->type == ConfigureNotify)
            fit_children(binding);
    }

    // MenuButton normally anchors its popup at the arrow. Override only
    // that geometry; SimpleMenu still owns grabs, tracking and dismissal.
    void opening(Widget menu, XtPointer data, XtPointer) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *binding = state(owner);
        if (!binding) return;
        Dimension width = 1, height = 1, popup_height = 1;
        XtVaGetValues(binding->root, XtNwidth, &width, XtNheight, &height, nullptr);
        XtVaGetValues(menu, XtNheight, &popup_height, nullptr);
        WidgetList entries = nullptr;
        Cardinal count = 0;
        XtVaGetValues(menu, XtNchildren, &entries, XtNnumChildren, &count, nullptr);
        for (Cardinal index = 0; index < count; ++index) {
            XFontStruct *font = nullptr;
            char *label = nullptr;
            Dimension left = 0;
            XtVaGetValues(entries[index], XtNfont, &font, XtNlabel, &label,
                XtNleftMargin, &left, nullptr);
            const int text_width = font && label
                ? XTextWidth(font, label, static_cast<int>(std::char_traits<char>::length(label))) : 0;
            XtVaSetValues(entries[index], XtNrightMargin,
                std::max(0, int(width) - 2 - left - text_width), nullptr);
        }
        Position x = 0, y = 0;
        XtTranslateCoords(binding->root, 0, 0, &x, &y);
        const int screen_width = WidthOfScreen(XtScreen(menu));
        const int screen_height = HeightOfScreen(XtScreen(menu));
        const int top = y + height + popup_height + 2 <= screen_height
            ? y + height : y - popup_height - 2;
        XtVaSetValues(menu,
            XtNwidth, std::max(1, int(width) - 2),
            XtNx, std::clamp<int>(x, 0, std::max(0, screen_width - width)),
            XtNy, std::max(0, top), nullptr);
        binding->opening_press = true;
        owner->on_native_drop_down(true);
    }

    // A combo supports click-then-click as well as Athena's press/drag.
    // Releasing the opening click over the field must not dismiss it.
    void menu_release(Widget, XtPointer data, XEvent *event, Boolean *dispatch) {
        auto *binding = state(static_cast<native::combo_box *>(data));
        if (!binding || event->type != ButtonRelease ||
            !binding->opening_press) return;
        binding->opening_press = false;
        Position x = 0, y = 0;
        Dimension width = 1, height = 1;
        XtTranslateCoords(binding->root, 0, 0, &x, &y);
        XtVaGetValues(binding->root, XtNwidth, &width, XtNheight, &height, nullptr);
        if (event->xbutton.x_root >= x && event->xbutton.x_root < x + width &&
            event->xbutton.y_root >= y && event->xbutton.y_root < y + height)
            *dispatch = False;
    }

    void closed(Widget, XtPointer data, XtPointer) {
        static_cast<native::combo_box *>(data)->on_native_drop_down(false);
    }

    void selected(Widget, XtPointer data, XtPointer) {
        auto *callback = static_cast<linux::x11::xaw_combo_callback *>(data);
        if (!callback || !callback->owner) return;
        auto *owner = callback->owner;
        const int index = callback->index;
        if (auto *binding = state(owner))
            XtVaSetValues(binding->text, XtNstring,
                owner->get_items().at(index).c_str(), nullptr);
        owner->on_native_selection(index);
    }

    void text_event(Widget widget, XtPointer data,
                    XEvent *event, Boolean *) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *binding = state(owner);
        if (!owner || !binding || binding->suppress || !event) return;
        if (event->type == ButtonPress && event->xbutton.button == Button1 &&
            owner->get_style() != native::combo_box_style::editable) {
            XtCallActionProc(binding->button, "PopupMenu", event, nullptr, 0);
            return;
        }
        if (event->type != KeyRelease) return;
        char *value = nullptr;
        XtVaGetValues(widget, XtNstring, &value, nullptr);
        owner->on_native_text(value ? value : "");
    }

    void clear_callbacks(linux::x11::xaw_combo_box *binding) {
        for (auto *callback : binding->callbacks) delete callback;
        binding->callbacks.clear();
    }

    void create_menu(native::combo_box *owner,
                     linux::x11::xaw_combo_box *binding) {
        Pixel border = BlackPixelOfScreen(XtScreen(binding->root));
        XtVaGetValues(binding->text, XtNborderColor, &border, nullptr);
        binding->menu = XtVaCreatePopupShell(
            "comboMenu",
            simpleMenuWidgetClass,
            binding->root,
            XtNborderWidth,
            1,
            XtNborderColor, border,
            XtNallowShellResize, False,
            XtNwidth, std::max(1, int(owner->get_dimensions().w) - 2),
            nullptr);
        XtAddCallback(binding->menu, XtNpopupCallback, opening, owner);
        XtAddCallback(binding->menu, XtNpopdownCallback, closed, owner);
        // SimpleMenu tracks BtnMotion by default; a combo remains open
        // after release and must also track ordinary pointer motion.
        XtOverrideTranslations(binding->menu,
            XtParseTranslationTable("<Motion>: highlight()"));
        XtAddEventHandler(binding->menu, ButtonReleaseMask, False, menu_release, owner);
        for (std::size_t index = 0;
             index < owner->get_items().size();
             ++index) {
            Widget entry = XtVaCreateManagedWidget(
                "item",
                smeBSBObjectClass,
                binding->menu,
                XtNlabel,
                owner->get_items()[index].c_str(),
                nullptr);
            auto *callback = new linux::x11::xaw_combo_callback;
            callback->owner = owner;
            callback->index = static_cast<int>(index);
            binding->callbacks.push_back(callback);
            XtAddCallback(entry, XtNcallback, selected, callback);
        }
    }
}

namespace native
{
    void combo_box::apply_items() {
        auto *binding = state(this);
        if (!binding || !binding->root)
            throw std::runtime_error(
                "X11: Missing combo box binding.");
        clear_callbacks(binding);
        if (binding->menu)
            XtDestroyWidget(binding->menu);
        binding->menu = nullptr;
        create_menu(this, binding);
    }

    void combo_box::apply_selected_index() { apply_text(); }

    void combo_box::apply_text() {
        auto *binding = state(this);
        if (!binding || !binding->text)
            throw std::runtime_error("X11: Missing combo box binding.");
        binding->suppress = true;
        XtVaSetValues(binding->text, XtNstring, get_text().c_str(), nullptr);
        binding->suppress = false;
    }

    void combo_box::apply_style() {
        auto *binding = state(this);
        if (binding && binding->text)
            XtVaSetValues(binding->text, XtNeditType,
                get_style() == combo_box_style::editable
                    ? XawtextEdit : XawtextRead, nullptr);
    }

    void combo_box::create_native() {
        Widget parent = linux::x11::parent_widget(this);
        if (!parent || !get_parent()->get_created())
            throw std::runtime_error(
                "X11: combo box requires a created parent.");
        auto *self = this;
        auto *binding = new linux::x11::xaw_combo_box;
        binding->root = XtVaCreateWidget(
            "comboBox", linux::x11::layout_host_class(), parent,
            XtNhorizDistance, _bounds.p.x,
            XtNvertDistance, _bounds.p.y,
            XtNwidth, linux::x11::widget_dimension(_bounds.d.w),
            XtNheight, linux::x11::widget_dimension(_bounds.d.h),
            XtNborderWidth, 0,
            XtNdefaultDistance, 0,
            XtNleft, XtChainLeft,
            XtNright, XtChainLeft,
            XtNtop, XtChainTop,
            XtNbottom, XtChainTop,
            XtNresizable, False,
            nullptr);
        const int button_width = std::min<int>(_bounds.d.w, _bounds.d.h+4);
        const int child_height = std::max(1,
            static_cast<int>(_bounds.d.h)-2);
        const int text_width = std::max(1,
            static_cast<int>(_bounds.d.w)-button_width-1);
        const int arrow_width = std::max(1, button_width-2);
        binding->text = XtVaCreateManagedWidget(
            "comboText", asciiTextWidgetClass, binding->root,
            XtNhorizDistance, 0,
            XtNvertDistance, 0,
            XtNwidth, text_width,
            XtNheight, child_height,
            XtNborderWidth, 1,
            XtNstring, get_text().c_str(),
            XtNeditType, get_style() == combo_box_style::editable
                ? XawtextEdit : XawtextRead,
            XtNleft, XtChainLeft, XtNright, XtChainRight,
            XtNtop, XtChainTop, XtNbottom, XtChainBottom,
            nullptr);
        binding->button = XtVaCreateManagedWidget(
            "comboButton", menuButtonWidgetClass, binding->root,
            XtNfromHoriz, binding->text,
            XtNhorizDistance, -1,
            XtNvertDistance, 0,
            XtNwidth, arrow_width,
            XtNheight, child_height,
            XtNborderWidth, 1,
            XtNlabel, "", XtNmenuName, "comboMenu",
            XtNresize, False,
            XtNleft, XtChainRight, XtNright, XtChainRight,
            XtNtop, XtChainTop, XtNbottom, XtChainBottom,
            nullptr);
        if (linux::x11::cached_display) {
            binding->arrow = XCreateBitmapFromData(
                linux::x11::cached_display,
                RootWindowOfScreen(XtScreen(binding->root)),
                bitmap_arrow_bits,
                bitmap_arrow_width,
                bitmap_arrow_height);
            if (binding->arrow != None)
                XtVaSetValues(binding->button,
                              XtNbitmap,
                              binding->arrow,
                              nullptr);
        }
        create_menu(self, binding);
        if (!binding->root || !binding->text || !binding->button ||
            !binding->menu) {
            if (binding->root) XtDestroyWidget(binding->root);
            clear_callbacks(binding); delete binding;
            throw std::runtime_error("X11: Failed to create combo box.");
        }
        fit_children(binding);
        XtAddEventHandler(binding->text, KeyReleaseMask | ButtonPressMask, False,
                          text_event, self);
        linux::x11::wnd_bindings.register_pair(binding->root, self);
        linux::x11::combo_box_bindings.register_pair(self, binding);
        XtAddEventHandler(binding->root, StructureNotifyMask, False, resized, self);
    }

    void combo_box::show_native() {
        auto *binding = state(this);
        if (!_created || !binding)
            throw std::runtime_error("X11: combo box is not created.");
        XtManageChild(binding->root);
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *binding = state(self);
        linux::x11::combo_box_bindings.unregister_by_handle(self);
        linux::x11::wnd_bindings.unregister_by_object(self);
        if (binding) {
            if (binding->root) XtDestroyWidget(binding->root);
            if (binding->arrow != None &&
                linux::x11::cached_display) {
                XSync(linux::x11::cached_display, False);
                XFreePixmap(linux::x11::cached_display,
                            binding->arrow);
            }
            clear_callbacks(binding); delete binding;
        }
    }
} // namespace native

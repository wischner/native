//
// Implements a native WINGs combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

#include <X11/extensions/shape.h>
#include <X11/Xutil.h>
#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    linux::wmaker::native_combo_box *binding(native::combo_box *owner) {
        return linux::wmaker::combo_box_bindings.object_from_handle(owner);
    }

    void popup_changed(WMWidget *, void *data) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *state = binding(owner);
        if (!owner || !state || state->suppress) return;
        const int index = WMGetPopUpButtonSelectedItem(state->popup);
        if (index >= 0 && state->field) {
            const char *text = WMGetPopUpButtonItem(state->popup, index);
            state->suppress = true;
            WMSetTextFieldText(state->field, text ? text : "");
            state->suppress = false;
        }
        linux::wmaker::defer([owner, index]() {
            if (owner->get_created()) {
                owner->on_native_selection(index);
                owner->on_native_drop_down(false);
            }
        });
    }

    void field_changed(WMTextFieldDelegate *delegate, WMNotification *) {
        auto *owner = delegate
            ? static_cast<native::combo_box *>(delegate->data) : nullptr;
        auto *state = binding(owner);
        if (!owner || !state || state->suppress) return;
        char *value = WMGetTextFieldText(state->field);
        const std::string text = value ? value : "";
        if (value) std::free(value);
        linux::wmaker::defer([owner, text]() {
            if (owner->get_created()) owner->on_native_text(text);
        });
    }

    void draw_editable_arrow(native::combo_box &owner,
                             linux::wmaker::native_combo_box &state) {
        if (owner.get_style() != native::combo_box_style::editable ||
            !state.arrow_overlay || !state.arrow ||
            WMWidgetXID(state.arrow) == None) {
            return;
        }
        const int width = std::max(
            1, static_cast<int>(WMWidgetWidth(state.arrow)));
        const int height = std::max(
            1, static_cast<int>(WMWidgetHeight(state.arrow)));
        const Window target = WMWidgetXID(state.arrow);
        auto *screen = reinterpret_cast<W_Screen *>(
            linux::wmaker::screen);
        if (!screen)
            return;
        if (width > 4 && height > 4) {
            XFillRectangle(linux::wmaker::display,
                           target,
                           WMColorGC(screen->gray),
                           2,
                           2,
                           static_cast<unsigned int>(width - 4),
                           static_cast<unsigned int>(height - 4));
        }
        W_DrawRelief(screen,
                     target,
                     0,
                     0,
                     static_cast<unsigned int>(width),
                     static_cast<unsigned int>(height),
                     state.arrow_pressed ? WRSunken : WRRaised);
        const int center_x = width / 2;
        const int center_y = height / 2;
        XPoint arrow[] = {
            {static_cast<short>(center_x - 3),
             static_cast<short>(center_y - 2)},
            {static_cast<short>(center_x + 3),
             static_cast<short>(center_y - 2)},
            {static_cast<short>(center_x),
             static_cast<short>(center_y + 2)}};
        XFillPolygon(linux::wmaker::display,
                     target,
                     WMColorGC(screen->black),
                     arrow,
                     3,
                     Convex,
                     CoordModeOrigin);
    }

    void poll_arrow_release(void *data) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *state = binding(owner);
        if (!owner || !state)
            return;
        state->arrow_timer = nullptr;
        Window root = None;
        Window child = None;
        int root_x = 0;
        int root_y = 0;
        int child_x = 0;
        int child_y = 0;
        unsigned int buttons = 0;
        const Bool queried = XQueryPointer(
            linux::wmaker::display,
            WMWidgetXID(state->popup),
            &root,
            &child,
            &root_x,
            &root_y,
            &child_x,
            &child_y,
            &buttons);
        if (queried && (buttons & Button1Mask) != 0) {
            state->arrow_timer = WMAddTimerHandler(
                16, poll_arrow_release, owner);
            return;
        }
        state->arrow_pressed = false;
        draw_editable_arrow(*owner, *state);
    }

    void paint_editable_arrow(XEvent *event, void *data) {
        auto *owner = static_cast<native::combo_box *>(data);
        auto *state = binding(owner);
        if (!event || !owner || !state ||
            owner->get_style() != native::combo_box_style::editable) {
            return;
        }
        if (event->type == Expose) {
            if (event->xexpose.count != 0)
                return;
        } else if (event->type == ButtonPress) {
            if (event->xbutton.button != Button1)
                return;
            state->arrow_pressed = true;
            if (!state->arrow_timer) {
                state->arrow_timer = WMAddTimerHandler(
                    16, poll_arrow_release, owner);
            }
        } else {
            return;
        }
        draw_editable_arrow(*owner, *state);
    }

    void replace(linux::wmaker::native_combo_box *state,
                 const std::vector<std::string> &items) {
        while (WMGetPopUpButtonNumberOfItems(state->popup) > 0)
            WMRemovePopUpButtonItem(state->popup, 0);
        for (const auto &item : items)
            WMAddPopUpButtonItem(state->popup, item.c_str());
    }
}

namespace linux::wmaker
{
    void configure_combo_box(native::combo_box &owner,
                             native_combo_box &state) {
        const native::size dimensions = owner.get_dimensions();
        const bool editable = owner.get_style() ==
            native::combo_box_style::editable;
        const int width = std::max(1, static_cast<int>(dimensions.w));
        const int height = std::max(1, static_cast<int>(dimensions.h));
        const int arrow_inset = editable && width > 4 && height > 4
                                    ? 2
                                    : 0;
        const int arrow_width = std::min(
            std::max(1, width - 2 * arrow_inset),
            std::max(1, height - 2 * arrow_inset));
        const Window popup_window = WMWidgetXID(state.popup);
        const Window arrow_window = state.arrow
                                        ? WMWidgetXID(state.arrow)
                                        : None;
        int shape_event = 0;
        int shape_error = 0;
        const bool shape_available =
            popup_window != None &&
            XShapeQueryExtension(display, &shape_event, &shape_error);
        state.arrow_overlay = editable && shape_available &&
                              arrow_window != None;
        WMResizeWidget(state.field,
                       static_cast<unsigned int>(width),
                       static_cast<unsigned int>(height));
        WMMoveWidget(state.popup,
                     editable && !shape_available
                         ? width - arrow_width - arrow_inset
                         : 0,
                     editable && !shape_available ? arrow_inset : 0);
        WMResizeWidget(state.popup,
                       static_cast<unsigned int>(
                           editable && !shape_available
                               ? arrow_width
                               : width),
                       static_cast<unsigned int>(
                           editable && !shape_available
                               ? std::max(1,
                                          height - 2 * arrow_inset)
                               : height));
        if (state.arrow) {
            WMMoveWidget(state.arrow,
                         width - arrow_width - arrow_inset,
                         arrow_inset);
            WMResizeWidget(state.arrow,
                           static_cast<unsigned int>(arrow_width),
                           static_cast<unsigned int>(
                               std::max(1, height - 2 * arrow_inset)));
        }
        if (popup_window != None) {
            if (editable && shape_available) {
                XRectangle arrow = {
                    static_cast<short>(
                        width - arrow_width - arrow_inset),
                    static_cast<short>(arrow_inset),
                    static_cast<unsigned short>(arrow_width),
                    static_cast<unsigned short>(
                        std::max(1, height - 2 * arrow_inset))};
                XShapeCombineRectangles(display,
                                        popup_window,
                                        ShapeBounding,
                                        0,
                                        0,
                                        &arrow,
                                        1,
                                        ShapeSet,
                                        YXBanded);
                XShapeCombineRectangles(display,
                                        popup_window,
                                        ShapeInput,
                                        0,
                                        0,
                                        &arrow,
                                        1,
                                        ShapeSet,
                                        YXBanded);
            } else {
                XShapeCombineMask(display,
                                  popup_window,
                                  ShapeBounding,
                                  0,
                                  0,
                                  None,
                                  ShapeSet);
                XShapeCombineMask(display,
                                  popup_window,
                                  ShapeInput,
                                  0,
                                  0,
                                  None,
                                  ShapeSet);
            }
        }
        if (state.arrow_overlay) {
            Region empty = XCreateRegion();
            XShapeCombineRegion(display,
                                arrow_window,
                                ShapeInput,
                                0,
                                0,
                                empty,
                                ShapeSet);
            XDestroyRegion(empty);
        }
        WMSetPopUpButtonPullsDown(
            state.popup, editable ? True : False);
        if (editable && WMWidgetXID(state.field) != None) {
            WMRaiseWidget(state.field);
            WMRaiseWidget(state.popup);
            if (state.arrow_overlay)
                WMRaiseWidget(state.arrow);
        }
    }
} // namespace linux::wmaker

namespace native
{
    void combo_box::apply_items() {
        auto *state = binding(this);
        if (!state) throw std::runtime_error(
            "Window Maker/WINGs: missing combo box binding.");
        state->suppress = true;
        replace(state, get_items());
        state->suppress = false;
    }

    void combo_box::apply_selected_index() {
        auto *state = binding(this);
        if (!state) throw std::runtime_error(
            "Window Maker/WINGs: missing combo box binding.");
        state->suppress = true;
        WMSetPopUpButtonSelectedItem(state->popup, get_selected_index());
        state->suppress = false;
    }

    void combo_box::apply_text() {
        auto *state = binding(this);
        if (!state) throw std::runtime_error(
            "Window Maker/WINGs: missing combo box binding.");
        state->suppress = true;
        WMSetPopUpButtonText(state->popup, get_text().c_str());
        if (state->field)
            WMSetTextFieldText(state->field, get_text().c_str());
        state->suppress = false;
    }

    void combo_box::apply_style() {
        auto *state = binding(this);
        if (!state || !state->frame || !state->field || !state->popup ||
            !state->arrow)
            throw std::runtime_error(
                "Window Maker/WINGs: missing combo box binding.");
        const bool editable =
            get_style() == combo_box_style::editable;
        linux::wmaker::configure_combo_box(*this, *state);

        if (WMWidgetXID(state->frame) != None) {
            if (editable) {
                WMRealizeWidget(state->field);
                WMMapWidget(state->field);
                if (state->arrow_overlay) {
                    WMMapWidget(state->arrow);
                    WMRaiseWidget(state->arrow);
                }
            } else {
                WMUnmapWidget(state->field);
                WMUnmapWidget(state->arrow);
            }
        }
    }

    void combo_box::create_native() {
        auto *self = this;
        auto *state = new linux::wmaker::native_combo_box;
        state->frame = WMCreateFrame(linux::wmaker::parent_widget(self));
        state->popup = WMCreatePopUpButton(state->frame);
        if (!state->frame || !state->popup) {
            if (state->frame) WMDestroyWidget(state->frame);
            delete state;
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create combo box.");
        }
        WMSetFrameRelief(state->frame, WRFlat);
        const point position = linux::wmaker::control_position(self);
        WMMoveWidget(state->frame, position.x, position.y);
        WMResizeWidget(state->frame, _bounds.d.w, _bounds.d.h);
        state->field = WMCreateTextField(state->frame);
        state->arrow = WMCreateFrame(state->frame);
        if (!state->field || !state->arrow) {
            WMDestroyWidget(state->frame);
            delete state;
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create combo box.");
        }
        WMSetFrameRelief(state->arrow, WRFlat);
        state->delegate.data = self;
        state->delegate.didChange = field_changed;
        WMSetTextFieldDelegate(state->field, &state->delegate);
        WMSetTextFieldText(state->field, get_text().c_str());
        WMCreateEventHandler(WMWidgetView(state->popup),
                             ButtonPressMask,
                             paint_editable_arrow,
                             self);
        WMCreateEventHandler(WMWidgetView(state->arrow),
                             ExposureMask,
                             paint_editable_arrow,
                             self);
        linux::wmaker::configure_combo_box(*self, *state);
        WMSetPopUpButtonAction(state->popup, popup_changed, self);
        replace(state, get_items());
        if (get_selected_index() >= 0)
            WMSetPopUpButtonSelectedItem(state->popup, get_selected_index());
        linux::wmaker::wnd_bindings.register_pair(state->frame, self);
        linux::wmaker::combo_box_bindings.register_pair(self, state);
    }

    void combo_box::show_native() {
        auto *state = binding(this);
        if (!_created || !state)
            throw std::runtime_error(
                "Window Maker/WINGs: combo box is not created.");
        WMRealizeWidget(state->frame);
        linux::wmaker::configure_combo_box(*this, *state);
        WMMapSubwidgets(state->frame);
        if (get_style() != combo_box_style::editable) {
            WMUnmapWidget(state->field);
            WMUnmapWidget(state->arrow);
        } else {
            WMRaiseWidget(state->field);
            WMRaiseWidget(state->popup);
            if (state->arrow_overlay)
                WMRaiseWidget(state->arrow);
        }
        WMMapWidget(state->frame);
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = binding(self);
        if (state && state->arrow_timer) {
            WMDeleteTimerHandler(state->arrow_timer);
            state->arrow_timer = nullptr;
        }
        linux::wmaker::combo_box_bindings.unregister_by_handle(self);
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (state && state->frame) WMDestroyWidget(state->frame);
        delete state;
    }
} // namespace native

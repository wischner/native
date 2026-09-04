//
// Implements icon_view with Motif's native spatial XmContainer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <Xm/Container.h>
#include <Xm/IconG.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/Xm.h>

#include <native.h>

#include "../../platforms/linux/x_image.h"
#include "globals.h"

namespace
{
    using collection_state = linux::openmotif::motif_collection;

    collection_state &binding_for(native::icon_view &view) {
        auto *state = linux::openmotif::icon_view_bindings
                          .object_from_handle(&view);
        if (!state || !state->widget || !state->content)
            throw std::runtime_error(
                "Motif: missing native icon_view binding.");
        return *state;
    }

    void free_pixmaps(collection_state &state) {
        for (Pixmap pixmap : state.pixmaps) {
            if (pixmap != XmUNSPECIFIED_PIXMAP && pixmap != None)
                XFreePixmap(linux::openmotif::cached_display, pixmap);
        }
        state.pixmaps.clear();
    }

    void clear_native_items(collection_state &state) {
        for (Widget item : state.items) {
            XtVaSetValues(item,
                          XmNlargeIconPixmap,
                          XmUNSPECIFIED_PIXMAP,
                          XmNsmallIconPixmap,
                          XmUNSPECIFIED_PIXMAP,
                          nullptr);
        }
        if (!state.items.empty() && linux::openmotif::cached_display)
            XSync(linux::openmotif::cached_display, False);
        for (Widget item : state.items)
            XtDestroyWidget(item);
        state.items.clear();
        free_pixmaps(state);
    }

    Pixmap create_icon(collection_state &state,
                       const native::img *image,
                       native::size dimensions) {
        if (!image || !dimensions.w || !dimensions.h)
            return XmUNSPECIFIED_PIXMAP;
        Display *display = linux::openmotif::cached_display;
        Screen *screen = XtScreen(state.content);
        Pixmap pixmap = XCreatePixmap(
            display,
            RootWindowOfScreen(screen),
            dimensions.w,
            dimensions.h,
            DefaultDepthOfScreen(screen));
        if (pixmap == None)
            return XmUNSPECIFIED_PIXMAP;
        Pixel background = 0;
        XtVaGetValues(state.content,
                      XmNbackground,
                      &background,
                      nullptr);
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XSetForeground(display, gc, background);
        XFillRectangle(display,
                       pixmap,
                       gc,
                       0,
                       0,
                       dimensions.w,
                       dimensions.h);
        native::detail::blend_x_image(
            display,
            pixmap,
            gc,
            *image,
            {0, 0},
            native::rect(0, 0, dimensions.w, dimensions.h),
            dimensions);
        XFreeGC(display, gc);
        state.pixmaps.push_back(pixmap);
        return pixmap;
    }

    int index_for(collection_state &state, Widget item) {
        const auto found = std::find(
            state.items.begin(), state.items.end(), item);
        return found == state.items.end()
                   ? -1
                   : static_cast<int>(found - state.items.begin());
    }

    void selection_changed(Widget,
                           XtPointer client_data,
                           XtPointer call_data) {
        auto *view = static_cast<native::icon_view *>(client_data);
        if (!view || !call_data)
            return;
        auto &state = binding_for(*view);
        if (state.suppress)
            return;
        auto *selection =
            static_cast<XmContainerSelectCallbackStruct *>(call_data);
        const int index = selection->selected_item_count > 0
                              ? index_for(state,
                                          selection->selected_items[0])
                              : -1;
        view->on_native_selection(index);
    }

    void default_action(Widget,
                        XtPointer client_data,
                        XtPointer call_data) {
        auto *view = static_cast<native::icon_view *>(client_data);
        if (!view || !call_data)
            return;
        auto &state = binding_for(*view);
        auto *selection =
            static_cast<XmContainerSelectCallbackStruct *>(call_data);
        if (selection->selected_item_count <= 0)
            return;
        view->on_native_activate(
            index_for(state, selection->selected_items[0]));
    }

    void scroll_changed(Widget,
                        XtPointer client_data,
                        XtPointer call_data) {
        auto *view = static_cast<native::icon_view *>(client_data);
        if (!view || !call_data)
            return;
        auto &state = binding_for(*view);
        if (state.suppress)
            return;
        auto *scroll = static_cast<XmScrollBarCallbackStruct *>(call_data);
        view->on_native_scroll(
            scroll->value-view->get_scroll_offset());
    }

    void connect_scrollbar(native::icon_view &view,
                           collection_state &state) {
        if (state.native_scroll_callbacks)
            return;
        XtVaGetValues(state.widget,
                      XmNverticalScrollBar,
                      &state.vertical_scrollbar,
                      nullptr);
        if (!state.vertical_scrollbar)
            return;
        XtAddCallback(state.vertical_scrollbar,
                      XmNvalueChangedCallback,
                      scroll_changed,
                      &view);
        XtAddCallback(state.vertical_scrollbar,
                      XmNdragCallback,
                      scroll_changed,
                      &view);
        state.native_scroll_callbacks = true;
    }

    unsigned char view_type(const native::icon_view &view) {
        return view.get_label_mode() ==
                       native::icon_view_label_mode::beside
                   ? XmSMALL_ICON
                   : XmLARGE_ICON;
    }

    void rebuild(native::icon_view &view) {
        auto &state = binding_for(view);
        state.suppress = true;
        clear_native_items(state);
        const unsigned char type = view_type(view);
        XtVaSetValues(state.content,
                      XmNentryViewType,
                      type,
                      nullptr);
        const auto &items = view.get_items();
        for (std::size_t index = 0; index < items.size(); ++index) {
            const native::icon_view_item &item = items[index];
            const char *label =
                view.get_label_mode() ==
                        native::icon_view_label_mode::hidden
                    ? ""
                    : item.text.c_str();
            XmString text = XmStringCreateLocalized(
                const_cast<char *>(label));
            const Pixmap icon = create_icon(
                state, item.image.get(), view.get_icon_size());
            Widget native_item = XmVaCreateManagedIconGadget(
                state.content,
                const_cast<char *>("iconItem"),
                XmNlabelString,
                text,
                XmNviewType,
                type,
                XmNlargeIconPixmap,
                icon,
                XmNsmallIconPixmap,
                icon,
                XmNpositionIndex,
                static_cast<int>(index),
                XmNsensitive,
                item.enabled ? True : False,
                nullptr);
            XmStringFree(text);
            state.items.push_back(native_item);
        }
        XmContainerRelayout(state.content);
        state.suppress = false;
    }

    Widget create_widget(native::icon_view &view,
                         collection_state &state) {
        native::wnd *parent = view.get_parent();
        Widget parent_widget = linux::openmotif::parent_widget(&view);
        if (!parent || !parent->get_created() || !parent_widget)
            throw std::runtime_error(
                "Motif: icon_view requires a created parent.");
        const native::rect bounds = view.get_bounds();
        Widget scroller = XtVaCreateWidget(
            "iconScroll",
            xmScrolledWindowWidgetClass,
            parent_widget,
            XmNx,
            bounds.p.x,
            XmNy,
            bounds.p.y,
            XmNwidth,
            bounds.d.w,
            XmNheight,
            bounds.d.h,
            XmNscrollingPolicy,
            XmAUTOMATIC,
            nullptr);
        state.content = XmVaCreateManagedContainer(
            scroller,
            const_cast<char *>("icons"),
            XmNlayoutType,
            XmSPATIAL,
            XmNspatialStyle,
            XmGRID,
            XmNentryViewType,
            view_type(view),
            XmNselectionPolicy,
            XmSINGLE_SELECT,
            XmNnavigationType,
            XmTAB_GROUP,
            nullptr);
        XtAddCallback(state.content,
                      XmNselectionCallback,
                      selection_changed,
                      &view);
        XtAddCallback(state.content,
                      XmNdefaultActionCallback,
                      default_action,
                      &view);
        linux::openmotif::wnd_bindings.register_pair(scroller, &view);
        return scroller;
    }
} // namespace

namespace native
{
    void icon_view::apply_items() {
        rebuild(*this);
        apply_selected_index();
    }

    void icon_view::apply_icon_size() {
        rebuild(*this);
        apply_selected_index();
    }

    void icon_view::apply_label_mode() {
        rebuild(*this);
        apply_selected_index();
    }

    void icon_view::apply_selected_index() {
        auto &state = binding_for(*this);
        if (state.suppress)
            return;
        state.suppress = true;
        for (std::size_t index = 0;
             index < state.items.size();
             ++index) {
            XtVaSetValues(
                state.items[index],
                XmNvisualEmphasis,
                static_cast<int>(index) == get_selected_index()
                    ? XmSELECTED
                    : XmNOT_SELECTED,
                nullptr);
        }
        state.suppress = false;
    }

    void icon_view::apply_scroll_offset() {
        auto &state = binding_for(*this);
        connect_scrollbar(*this, state);
        Widget scrollbar = state.vertical_scrollbar;
        if (!scrollbar)
            return;
        int maximum = 0;
        int slider = 1;
        int increment = 1;
        int page = 1;
        XtVaGetValues(scrollbar,
                      XmNmaximum,
                      &maximum,
                      XmNsliderSize,
                      &slider,
                      XmNincrement,
                      &increment,
                      XmNpageIncrement,
                      &page,
                      nullptr);
        const int value = std::clamp(
            get_scroll_offset(), 0, std::max(0, maximum-slider));
        state.suppress = true;
        XmScrollBarSetValues(scrollbar,
                             value,
                             slider,
                             increment,
                             page,
                             False);
        state.suppress = false;
    }

    void icon_view::create_native() {
        auto *self = this;
        auto *state = new collection_state();
        state->content = nullptr;
        state->widget = create_widget(*self, *state);
        linux::openmotif::icon_view_bindings.register_pair(self, state);
        self->synchronize_theme_metrics();
        rebuild(*self);
        self->apply_selected_index();
    }

    void icon_view::show_native() {
        auto *state = linux::openmotif::icon_view_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state || !state->widget)
            throw std::runtime_error(
                "Motif: icon_view is not created.");
        XtManageChild(state->widget);
        connect_scrollbar(
            *this, *state);
        this->apply_scroll_offset();
    }

    void icon_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::openmotif::icon_view_bindings
                          .object_from_handle(self);
        if (state) {
            clear_native_items(*state);
            if (state->widget) {
                linux::openmotif::wnd_bindings.unregister_by_handle(
                    state->widget);
                XtDestroyWidget(state->widget);
            }
            linux::openmotif::icon_view_bindings
                .unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

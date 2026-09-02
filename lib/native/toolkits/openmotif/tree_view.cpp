//
// Implements tree_view with the native Motif XmContainer outline view.
// XmIconGadget entries retain toolkit disclosure, selection, keyboard,
// focus, scrolling, and disabled-state behavior.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <functional>
#include <stdexcept>

#include <Xm/Container.h>
#include <Xm/IconG.h>
#include <Xm/ScrolledW.h>
#include <Xm/Xm.h>

#include <native.h>

#include "../../platforms/linux/x_image.h"
#include "collection_host.h"
#include "globals.h"

namespace
{
    using collection_state = linux::openmotif::motif_collection;
    constexpr unsigned int infolib_disclosure_side = 13;
    constexpr short infolib_disclosure_inset = 3;

    collection_state &binding_for(native::tree_view &tree) {
        auto *state = linux::openmotif::tree_view_bindings
                          .object_from_handle(&tree);
        if (!state || !state->widget)
            throw std::runtime_error(
                "Motif: missing tree_view binding.");
        return *state;
    }

    void clear_items(collection_state &state) {
        // XmIconGadget keeps using its icon pixmap while destruction and
        // relayout expose work is pending.  Detach every pixmap before the
        // gadget is destroyed; otherwise a subsequent presentation switch
        // can reuse the XID and Motif issues X_CopyArea against an already
        // freed drawable.
        for (Widget item : state.items) {
            XtVaSetValues(item,
                          XmNsmallIconPixmap,
                          XmUNSPECIFIED_PIXMAP,
                          nullptr);
        }
        if (!state.items.empty() && linux::openmotif::cached_display)
            XSync(linux::openmotif::cached_display, False);
        for (Widget item : state.items)
            XtDestroyWidget(item);
        state.items.clear();
        state.tree_ids.clear();
        if (linux::openmotif::cached_display)
            XSync(linux::openmotif::cached_display, False);
        for (Pixmap pixmap : state.pixmaps) {
            if (pixmap != XmUNSPECIFIED_PIXMAP)
                XFreePixmap(linux::openmotif::cached_display, pixmap);
        }
        state.pixmaps.clear();
    }

    void clear_disclosure_pixmaps(collection_state &state) {
        // The container does not own these pixmaps.  Remove the resource
        // references before releasing them so delayed redraws cannot copy
        // from stale drawable IDs during a native/3-D mode transition.
        if (state.content && state.native_tree) {
            XtVaSetValues(state.content,
                          XmNcollapsedStatePixmap,
                          XmUNSPECIFIED_PIXMAP,
                          XmNexpandedStatePixmap,
                          XmUNSPECIFIED_PIXMAP,
                          nullptr);
            if (linux::openmotif::cached_display)
                XSync(linux::openmotif::cached_display, False);
        }
        const auto release = [](Pixmap &pixmap) {
            if (pixmap != XmUNSPECIFIED_PIXMAP && pixmap != None)
                XFreePixmap(linux::openmotif::cached_display, pixmap);
            pixmap = XmUNSPECIFIED_PIXMAP;
        };
        release(state.collapsed_tree_pixmap);
        release(state.expanded_tree_pixmap);
    }

    Pixmap create_disclosure_pixmap(Widget reference, bool expanded) {
        Display *display = linux::openmotif::cached_display;
        if (!display || !reference)
            return XmUNSPECIFIED_PIXMAP;
        Pixel background = 0;
        Pixel foreground = 0;
        XtVaGetValues(reference,
                      XmNbackground,
                      &background,
                      XmNforeground,
                      &foreground,
                      nullptr);
        Pixmap pixmap = XCreatePixmap(
            display,
            RootWindowOfScreen(XtScreen(reference)),
            infolib_disclosure_side,
            infolib_disclosure_side,
            DefaultDepthOfScreen(XtScreen(reference)));
        if (pixmap == None)
            return XmUNSPECIFIED_PIXMAP;
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XSetForeground(display, gc, background);
        XFillRectangle(display,
                       pixmap,
                       gc,
                       0,
                       0,
                       infolib_disclosure_side,
                       infolib_disclosure_side);
        XPoint points[3];
        if (expanded) {
            points[0] = {0, infolib_disclosure_inset};
            points[1] = {
                static_cast<short>(infolib_disclosure_side - 1),
                infolib_disclosure_inset};
            points[2] = {
                static_cast<short>(infolib_disclosure_side / 2),
                static_cast<short>(infolib_disclosure_side - 1 -
                                   infolib_disclosure_inset)};
        } else {
            points[0] = {infolib_disclosure_inset, 0};
            points[1] = {
                static_cast<short>(infolib_disclosure_side - 1 -
                                   infolib_disclosure_inset),
                static_cast<short>(infolib_disclosure_side / 2)};
            points[2] = {
                infolib_disclosure_inset,
                static_cast<short>(infolib_disclosure_side - 1)};
        }
        XSetForeground(display, gc, foreground);
        XFillPolygon(display, pixmap, gc, points, 3, Convex,
                     CoordModeOrigin);
        XFreeGC(display, gc);
        return pixmap;
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

    Widget widget_for(collection_state &state,
                      native::tree_item_id id) {
        const auto found = std::find(state.tree_ids.begin(),
                                     state.tree_ids.end(),
                                     id);
        return found == state.tree_ids.end()
                   ? nullptr
                   : state.items[static_cast<std::size_t>(
                         found - state.tree_ids.begin())];
    }

    native::tree_item_id id_for(collection_state &state,
                                Widget item) {
        const auto found = std::find(state.items.begin(),
                                     state.items.end(),
                                     item);
        return found == state.items.end()
                   ? native::invalid_tree_item_id
                   : state.tree_ids[static_cast<std::size_t>(
                         found - state.items.begin())];
    }

    void selection_changed(Widget,
                           XtPointer client_data,
                           XtPointer call_data) {
        auto *tree = static_cast<native::tree_view *>(client_data);
        if (!tree || !call_data)
            return;
        auto &state = binding_for(*tree);
        if (state.suppress)
            return;
        auto *selected =
            static_cast<XmContainerSelectCallbackStruct *>(call_data);
        native::tree_item_id id = native::invalid_tree_item_id;
        if (selected->selected_item_count > 0)
            id = id_for(state, selected->selected_items[0]);
        state.suppress = true;
        tree->on_native_selection(id);
        state.suppress = false;
    }

    void default_action(Widget,
                        XtPointer client_data,
                        XtPointer call_data) {
        auto *tree = static_cast<native::tree_view *>(client_data);
        if (!tree || !call_data)
            return;
        auto &state = binding_for(*tree);
        auto *selected =
            static_cast<XmContainerSelectCallbackStruct *>(call_data);
        if (selected->selected_item_count <= 0)
            return;
        const native::tree_item_id id =
            id_for(state, selected->selected_items[0]);
        if (id == native::invalid_tree_item_id)
            return;
        if (selected->event &&
            (selected->event->type == ButtonPress ||
             selected->event->type == ButtonRelease)) {
            tree->on_native_double_click(id);
        } else {
            tree->on_native_activate(id);
        }
    }

    void outline_changed(Widget,
                         XtPointer client_data,
                         XtPointer call_data) {
        auto *tree = static_cast<native::tree_view *>(client_data);
        if (!tree || !call_data)
            return;
        auto &state = binding_for(*tree);
        if (state.suppress)
            return;
        auto *outline =
            static_cast<XmContainerOutlineCallbackStruct *>(call_data);
        const native::tree_item_id id = id_for(state, outline->item);
        if (id == native::invalid_tree_item_id)
            return;
        state.suppress = true;
        tree->on_native_expansion(
            id, outline->new_outline_state == XmEXPANDED);
        state.suppress = false;
    }

    void rebuild(native::tree_view &tree) {
        auto &state = binding_for(tree);
        state.suppress = true;
        clear_items(state);
        std::function<void(const std::vector<native::tree_view_item> &,
                           Widget)> append;
        append = [&tree, &state, &append](
                     const std::vector<native::tree_view_item> &items,
                     Widget parent) {
            int position = 0;
            for (const native::tree_view_item &item : items) {
                XmString label = XmStringCreateLocalized(
                    const_cast<char *>(item.text.c_str()));
                const Pixmap icon = create_icon(
                    state, item.image.get(), tree.get_icon_size());
                const bool raised = tree.get_presentation() ==
                    native::tree_view_presentation::three_dimensional;
                Widget native_item = XmVaCreateManagedIconGadget(
                    state.content,
                    const_cast<char *>("treeItem"),
                    XmNlabelString,
                    label,
                    XmNviewType,
                    XmSMALL_ICON,
                    XmNsmallIconPixmap,
                    icon,
                    XmNentryParent,
                    parent,
                    XmNpositionIndex,
                    position++,
                    XmNoutlineState,
                    item.expanded ? XmEXPANDED : XmCOLLAPSED,
                    XmNheight,
                    tree.get_row_bounds(0).d.h,
                    XmNmarginHeight,
                    0,
                    XmNmarginWidth,
                    2,
                    XmNspacing,
                    5,
                    XmNshadowThickness,
                    raised ? 1 : 0,
                    XmNhighlightThickness,
                    raised ? 1 : 0,
                    XmNsensitive,
                    item.enabled ? True : False,
                    nullptr);
                XmStringFree(label);
                state.items.push_back(native_item);
                state.tree_ids.push_back(item.id);
                append(item.children, native_item);
            }
        };
        append(tree.get_items(), nullptr);
        XmContainerRelayout(state.content);
        state.suppress = false;
    }

    Widget create_native_tree(
        native::tree_view &tree,
        collection_state &state) {
        native::wnd *parent = tree.get_parent();
        Widget parent_widget = linux::openmotif::parent_widget(&tree);
        if (!parent || !parent->get_created() || !parent_widget)
            throw std::runtime_error(
                "Motif: tree_view requires a created parent.");
        const native::rect bounds = tree.get_bounds();
        Widget scroller = XtVaCreateWidget(
            "treeScroll",
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
            const_cast<char *>("tree"),
            XmNlayoutType,
            XmOUTLINE,
            XmNentryViewType,
            XmSMALL_ICON,
            XmNoutlineButtonPolicy,
            XmOUTLINE_BUTTON_PRESENT,
            XmNoutlineLineStyle,
            tree.get_lines_visible()
                ? static_cast<unsigned char>(XmSINGLE)
                : static_cast<unsigned char>(XmNO_LINE),
            XmNoutlineIndentation,
            17,
            XmNselectionPolicy,
            XmSINGLE_SELECT,
            XmNnavigationType,
            XmTAB_GROUP,
            nullptr);
        state.collapsed_tree_pixmap = create_disclosure_pixmap(
            state.content, false);
        state.expanded_tree_pixmap = create_disclosure_pixmap(
            state.content, true);
        XtVaSetValues(
            state.content,
            XmNcollapsedStatePixmap,
            state.collapsed_tree_pixmap,
            XmNexpandedStatePixmap,
            state.expanded_tree_pixmap,
            nullptr);
        state.native_tree = true;
        XtAddCallback(state.content,
                      XmNselectionCallback,
                      selection_changed,
                      &tree);
        XtAddCallback(state.content,
                      XmNdefaultActionCallback,
                      default_action,
                      &tree);
        XtAddCallback(state.content,
                      XmNoutlineChangedCallback,
                      outline_changed,
                      &tree);
        linux::openmotif::wnd_bindings.register_pair(scroller, &tree);
        return scroller;
    }

    Widget create_tree_widget(native::tree_view &tree,
                              collection_state &state) {
        return create_native_tree(tree, state);
    }

    void destroy_tree_widget(collection_state &state) {
        clear_items(state);
        clear_disclosure_pixmaps(state);
        linux::openmotif::destroy_collection_scrollbars(state);
        if (state.widget) {
            linux::openmotif::wnd_bindings.unregister_by_handle(
                state.widget);
            XtDestroyWidget(state.widget);
        }
        state.widget = nullptr;
        state.content = nullptr;
    }

} // namespace

namespace native
{
    void tree_view::apply_items() {
        auto &state = binding_for(*this);
        XtVaSetValues(
            state.content,
            XmNoutlineLineStyle,
            get_lines_visible()
                ? static_cast<unsigned char>(XmSINGLE)
                : static_cast<unsigned char>(XmNO_LINE),
            nullptr);
        rebuild(*this);
        apply_selection();
    }

    void tree_view::apply_selection() {
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
                state.tree_ids[index] == _selected_item
                    ? XmSELECTED
                    : XmNOT_SELECTED,
                nullptr);
        }
        state.suppress = false;
    }

    void tree_view::apply_expansion(tree_item_id id, bool expanded) {
        auto &state = binding_for(*this);
        if (state.suppress)
            return;
        Widget item = widget_for(state, id);
        if (!item)
            return;
        state.suppress = true;
        XtVaSetValues(item,
                      XmNoutlineState,
                      expanded ? XmEXPANDED : XmCOLLAPSED,
                      nullptr);
        state.suppress = false;
    }

    void tree_view::apply_scroll_offset() {
        auto &state = binding_for(*this);
        if (get_visible_item_count() == 0)
            return;
        const int row_height = std::max<int>(
            1, get_row_bounds(0).d.h);
        const std::size_t index = std::min(
            get_visible_item_count() - 1,
            static_cast<std::size_t>(
                std::max(0, _scroll_offset) / row_height));
        Widget item = widget_for(
            state, get_visible_item(index).id);
        if (item)
            XmScrollVisible(state.widget, item, 0, 0);
    }

    void tree_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *state = new collection_state();
        state->widget = create_tree_widget(*self, *state);
        linux::openmotif::tree_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        rebuild(*self);
        self->apply_selection();
        self->on_native_create();
    }

    void tree_view::show() const {
        auto *state = linux::openmotif::tree_view_bindings
                          .object_from_handle(
                              const_cast<tree_view *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("Motif: tree_view is not created.");
        XtManageChild(state->widget);
    }

    void tree_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *state = linux::openmotif::tree_view_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        if (state) {
            destroy_tree_widget(*state);
            linux::openmotif::tree_view_bindings
                .unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

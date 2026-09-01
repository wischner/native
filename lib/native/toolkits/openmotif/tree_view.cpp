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
#include "globals.h"

namespace
{
    using collection_state = linux::openmotif::motif_collection;

    collection_state &binding_for(native::tree_view &tree) {
        auto *state = linux::openmotif::tree_view_bindings
                          .object_from_handle(&tree);
        if (!state || !state->widget || !state->content)
            throw std::runtime_error(
                "Motif: missing tree_view binding.");
        return *state;
    }

    void clear_items(collection_state &state) {
        for (Widget item : state.items)
            XtDestroyWidget(item);
        state.items.clear();
        state.tree_ids.clear();
        for (Pixmap pixmap : state.pixmaps) {
            if (pixmap != XmUNSPECIFIED_PIXMAP)
                XFreePixmap(linux::openmotif::cached_display, pixmap);
        }
        state.pixmaps.clear();
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

    Widget create_tree(native::tree_view &tree,
                       collection_state &state) {
        native::wnd *parent = tree.get_parent();
        Widget parent_widget = parent
                                   ? linux::openmotif::wnd_bindings
                                         .handle_from_object(parent)
                                   : nullptr;
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
            XmNselectionPolicy,
            XmSINGLE_SELECT,
            XmNnavigationType,
            XmTAB_GROUP,
            nullptr);
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
} // namespace

namespace native
{
    void tree_view::apply_items() {
        rebuild(*this);
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
        state->widget = create_tree(*self, *state);
        linux::openmotif::tree_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        rebuild(*self);
        self->apply_selection();
        self->on_wnd_create.emit();
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
            ::clear_items(*state);
            if (state->widget) {
                linux::openmotif::wnd_bindings.unregister_by_handle(
                    state->widget);
                XtDestroyWidget(state->widget);
            }
            linux::openmotif::tree_view_bindings
                .unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

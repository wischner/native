// Implements tab_view with Motif's native XmNotebook.

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <Xm/Form.h>
#include <Xm/Notebook.h>
#include <Xm/PushB.h>
#include <Xm/SeparatoG.h>
#include <Xm/SpinB.h>

#include <native.h>

#include "../../platforms/linux/x_image.h"
#include "../../rotated_text.h"
#include "globals.h"

namespace
{
    linux::openmotif::motif_tab_view *binding(native::tab_view &owner) {
        return linux::openmotif::tab_view_bindings
            .object_from_handle(&owner);
    }

    bool side_tabs(native::tab_placement placement) {
        return placement == native::tab_placement::left ||
               placement == native::tab_placement::right;
    }

    unsigned char notebook_orientation(
        native::tab_placement placement) {
        return side_tabs(placement) ? XmHORIZONTAL : XmVERTICAL;
    }

    unsigned char notebook_placement(
        native::tab_placement placement) {
        switch (placement) {
        case native::tab_placement::top:
            return XmTOP_RIGHT;
        case native::tab_placement::bottom:
        case native::tab_placement::right:
            return XmBOTTOM_RIGHT;
        case native::tab_placement::left:
            return XmBOTTOM_LEFT;
        }
        return XmTOP_RIGHT;
    }

    native::rgba pixel_color(Widget widget, Pixel pixel) {
        XColor color{};
        color.pixel = pixel;
        XQueryColor(linux::openmotif::cached_display,
                    DefaultColormapOfScreen(XtScreen(widget)),
                    &color);
        return native::rgba(
            static_cast<std::uint8_t>(color.red >> 8),
            static_cast<std::uint8_t>(color.green >> 8),
            static_cast<std::uint8_t>(color.blue >> 8),
            255);
    }

    Pixmap create_rotated_label(Widget tab,
                                const std::string &text,
                                bool clockwise) {
        if (!tab || !linux::openmotif::cached_display)
            return XmUNSPECIFIED_PIXMAP;
        const native::font_t &font =
            native::font_t::stock(native::font_role::control);
        const native::font_metrics metrics = font.get_metrics();
        const native::text_metrics measured = font.measure_text(text);
        const int width = std::max(1, metrics.height + 4);
        const int height = std::max(1, measured.width + 8);
        native::img label(static_cast<native::dim>(width),
                          static_cast<native::dim>(height));
        Pixel background = 0;
        Pixel foreground = 0;
        XtVaGetValues(tab,
                      XmNbackground,
                      &background,
                      XmNforeground,
                      &foreground,
                      nullptr);
        label.get_gpx()
            .clear(native::rgba(0, 0, 0, 0))
            .set_font(font)
            .set_ink(pixel_color(tab, foreground));
        native::detail::draw_rotated_text(
            label.get_gpx(),
            text,
            native::rect(0, 0, width, height),
            clockwise,
            4);

        Display *display = linux::openmotif::cached_display;
        Pixmap pixmap = XCreatePixmap(
            display,
            RootWindowOfScreen(XtScreen(tab)),
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            DefaultDepthOfScreen(XtScreen(tab)));
        if (pixmap == None)
            return XmUNSPECIFIED_PIXMAP;
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XSetForeground(display, gc, background);
        XFillRectangle(display,
                       pixmap,
                       gc,
                       0,
                       0,
                       static_cast<unsigned int>(width),
                       static_cast<unsigned int>(height));
        native::detail::blend_x_image(
            display,
            pixmap,
            gc,
            label,
            {0, 0},
            native::rect(0, 0, width, height),
            {static_cast<native::dim>(width),
             static_cast<native::dim>(height)});
        XFreeGC(display, gc);
        return pixmap;
    }

    void page_changed(Widget, XtPointer data, XtPointer call_data) {
        auto *owner = static_cast<native::tab_view *>(data);
        auto *event = static_cast<XmNotebookCallbackStruct *>(call_data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (owner && state && event && !state->suppress &&
            event->page_number > 0) {
            owner->on_native_selection(event->page_number-1);
        }
    }

    // XmNotebook creates a numeric SpinBox page scroller at realization.
    // native::tab_view already exposes every page through its tabs, so the
    // extra control is redundant and steals page space from small views.
    void hide_page_scroller(Widget notebook) {
        Widget scroller = XtNameToWidget(
            notebook, const_cast<char *>("PageScroller"));
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(notebook,
                      XmNchildren, &children,
                      XmNnumChildren, &count,
                      nullptr);
        for (Cardinal index = 0; index < count; ++index) {
            Widget child = children[index];
            if (child != scroller &&
                !XtIsSubclass(child, xmSpinBoxWidgetClass))
                continue;
            XtSetMappedWhenManaged(child, False);
            if (XtIsManaged(child))
                XtUnmanageChild(child);
            if (XtIsRealized(child))
                XtUnmapWidget(child);
        }

    }

    void hide_page_scroller_later(XtPointer data, XtIntervalId *) {
        hide_page_scroller(static_cast<Widget>(data));
    }

    void rebuild(native::tab_view &owner,
                 linux::openmotif::motif_tab_view &state) {
        XtVaSetValues(
            state.notebook,
            XmNorientation,
            notebook_orientation(owner.get_tab_placement()),
            XmNbackPagePlacement,
            notebook_placement(owner.get_tab_placement()),
            XmNshadowThickness,
            owner.get_page_frame_visible()
                ? state.frame_shadow_thickness
                : 0,
            nullptr);
        for (std::size_t index = 0; index < owner.get_item_count(); ++index) {
            native::wnd &content = owner.get_item(index).get_content();
            if (content.get_created())
                content.destroy();
        }
        for (Widget tab : state.tabs)
            XtDestroyWidget(tab);
        for (Widget page : state.page_frames)
            XtDestroyWidget(page);
        if (!state.label_pixmaps.empty())
            XSync(linux::openmotif::cached_display, False);
        for (Pixmap pixmap : state.label_pixmaps) {
            if (pixmap != XmUNSPECIFIED_PIXMAP && pixmap != None)
                XFreePixmap(linux::openmotif::cached_display, pixmap);
        }
        state.tabs.clear();
        state.page_frames.clear();
        state.pages.clear();
        state.label_pixmaps.clear();

        for (std::size_t index = 0; index < owner.get_item_count(); ++index) {
            const int page_number = static_cast<int>(index)+1;
            Widget page_frame = XtVaCreateManagedWidget(
                "page", xmFormWidgetClass, state.notebook,
                XmNnotebookChildType, XmPAGE,
                XmNpageNumber, page_number,
                XmNmarginWidth, 0,
                XmNmarginHeight, 0,
                XmNshadowThickness, 0,
                nullptr);
            Widget page = page_frame;
            if (!owner.get_page_frame_visible()) {
                const native::tab_placement placement =
                    owner.get_tab_placement();
                const bool horizontal = !side_tabs(placement);
                Arg separator_args[16];
                Cardinal separator_count = 0;
                XtSetArg(separator_args[separator_count],
                         XmNorientation,
                         horizontal ? XmHORIZONTAL : XmVERTICAL);
                ++separator_count;
                XtSetArg(separator_args[separator_count],
                         XmNseparatorType, XmSINGLE_LINE);
                ++separator_count;
                if (horizontal) {
                    XtSetArg(separator_args[separator_count],
                             XmNheight, 1);
                    ++separator_count;
                    XtSetArg(separator_args[separator_count],
                             XmNleftAttachment, XmATTACH_FORM);
                    ++separator_count;
                    XtSetArg(separator_args[separator_count],
                             XmNrightAttachment, XmATTACH_FORM);
                    ++separator_count;
                    XtSetArg(separator_args[separator_count],
                             placement == native::tab_placement::top
                                 ? XmNtopAttachment
                                 : XmNbottomAttachment,
                             XmATTACH_FORM);
                    ++separator_count;
                } else {
                    XtSetArg(separator_args[separator_count],
                             XmNwidth, 1);
                    ++separator_count;
                    XtSetArg(separator_args[separator_count],
                             XmNtopAttachment, XmATTACH_FORM);
                    ++separator_count;
                    XtSetArg(separator_args[separator_count],
                             XmNbottomAttachment, XmATTACH_FORM);
                    ++separator_count;
                    XtSetArg(separator_args[separator_count],
                             placement == native::tab_placement::left
                                 ? XmNleftAttachment
                                 : XmNrightAttachment,
                             XmATTACH_FORM);
                    ++separator_count;
                }
                Widget separator = XtCreateManagedWidget(
                    "pageSeparator",
                    xmSeparatorGadgetClass,
                    page_frame,
                    separator_args,
                    separator_count);
                Arg page_args[16];
                Cardinal page_count = 0;
                XtSetArg(page_args[page_count], XmNmarginWidth, 0);
                ++page_count;
                XtSetArg(page_args[page_count], XmNmarginHeight, 0);
                ++page_count;
                XtSetArg(page_args[page_count], XmNshadowThickness, 0);
                ++page_count;
                XtSetArg(page_args[page_count], XmNleftAttachment,
                         placement == native::tab_placement::left
                             ? XmATTACH_WIDGET
                             : XmATTACH_FORM);
                ++page_count;
                if (placement == native::tab_placement::left) {
                    XtSetArg(page_args[page_count], XmNleftWidget,
                             separator);
                    ++page_count;
                }
                XtSetArg(page_args[page_count], XmNrightAttachment,
                         placement == native::tab_placement::right
                             ? XmATTACH_WIDGET
                             : XmATTACH_FORM);
                ++page_count;
                if (placement == native::tab_placement::right) {
                    XtSetArg(page_args[page_count], XmNrightWidget,
                             separator);
                    ++page_count;
                }
                XtSetArg(page_args[page_count], XmNtopAttachment,
                         placement == native::tab_placement::top
                             ? XmATTACH_WIDGET
                             : XmATTACH_FORM);
                ++page_count;
                if (placement == native::tab_placement::top) {
                    XtSetArg(page_args[page_count], XmNtopWidget,
                             separator);
                    ++page_count;
                }
                XtSetArg(page_args[page_count], XmNbottomAttachment,
                         placement == native::tab_placement::bottom
                             ? XmATTACH_WIDGET
                             : XmATTACH_FORM);
                ++page_count;
                if (placement == native::tab_placement::bottom) {
                    XtSetArg(page_args[page_count], XmNbottomWidget,
                             separator);
                    ++page_count;
                }
                page = XtCreateManagedWidget(
                    "content", xmFormWidgetClass, page_frame,
                    page_args, page_count);
            }
            XmString label = XmStringCreateLocalized(const_cast<char *>(
                owner.get_item(index).get_title().c_str()));
            Widget tab = XtVaCreateManagedWidget(
                "tab", xmPushButtonWidgetClass, state.notebook,
                XmNnotebookChildType, XmMAJOR_TAB,
                XmNpageNumber, page_number,
                XmNlabelString, label,
                XmNsensitive, owner.get_item(index).get_enabled(),
                nullptr);
            XmStringFree(label);
            if (side_tabs(owner.get_tab_placement())) {
                const Pixmap pixmap = create_rotated_label(
                    tab,
                    owner.get_item(index).get_title(),
                    owner.get_tab_placement() ==
                        native::tab_placement::right);
                if (pixmap != XmUNSPECIFIED_PIXMAP && pixmap != None) {
                    XtVaSetValues(tab,
                                  XmNlabelType,
                                  XmPIXMAP,
                                  XmNlabelPixmap,
                                  pixmap,
                                  XmNlabelInsensitivePixmap,
                                  pixmap,
                                  nullptr);
                    state.label_pixmaps.push_back(pixmap);
                }
            }
            state.page_frames.push_back(page_frame);
            state.pages.push_back(page);
            state.tabs.push_back(tab);
        }
    }
} // namespace

namespace native
{
    void tab_view::apply_items() {
        auto *state = binding(*this);
        if (!state || !state->notebook)
            throw std::runtime_error("Motif: missing notebook binding.");
        state->suppress = true;
        rebuild(*this, *state);
        state->suppress = false;
        if (!state->tabs.empty()) {
            Dimension width = 0;
            Dimension height = 0;
            XtVaGetValues(state->tabs.front(),
                          XmNwidth,
                          &width,
                          XmNheight,
                          &height,
                          nullptr);
            _tab_height = std::max(
                1,
                static_cast<int>(
                    side_tabs(get_tab_placement()) ? width : height));
        }
    }

    void tab_view::apply_selected_index() {
        auto *state = binding(*this);
        if (!state || !state->notebook)
            throw std::runtime_error("Motif: missing notebook binding.");
        if (get_selected_index() < 0)
            return;
        state->suppress = true;
        XtVaSetValues(state->notebook,
                      XmNcurrentPageNumber,
                      get_selected_index()+1,
                      nullptr);
        state->suppress = false;
    }

    void tab_view::create_native() {
        auto *self = this;
        Widget parent = linux::openmotif::parent_widget(self);
        if (!parent)
            throw std::runtime_error(
                "Motif: tab_view requires a created parent.");
        auto *state = new linux::openmotif::motif_tab_view();
        state->notebook = XtVaCreateWidget(
            "notebook", xmNotebookWidgetClass, parent,
            XmNx, _bounds.p.x,
            XmNy, _bounds.p.y,
            XmNwidth, _bounds.d.w,
            XmNheight, _bounds.d.h,
            XmNbindingType, XmNONE,
            XmNbackPageNumber, 0,
            XmNbackPageSize, 0,
            XmNinnerMarginWidth, 0,
            XmNinnerMarginHeight, 0,
            XmNorientation, notebook_orientation(get_tab_placement()),
            XmNbackPagePlacement,
            notebook_placement(get_tab_placement()),
            nullptr);
        if (!state->notebook) {
            delete state;
            throw std::runtime_error("Motif: failed to create XmNotebook.");
        }
        XtVaGetValues(state->notebook,
                      XmNshadowThickness,
                      &state->frame_shadow_thickness,
                      nullptr);
        linux::openmotif::wnd_bindings.register_pair(state->notebook, self);
        linux::openmotif::tab_view_bindings.register_pair(self, state);
        XtAddCallback(state->notebook, XmNpageChangedCallback,
                      page_changed, self);
        self->configure_page_host(true, true);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void tab_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state || !state->notebook)
            throw std::runtime_error("Motif: tab_view is not created.");
        XtManageChild(state->notebook);
        hide_page_scroller(state->notebook);
        XtAppAddTimeOut(linux::openmotif::app_instance,
                        0,
                        hide_page_scroller_later,
                        state->notebook);
        const int selected = get_selected_index();
        if (selected >= 0) {
            wnd &content = get_item(
                static_cast<std::size_t>(selected)).get_content();
            if (content.get_created())
                content.show();
        }
    }

    void tab_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = binding(*self);
        if (state && state->notebook) {
            linux::openmotif::wnd_bindings.unregister_by_handle(
                state->notebook);
            XtDestroyWidget(state->notebook);
            if (!state->label_pixmaps.empty())
                XSync(linux::openmotif::cached_display, False);
            for (Pixmap pixmap : state->label_pixmaps) {
                if (pixmap != XmUNSPECIFIED_PIXMAP && pixmap != None)
                    XFreePixmap(linux::openmotif::cached_display,
                                pixmap);
            }
        }
        linux::openmotif::tab_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native

// Implements tab_view with Motif's native XmNotebook.

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <Xm/Form.h>
#include <Xm/Notebook.h>
#include <Xm/PushB.h>

#include <native.h>

#include "globals.h"

namespace
{
    linux::openmotif::motif_tab_view *binding(native::tab_view &owner) {
        return linux::openmotif::tab_view_bindings
            .object_from_handle(&owner);
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

    void rebuild(native::tab_view &owner,
                 linux::openmotif::motif_tab_view &state) {
        for (std::size_t index = 0; index < owner.get_item_count(); ++index) {
            native::wnd &content = owner.get_item(index).get_content();
            if (content.get_created())
                content.destroy();
        }
        for (Widget tab : state.tabs)
            XtDestroyWidget(tab);
        for (Widget page : state.pages)
            XtDestroyWidget(page);
        state.tabs.clear();
        state.pages.clear();

        for (std::size_t index = 0; index < owner.get_item_count(); ++index) {
            const int page_number = static_cast<int>(index)+1;
            Widget page = XtVaCreateManagedWidget(
                "page", xmFormWidgetClass, state.notebook,
                XmNnotebookChildType, XmPAGE,
                XmNpageNumber, page_number,
                XmNmarginWidth, 0,
                XmNmarginHeight, 0,
                nullptr);
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

    void tab_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tab_view *>(this);
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
            nullptr);
        if (!state->notebook) {
            delete state;
            throw std::runtime_error("Motif: failed to create XmNotebook.");
        }
        linux::openmotif::wnd_bindings.register_pair(state->notebook, self);
        linux::openmotif::tab_view_bindings.register_pair(self, state);
        XtAddCallback(state->notebook, XmNpageChangedCallback,
                      page_changed, self);
        _created = true;
        self->_content_host_is_page = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        auto *state = binding(*const_cast<tab_view *>(this));
        if (!_created || !state || !state->notebook)
            throw std::runtime_error("Motif: tab_view is not created.");
        XtManageChild(state->notebook);
        const int selected = get_selected_index();
        if (selected >= 0) {
            wnd &content = get_item(
                static_cast<std::size_t>(selected)).get_content();
            if (content.get_created())
                content.show();
        }
    }

    void tab_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state && state->notebook) {
            linux::openmotif::wnd_bindings.unregister_by_handle(
                state->notebook);
            XtDestroyWidget(state->notebook);
        }
        linux::openmotif::tab_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native

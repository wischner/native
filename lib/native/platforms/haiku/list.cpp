//
// Implements the native Haiku list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <ListView.h>
#include <StringItem.h>
#include <Window.h>
#include <native.h>
#include <native/list.h>
#include "../../control_render_access.h"
#include "globals.h"
namespace
{
    template <typename function_type>
    void locked(BWindow *window, function_type function) {
        if (!window)
            return;
        bool held = window->IsLocked();
        if (!held && !window->Lock())
            return;
        function();
        if (!held)
            window->Unlock();
    }
    BView *parent_view(native::list *c) {
        auto *p = c->get_parent();
        BView *view = haiku::parent_view(p);
        if (!p || !p->get_created() || !view || !view->Window())
            throw std::runtime_error(
                "Haiku: list requires a created parent.");
        return view;
    }
    class native_list_view : public BListView
    {
    public:
        native_list_view(BRect r, native::list *o)
            : BListView(r,
                        "native_list",
                        B_SINGLE_SELECTION_LIST,
                        B_FOLLOW_LEFT | B_FOLLOW_TOP,
                        B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS)
            , _owner(o) {}
        void SelectionChanged() override {
            BListView::SelectionChanged();
            if (!_suppress && _owner)
                _owner->on_native_selection(CurrentSelection());
        }

        void Draw(BRect update) override {
            if (!_owner || !_owner->get_created()) {
                BListView::Draw(update);
                return;
            }
            native::gpx &graphics = _owner->get_gpx();
            auto appearance = native::theme::create(graphics);
            const BRect frame = Bounds();
            const native::rect bounds(
                0,
                0,
                static_cast<native::dim>(frame.IntegerWidth() + 1),
                static_cast<native::dim>(frame.IntegerHeight() + 1));
            graphics.set_clip(native::rect(
                static_cast<native::coord>(update.left),
                static_cast<native::coord>(update.top),
                static_cast<native::dim>(update.IntegerWidth() + 1),
                static_cast<native::dim>(update.IntegerHeight() + 1)));
            native::theme::state state;
            state.focused = IsFocus();
            native::detail::control_render_access::draw(
                *_owner, graphics, *appearance, bounds, state);
        }
        bool _suppress = false;

    private:
        native::list *_owner;
    };
    void replace(native_list_view *v,
                 const std::vector<std::string> &items) {
        v->_suppress = true;
        while (v->CountItems() > 0)
            delete v->RemoveItem(static_cast<int32>(0));
        for (const auto &i : items)
            v->AddItem(new BStringItem(i.c_str()));
        v->_suppress = false;
    }
} // namespace
namespace native
{
    void list::apply_items() {
        auto *b = haiku::list_bindings.object_from_handle(this);
        auto *v =
            b ? static_cast<native_list_view *>(b->view) : nullptr;
        if (!v)
            throw std::runtime_error("Haiku: Missing list binding.");
        locked(v->Window(), [&] {
            replace(v, _items);
        });
    }
    void list::apply_selected_index() {
        auto *b = haiku::list_bindings.object_from_handle(this);
        auto *v =
            b ? static_cast<native_list_view *>(b->view) : nullptr;
        if (!v)
            throw std::runtime_error("Haiku: Missing list binding.");
        locked(v->Window(), [&] {
            v->_suppress = true;
            v->DeselectAll();
            if (_selected_index >= 0)
                v->Select(_selected_index);
            v->_suppress = false;
        });
    }
    void list::create() const {
        if (_created)
            return;
        auto *self = const_cast<list *>(this);
        BView *parent = parent_view(self);
        BWindow *w = parent->Window();
        native_list_view *v = nullptr;
        locked(w, [&] {
            v = new native_list_view(BRect(_bounds.p.x,
                                           _bounds.p.y,
                                           _bounds.x2() - 1,
                                           _bounds.y2() - 1),
                                     self);
            replace(v, _items);
            if (_selected_index >= 0) {
                v->_suppress = true;
                v->Select(_selected_index);
                v->_suppress = false;
            }
            parent->AddChild(v);
        });
        if (!v)
            throw std::runtime_error("Haiku: Failed to create list.");
        auto *b = new haiku::haiku_list();
        b->view = v;
        haiku::list_bindings.register_pair(self, b);
        _created = true;
        self->on_native_create();
    }
    void list::show() const {
        auto *b = haiku::list_bindings.object_from_handle(
            const_cast<list *>(this));
        if (!_created || !b || !b->view)
            throw std::runtime_error("Haiku: list is not created.");
        locked(b->view->Window(), [&] {
            b->view->Show();
        });
    }
    void list::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<list *>(this);
        auto *b = haiku::list_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (b) {
            BWindow *w = b->view ? b->view->Window() : nullptr;
            locked(w, [&] {
                b->view->RemoveSelf();
                delete b->view;
            });
            haiku::list_bindings.unregister_by_handle(self);
            delete b;
        }
    }
} // namespace native

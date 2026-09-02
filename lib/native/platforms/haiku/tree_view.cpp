//
// Implements tree_view with Haiku's BOutlineListView and BScrollView.
// A small native BStringItem subclass retains stable IDs and paints the
// optional portable image while BOutlineListView owns hierarchy chrome.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>

#include <Bitmap.h>
#include <Message.h>
#include <OutlineListView.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <Window.h>

#include <native.h>

#include "../../control_render_access.h"
#include "globals.h"

namespace
{
    template <typename function_type>
    void locked(BWindow *window, function_type function) {
        if (!window)
            return;
        const bool held = window->IsLocked();
        if (!held && !window->Lock())
            return;
        function();
        if (!held)
            window->Unlock();
    }

    class tree_string_item final : public BStringItem
    {
    public:
        tree_string_item(const native::tree_view_item &item,
                         uint32 depth)
            : BStringItem(item.text.c_str(), depth, item.expanded)
            , id(item.id) {
            SetEnabled(item.enabled);
        }

        native::tree_item_id id;

    };

    class native_tree_view final : public BOutlineListView
    {
    public:
        native_tree_view(BRect frame, native::tree_view &owner)
            : BOutlineListView(frame,
                               "native_tree_view",
                               B_SINGLE_SELECTION_LIST,
                               B_FOLLOW_ALL,
                               B_WILL_DRAW | B_NAVIGABLE |
                                   B_FRAME_EVENTS)
            , _owner(owner) {
            SetViewUIColor(B_LIST_BACKGROUND_COLOR);
            SetLowUIColor(B_LIST_BACKGROUND_COLOR);
        }

        void SelectionChanged() override {
            BOutlineListView::SelectionChanged();
            if (_suppress)
                return;
            auto *item = dynamic_cast<tree_string_item *>(
                ItemAt(CurrentSelection()));
            if (!item || item->IsEnabled()) {
                _owner.on_native_selection(
                    item ? item->id
                         : native::invalid_tree_item_id);
                return;
            }
            _suppress = true;
            DeselectAll();
            auto *binding = haiku::tree_view_bindings
                                .object_from_handle(&_owner);
            if (binding) {
                const auto previous = binding->items.find(
                    _owner.get_selected_item());
                if (previous != binding->items.end()) {
                    const int32 index = IndexOf(previous->second);
                    if (index >= 0)
                        Select(index);
                }
            }
            _suppress = false;
        }

        void MakeFocus(bool focused = true) override {
            BOutlineListView::MakeFocus(focused);
            _owner.on_native_focus(focused);
        }

        void MouseDown(BPoint where) override {
            BOutlineListView::MouseDown(where);
            int32 clicks = 1;
            if (Window() && Window()->CurrentMessage())
                Window()->CurrentMessage()->FindInt32(
                    "clicks", &clicks);
            if (clicks >= 2) {
                auto *item = dynamic_cast<tree_string_item *>(
                    ItemAt(IndexOf(where)));
                if (item)
                    _owner.on_native_double_click(item->id);
            }
        }

        void KeyDown(const char *bytes, int32 count) override {
            if (bytes && count > 0 && bytes[0] == B_ENTER) {
                _owner.on_native_navigation(
                    native::tree_view_navigation::activate);
                return;
            }
            if (bytes && count > 0 && bytes[0] == B_SPACE) {
                _owner.on_native_navigation(
                    native::tree_view_navigation::toggle);
                return;
            }
            BOutlineListView::KeyDown(bytes, count);
        }

    protected:
        void DrawItem(BListItem *raw,
                      BRect frame,
                      bool complete = false) override {
            auto *item = dynamic_cast<tree_string_item *>(raw);
            if (!item || !_owner.get_created()) {
                BOutlineListView::DrawItem(raw, frame, complete);
                return;
            }
            const int32 native_index = IndexOf(raw);
            if (native_index < 0)
                return;
            native::rect bounds(
                static_cast<native::coord>(frame.left),
                static_cast<native::coord>(frame.top),
                static_cast<native::dim>(
                    std::max<float>(0, frame.Width() + 1)),
                static_cast<native::dim>(
                    std::max<float>(0, frame.Height() + 1)));
            native::gpx &graphics = _owner.get_gpx();
            haiku::scoped_gpx_target drawing_target(_owner, this);
            graphics.set_clip(bounds);
            auto appearance = native::theme::create(graphics);
            native::theme::state state;
            state.selected = item->IsSelected();
            state.disabled = !item->IsEnabled();
            state.focused = state.selected && IsFocus();
            const native::tree_view_item &portable_item =
                _owner.get_item(item->id);
            native::detail::control_render_access::draw_tree_row(
                _owner,
                graphics,
                *appearance,
                static_cast<std::size_t>(native_index),
                portable_item,
                item->OutlineLevel(),
                bounds,
                state,
                false);
            if (!portable_item.children.empty()) {
                DrawLatch(frame,
                          item->OutlineLevel(),
                          !item->IsExpanded(),
                          item->IsSelected() || complete,
                          false);
            }
        }

        BRect LatchRect(BRect item_rect, int32 level) const override {
            const int32 index = IndexOf(BPoint(
                item_rect.left,
                item_rect.top + item_rect.Height() / 2.0f));
            if (index < 0)
                return BOutlineListView::LatchRect(
                    item_rect, level);
            const native::rect portable_row =
                _owner.get_row_bounds(static_cast<std::size_t>(index));
            const native::rect portable_latch =
                _owner.get_disclosure_bounds(
                    static_cast<std::size_t>(index));
            const float left = item_rect.left +
                portable_latch.p.x - portable_row.p.x;
            const float top = item_rect.top +
                portable_latch.p.y - portable_row.p.y;
            return BRect(
                left,
                top,
                left + portable_latch.d.w - 1,
                top + portable_latch.d.h - 1);
        }

    public:
        void ExpandOrCollapse(BListItem *item,
                              bool expand) override {
            const bool changed = item &&
                                 item->IsExpanded() != expand;
            BOutlineListView::ExpandOrCollapse(item, expand);
            auto *tree_item = dynamic_cast<tree_string_item *>(item);
            if (!_suppress && changed && tree_item) {
                _suppress = true;
                _owner.on_native_expansion(
                    tree_item->id, expand);
                _suppress = false;
            }
        }

        bool _suppress = false;

    private:
        native::tree_view &_owner;
    };

    haiku::haiku_tree_view &binding_for(native::tree_view &tree) {
        auto *binding =
            haiku::tree_view_bindings.object_from_handle(&tree);
        if (!binding || !binding->view)
            throw std::runtime_error(
                "Haiku: missing tree_view binding.");
        return *binding;
    }

    native_tree_view *view_for(native::tree_view &tree) {
        return static_cast<native_tree_view *>(binding_for(tree).view);
    }

    void clear_items(native_tree_view &view,
                     haiku::haiku_tree_view &binding) {
        binding.items.clear();
        while (view.FullListCountItems() > 0) {
            BListItem *item = view.FullListItemAt(
                view.FullListCountItems() - 1);
            view.RemoveItem(item);
            delete item;
        }
    }

    void rebuild(native::tree_view &tree) {
        auto &binding = binding_for(tree);
        auto &view = *view_for(tree);
        view._suppress = true;
        clear_items(view, binding);
        std::function<void(const std::vector<native::tree_view_item> &,
                           tree_string_item *,
                           uint32)> append;
        append = [&tree, &view, &binding, &append](
                     const std::vector<native::tree_view_item> &items,
                     tree_string_item *parent,
                     uint32 depth) {
            auto add_item = [&](const native::tree_view_item &item) {
                auto *native_item = new tree_string_item(
                    item, depth);
                const bool added = parent
                                       ? view.AddUnder(native_item,
                                                       parent)
                                       : view.AddItem(native_item);
                if (!added) {
                    delete native_item;
                    return;
                }
                binding.items[item.id] = native_item;
                append(item.children, native_item, depth + 1);
                if (item.expanded)
                    view.Expand(native_item);
                else
                    view.Collapse(native_item);
            };
            if (parent) {
                // AddUnder inserts immediately after its parent. Add sibling
                // branches in reverse so their visible order remains the
                // order supplied by the portable model.
                for (auto item = items.rbegin(); item != items.rend(); ++item)
                    add_item(*item);
            } else {
                for (const native::tree_view_item &item : items)
                    add_item(item);
            }
        };
        append(tree.get_items(), nullptr, 0);
        view._suppress = false;
    }
} // namespace

namespace native
{
    void tree_view::apply_items() {
        auto &binding = binding_for(*this);
        locked(binding.view->Window(), [&] {
            rebuild(*this);
        });
    }

    void tree_view::apply_selection() {
        auto &binding = binding_for(*this);
        auto *view = view_for(*this);
        locked(view->Window(), [&] {
            view->_suppress = true;
            view->DeselectAll();
            const auto found = binding.items.find(_selected_item);
            if (found != binding.items.end()) {
                const int32 index = view->IndexOf(found->second);
                if (index >= 0) {
                    view->Select(index);
                    view->ScrollToSelection();
                }
            }
            view->_suppress = false;
        });
    }

    void tree_view::apply_expansion(tree_item_id id, bool expanded) {
        auto &binding = binding_for(*this);
        auto *view = view_for(*this);
        const auto found = binding.items.find(id);
        if (found == binding.items.end())
            return;
        locked(view->Window(), [&] {
            view->_suppress = true;
            if (expanded)
                view->Expand(found->second);
            else
                view->Collapse(found->second);
            view->_suppress = false;
        });
    }

    void tree_view::apply_scroll_offset() {
        auto &binding = binding_for(*this);
        locked(binding.scroll->Window(), [&] {
            if (BScrollBar *bar =
                    binding.scroll->ScrollBar(B_VERTICAL)) {
                bar->SetValue(_scroll_offset);
            }
        });
    }

    void tree_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        BView *parent = haiku::parent_view(get_parent(), self);
        BWindow *window = parent ? parent->Window() : nullptr;
        if (!parent || !window)
            throw std::runtime_error(
                "Haiku: tree_view requires a created parent.");
        native_tree_view *view = nullptr;
        BScrollView *scroll = nullptr;
        locked(window, [&] {
            view = new native_tree_view(
                BRect(0,
                      0,
                      std::max<int>(1, _bounds.d.w - 18),
                      std::max<int>(1, _bounds.d.h - 2)),
                *self);
            scroll = new BScrollView("native_tree_scroll",
                                     view,
                                     B_FOLLOW_NONE,
                                     0,
                                     false,
                                     true,
                                     B_FANCY_BORDER);
            scroll->MoveTo(_bounds.p.x, _bounds.p.y);
            scroll->ResizeTo(_bounds.d.w - 1, _bounds.d.h - 1);
            scroll->Hide();
            parent->AddChild(scroll);
        });
        if (!view || !scroll)
            throw std::runtime_error(
                "Haiku: failed to create BOutlineListView tree_view.");
        auto *binding = new haiku::haiku_tree_view();
        binding->view = view;
        binding->scroll = scroll;
        haiku::tree_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        self->apply_items();
        self->apply_selection();
        self->on_native_create();
    }

    void tree_view::show() const {
        auto *binding = haiku::tree_view_bindings.object_from_handle(
            const_cast<tree_view *>(this));
        if (!_created || !binding || !binding->scroll)
            throw std::runtime_error(
                "Haiku: tree_view is not created.");
        locked(binding->scroll->Window(), [&] {
            binding->scroll->Show();
        });
    }

    void tree_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<tree_view *>(this);
        auto *binding =
            haiku::tree_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            BWindow *window = binding->scroll
                                  ? binding->scroll->Window()
                                  : nullptr;
            locked(window, [&] {
                if (binding->scroll) {
                    binding->scroll->RemoveSelf();
                    delete binding->scroll;
                }
            });
            haiku::tree_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native

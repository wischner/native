//
// Implements native Haiku text views with live complete-value
// validation and portable clipboard command routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <InterfaceDefs.h>
#include <ScrollView.h>
#include <TextView.h>
#include <Window.h>

#include "globals.h"

namespace
{
    // Execute a view operation while holding its BWindow lock.
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

    // Return the created native parent view.
    BView *parent_view(native::text_edit *editor) {
        native::wnd *parent = editor->get_parent();
        BView *view = haiku::parent_view(parent, editor);
        if (!parent || !parent->get_created() || !view ||
            !view->Window())
            throw std::runtime_error(
                "Haiku: text_edit requires a created parent.");
        return view;
    }

    // Build a candidate by replacing one byte-offset selection.
    std::string replaced(const std::string &value,
                         int32 begin,
                         int32 end,
                         const char *text,
                         int32 length) {
        const std::size_t first = static_cast<std::size_t>(
            std::max<int32>(0, begin));
        const std::size_t last = static_cast<std::size_t>(
            std::max(begin, end));
        std::string candidate = value;
        candidate.replace(first,
                          last - first,
                          text ? std::string(text, length)
                               : std::string());
        return candidate;
    }

    // BTextView subclass that validates every insertion and deletion.
    class native_text_edit_view final : public BTextView
    {
    public:
        native_text_edit_view(BRect frame,
                              BRect text_rect,
                              native::text_edit *owner)
            : BTextView(frame,
                        "native_text_edit",
                        text_rect,
                        B_FOLLOW_LEFT | B_FOLLOW_TOP,
                        B_WILL_DRAW | B_NAVIGABLE)
            , owner_(owner) {}

        void set_text(const std::string &text) {
            suppress_ = true;
            SetText(text.c_str());
            suppress_ = false;
        }

        bool replace_selection(const std::string &text) {
            int32 begin = 0;
            int32 end = 0;
            GetSelection(&begin, &end);
            const std::string candidate =
                replaced(Text(), begin, end, text.data(), text.size());
            if (!owner_ || !owner_->validate(candidate))
                return false;
            suppress_ = true;
            Delete(begin, end);
            Insert(text.c_str(), text.size());
            suppress_ = false;
            owner_->on_native_text(candidate);
            return true;
        }

        void InsertText(const char *text,
                        int32 length,
                        int32 offset,
                        const text_run_array *runs) override {
            if (suppress_) {
                BTextView::InsertText(text, length, offset, runs);
                return;
            }
            int32 begin = 0;
            int32 end = 0;
            GetSelection(&begin, &end);
            const std::string candidate =
                replaced(Text(), begin, end, text, length);
            if (!owner_ || !owner_->validate(candidate))
                return;
            BTextView::InsertText(text, length, offset, runs);
            owner_->on_native_text(candidate);
        }

        void DeleteText(int32 begin, int32 end) override {
            if (suppress_) {
                BTextView::DeleteText(begin, end);
                return;
            }
            const std::string candidate =
                replaced(Text(), begin, end, nullptr, 0);
            if (!owner_ || !owner_->validate(candidate))
                return;
            BTextView::DeleteText(begin, end);
            owner_->on_native_text(candidate);
        }

        void KeyDown(const char *bytes, int32 count) override {
            const uint32 keys = modifiers();
            if (owner_ && count == 1 &&
                (keys & (B_COMMAND_KEY | B_CONTROL_KEY)) != 0) {
                switch (bytes[0]) {
                case 'a':
                case 'A':
                    owner_->select_all();
                    return;
                case 'c':
                case 'C':
                    owner_->copy();
                    return;
                case 'x':
                case 'X':
                    owner_->cut();
                    return;
                case 'v':
                case 'V':
                    owner_->paste();
                    return;
                }
            }
            BTextView::KeyDown(bytes, count);
        }

    private:
        native::text_edit *owner_;
        bool suppress_ = false;
    };
} // namespace

namespace native
{
    void text_edit::apply_text() {
        auto *binding =
            haiku::text_edit_bindings.object_from_handle(this);
        auto *view = binding
                         ? dynamic_cast<native_text_edit_view *>(
                               binding->view)
                         : nullptr;
        if (!view)
            throw std::runtime_error(
                "Haiku: Missing text-edit binding.");
        locked(view->Window(), [&] {
            view->set_text(_text);
        });
    }

    void text_edit::apply_read_only() {
        auto *binding =
            haiku::text_edit_bindings.object_from_handle(this);
        if (!binding || !binding->view)
            throw std::runtime_error(
                "Haiku: Missing text-edit binding.");
        locked(binding->view->Window(), [&] {
            binding->view->MakeEditable(!_read_only);
        });
    }

    void text_edit::create() const {
        if (_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        BView *parent = parent_view(self);
        BWindow *window = parent->Window();
        const BRect frame(_bounds.p.x,
                          _bounds.p.y,
                          _bounds.x2() - 1,
                          _bounds.y2() - 1);
        native_text_edit_view *view = nullptr;
        BScrollView *scroll = nullptr;
        locked(window, [&] {
            const BRect text_rect(4,
                                  4,
                                  frame.Width() - 4,
                                  frame.Height() - 4);
            view = new native_text_edit_view(
                _mode == text_edit_mode::multi_line
                    ? BRect(0, 0, frame.Width() - 16, frame.Height())
                    : frame,
                text_rect,
                self);
            view->set_text(_text);
            view->MakeEditable(!_read_only);
            view->SetWordWrap(
                _mode == text_edit_mode::multi_line);
            if (_mode == text_edit_mode::multi_line) {
                scroll = new BScrollView("native_text_edit_scroll",
                                         view,
                                         B_FOLLOW_LEFT | B_FOLLOW_TOP,
                                         0,
                                         false,
                                         true,
                                         B_FANCY_BORDER);
                scroll->MoveTo(frame.LeftTop());
                scroll->ResizeTo(frame.Width(), frame.Height());
                parent->AddChild(scroll);
            } else {
                parent->AddChild(view);
            }
        });
        if (!view)
            throw std::runtime_error(
                "Haiku: Failed to create text_edit.");

        auto *binding = new haiku::haiku_text_edit;
        binding->view = view;
        binding->scroll = scroll;
        haiku::text_edit_bindings.register_pair(self, binding);
        _created = true;
        self->on_native_create();
    }

    void text_edit::show() const {
        auto *binding = haiku::text_edit_bindings.object_from_handle(
            const_cast<text_edit *>(this));
        if (!_created || !binding || !binding->view)
            throw std::runtime_error(
                "Haiku: text_edit is not created.");
        BView *outer = binding->scroll
                           ? static_cast<BView *>(binding->scroll)
                           : static_cast<BView *>(binding->view);
        locked(outer->Window(), [&] {
            outer->Show();
        });
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding =
            haiku::text_edit_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            BView *outer = binding->scroll
                               ? static_cast<BView *>(binding->scroll)
                               : static_cast<BView *>(binding->view);
            locked(outer->Window(), [&] {
                outer->RemoveSelf();
                delete outer;
            });
            haiku::text_edit_bindings.unregister_by_handle(self);
            delete binding;
        }
    }

    std::string text_edit::selected_text() const {
        auto *binding = haiku::text_edit_bindings.object_from_handle(
            const_cast<text_edit *>(this));
        if (!binding || !binding->view)
            return {};
        int32 begin = 0;
        int32 end = 0;
        binding->view->GetSelection(&begin, &end);
        return begin < end
                   ? std::string(binding->view->Text() + begin,
                                 static_cast<std::size_t>(end - begin))
                   : std::string();
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        auto *binding =
            haiku::text_edit_bindings.object_from_handle(this);
        auto *view = binding
                         ? dynamic_cast<native_text_edit_view *>(
                               binding->view)
                         : nullptr;
        return view && !_read_only && view->replace_selection(text);
    }

    void text_edit::select_all_native() const {
        auto *binding = haiku::text_edit_bindings.object_from_handle(
            const_cast<text_edit *>(this));
        if (binding && binding->view)
            binding->view->SelectAll();
    }
} // namespace native

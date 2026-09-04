//
// Composes Haiku combo boxes from native controls. Both styles open the
// same below-field menu; the editable arrow lives inside the text frame.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <Alignment.h>
#include <ControlLook.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>
#include <TextView.h>
#include <native/combo_box.h>

#include "globals.h"

namespace
{
    constexpr uint32 text_message = 'nctx';
    constexpr uint32 popup_message = 'ncpp';

    template <typename function_type>
    void locked(BWindow *window, function_type function) {
        if (!window) return;
        const bool held = window->IsLocked();
        if (!held && !window->Lock()) return;
        function();
        if (!held) window->Unlock();
    }

    class combo_menu final : public BPopUpMenu
    {
    public:
        combo_menu() : BPopUpMenu("native_combo_options", true, false) {}

        float horizontal_margins() const {
            float left = 0, top = 0, right = 0, bottom = 0;
            GetItemMargins(&left, &top, &right, &bottom);
            return left + right;
        }
    };

    class combo_item final : public BMenuItem
    {
    public:
        combo_item(const std::string &label, int width)
            : BMenuItem(label.c_str(), nullptr), _width(width) {}

        void GetContentSize(float *width, float *height) override {
            BMenuItem::GetContentSize(width, height);
            const float margins = Menu()
                ? static_cast<combo_menu *>(Menu())->horizontal_margins() : 0;
            *width = std::max(0.0f, _width - margins - 2);
        }

    private:
        int _width;
    };

    class choice_button final : public BButton
    {
    public:
        choice_button(BRect frame, const std::string &label)
            : BButton(frame, "native_combo_choice", label.c_str(),
                      new BMessage(popup_message)) {}

        void Draw(BRect update) override {
            PushState();
            BRect frame = Bounds();
            const auto base = ui_color(B_PANEL_BACKGROUND_COLOR);
            const uint32 flags = be_control_look->Flags(this);
            be_control_look->DrawMenuFieldFrame(
                this, frame, update, base, base, flags);
            be_control_look->DrawMenuFieldBackground(
                this, frame, update, base, true, flags);
            frame.left += 6;
            frame.right -= 18;
            be_control_look->DrawLabel(this, Label(), frame, update,
                base, flags, BAlignment(B_ALIGN_LEFT, B_ALIGN_MIDDLE));
            PopState();
        }
    };

    // BTextControl normally derives its border from the editor frame.
    // The combo reserves some of that frame for a native arrow child.
    class combo_text final : public BTextControl
    {
    public:
        combo_text(BRect frame, const std::string &text)
            : BTextControl(frame, "native_combo_text", "", text.c_str(),
                           new BMessage(text_message)) {}

        void Draw(BRect update) override {
            PushState();
            BRect frame = Bounds();
            uint32 flags = IsEnabled() ? 0 : BPrivate::BControlLook::B_DISABLED;
            const bool focused = TextView()->IsFocus() && Window()->IsActive();
            // The native editor only invalidates around its own frame.
            // Focus must also repaint the portion enclosing the arrow.
            if (_focused != focused) {
                _focused = focused;
                Invalidate();
            }
            if (focused)
                flags |= BPrivate::BControlLook::B_FOCUSED;
            be_control_look->DrawTextControlBorder(
                this, frame, update, ViewColor(), flags);
            PopState();
        }

    private:
        bool _focused = false;
    };

    class arrow_button final : public BButton
    {
    public:
        arrow_button()
            : BButton(BRect(0, 0, 18, 18), "native_combo_arrow", "",
                      new BMessage(popup_message)) {}

        void Draw(BRect update) override {
            PushState();
            const auto base = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
            SetHighColor(Value() == B_CONTROL_ON
                ? tint_color(base, B_DARKEN_1_TINT) : base);
            FillRect(update);
            SetHighColor(ui_color(B_CONTROL_BORDER_COLOR));
            StrokeLine(Bounds().LeftTop(), Bounds().LeftBottom());
            BRect arrow = Bounds();
            arrow.InsetBy(5, 5);
            be_control_look->DrawArrowShape(this, arrow, update,
                ui_color(B_CONTROL_TEXT_COLOR),
                BPrivate::BControlLook::B_DOWN_ARROW, 0, 0.6f);
            PopState();
        }
    };

    class combo_root final : public BView
    {
    public:
        combo_root(BRect frame, native::combo_box &owner)
            : BView(frame, "native_combo", B_FOLLOW_NONE,
                    B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE)
            , _owner(owner) {
            SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
        }

        void configure() {
            auto *state = haiku::combo_box_bindings.object_from_handle(&_owner);
            if (!state) return;
            const bool editable = _owner.get_style() ==
                native::combo_box_style::editable;
            const float width = Bounds().Width();
            const float height = Bounds().Height();
            state->choice->ResizeTo(width, height);
            state->text->ResizeTo(width, height);
            const float arrow_width = std::max(12.0f, height - 4);
            state->arrow->MoveTo(std::max(2.0f, width - arrow_width - 2), 2);
            state->arrow->ResizeTo(arrow_width, std::max(0.0f, height - 4));
            BTextView *editor = state->text->TextView();
            editor->ResizeTo(std::max(0.0f,
                state->arrow->Frame().left - editor->Frame().left - 2),
                editor->Bounds().Height());
            BRect text_rect = editor->TextRect();
            text_rect.right = text_rect.left + editor->Bounds().Width();
            editor->SetTextRect(text_rect);
            auto visible = [](BView *view, bool show) {
                if (show && view->IsHidden(view)) view->Show();
                else if (!show && !view->IsHidden(view)) view->Hide();
            };
            visible(state->choice, !editable);
            visible(state->text, editable);
        }

        void FrameResized(float, float) override { configure(); }

        void MessageReceived(BMessage *message) override {
            auto *state = haiku::combo_box_bindings.object_from_handle(&_owner);
            if (!state || state->suppress) return;
            if (message->what == text_message) {
                _owner.on_native_text(state->text->Text());
            } else if (message->what == popup_message) {
                if (_owner.get_items().empty()) return;
                combo_menu menu;
                for (std::size_t i = 0; i < _owner.get_items().size(); ++i) {
                    auto *item = new combo_item(_owner.get_items()[i],
                                               _owner.get_dimensions().w);
                    menu.AddItem(item);
                    item->SetMarked(static_cast<int>(i) == _owner.get_selected_index());
                }
                _owner.on_native_drop_down(true);
                BMenuItem *item = menu.Go(ConvertToScreen(BPoint(0,
                    Bounds().bottom + 1)), false, true);
                if (item) {
                    const int32 index = menu.IndexOf(item);
                    state->text->SetText(_owner.get_items()[index].c_str());
                    state->choice->SetLabel(_owner.get_items()[index].c_str());
                    _owner.on_native_selection(index);
                }
                _owner.on_native_drop_down(false);
            } else BView::MessageReceived(message);
        }

    private:
        native::combo_box &_owner;
    };
}

namespace native
{
    void combo_box::apply_items() {
        apply_selected_index();
    }

    void combo_box::apply_selected_index() {
        auto *state = haiku::combo_box_bindings.object_from_handle(this);
        if (!state) return;
        locked(state->view->Window(), [&] {
            state->choice->SetLabel(get_text().c_str());
        });
    }

    void combo_box::apply_text() {
        auto *state = haiku::combo_box_bindings.object_from_handle(this);
        if (state) locked(state->view->Window(), [&] {
            if (get_text() != state->text->Text())
                state->text->SetText(get_text().c_str());
            state->choice->SetLabel(get_text().c_str());
        });
    }

    void combo_box::apply_style() {
        auto *state = haiku::combo_box_bindings.object_from_handle(this);
        if (state) locked(state->view->Window(), [&] {
            static_cast<combo_root *>(state->view)->configure();
        });
    }

    void combo_box::create_native() {
        BView *parent = haiku::parent_view(get_parent(), this);
        if (!parent || !parent->Window())
            throw std::runtime_error("Haiku: combo box requires a parent.");
        auto *state = new haiku::haiku_combo_box;
        locked(parent->Window(), [&] {
            auto *root = new combo_root(BRect(_bounds.p.x, _bounds.p.y,
                _bounds.x2() - 1, _bounds.y2() - 1), *this);
            root->Hide();
            state->view = root;
            state->choice = new choice_button(root->Bounds(), get_text());
            state->text = new combo_text(root->Bounds(), get_text());
            state->text->SetDivider(0);
            state->text->SetModificationMessage(new BMessage(text_message));
            state->arrow = new arrow_button;
            haiku::combo_box_bindings.register_pair(this, state);
            root->AddChild(state->choice);
            root->AddChild(state->text);
            state->text->AddChild(state->arrow);
            parent->AddChild(root);
            state->text->SetTarget(root);
            state->arrow->SetTarget(root);
            state->choice->SetTarget(root);
            root->configure();
        });
    }

    void combo_box::show_native() {
        auto *state = haiku::combo_box_bindings.object_from_handle(this);
        if (!state) throw std::runtime_error("Haiku: combo box not created.");
        locked(state->view->Window(), [&] { state->view->Show(); });
    }

    void combo_box::destroy_native() {
        auto *state = haiku::combo_box_bindings.object_from_handle(this);
        if (!state) return;
        locked(state->view->Window(), [&] {
            state->view->RemoveSelf();
            delete state->view;
        });
        haiku::combo_box_bindings.unregister_by_handle(this);
        delete state;
    }
}

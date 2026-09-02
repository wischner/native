//
// Implements the Haiku combo box from native text and popup controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <InterfaceDefs.h>
#include <OptionPopUp.h>
#include <TextControl.h>
#include <View.h>
#include <Window.h>

#include <native/combo_box.h>

#include "globals.h"

namespace
{
    constexpr uint32 text_changed_message = 'nctx';

    template <typename function_type>
    void locked(BWindow *window, function_type function) {
        if (!window) return;
        const bool held = window->IsLocked();
        if (!held && !window->Lock()) return;
        function();
        if (!held) window->Unlock();
    }

    class native_combo_popup final : public BOptionPopUp
    {
    public:
        native_combo_popup(BRect frame, native::combo_box *owner)
            : BOptionPopUp(frame, "native_combo_popup", "",
                           new BMessage('ncmb'))
            , owner_(owner) {}

        status_t Invoke(BMessage *message = nullptr) override {
            const status_t result = BOptionPopUp::Invoke(message);
            auto *state = owner_ ? haiku::combo_box_bindings
                                       .object_from_handle(owner_) : nullptr;
            if (owner_ && state && !state->suppress) {
                int32 value = -1;
                SelectedOption(nullptr, &value);
                if (value >= 0 &&
                    value < static_cast<int32>(owner_->get_items().size())) {
                    if (state->text) {
                        state->suppress = true;
                        state->text->SetText(
                            owner_->get_items()[value].c_str());
                        state->suppress = false;
                    }
                    owner_->on_native_selection(static_cast<int>(value));
                }
            }
            return result;
        }

    private:
        native::combo_box *owner_;
    };

    class native_combo_root final : public BView
    {
    public:
        native_combo_root(BRect frame, native::combo_box *owner)
            : BView(frame, "native_combo", B_FOLLOW_LEFT_TOP,
                    B_WILL_DRAW | B_FRAME_EVENTS)
            , owner_(owner) {
            SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        }

        void configure(native::combo_box_style style) {
            auto *state = owner_ ? haiku::combo_box_bindings
                                       .object_from_handle(owner_) : nullptr;
            if (!state || !state->popup || !state->text)
                return;
            const float width = Bounds().Width()+1;
            const float height = Bounds().Height()+1;
            state->popup->MoveTo(0, 0);
            state->popup->ResizeTo(std::max(1.0f, width),
                                   std::max(1.0f, height));
            if (style == native::combo_box_style::editable) {
                state->text->MoveTo(0, 0);
                state->text->ResizeTo(std::max(1.0f, width-height),
                                      std::max(1.0f, height));
                state->text->Show();
            } else {
                state->text->Hide();
            }
        }

        void FrameResized(float, float) override {
            configure(owner_->get_style());
        }

        void MessageReceived(BMessage *message) override {
            if (message && message->what == text_changed_message && owner_) {
                auto *state = haiku::combo_box_bindings
                                  .object_from_handle(owner_);
                if (state && state->text && !state->suppress)
                    owner_->on_native_text(state->text->Text());
                return;
            }
            BView::MessageReceived(message);
        }

    private:
        native::combo_box *owner_;
    };

    haiku::haiku_combo_box *state_for(native::combo_box *owner) {
        return haiku::combo_box_bindings.object_from_handle(owner);
    }

    void replace(native_combo_popup *control,
                 const std::vector<std::string> &items) {
        while (control->CountOptions() > 0)
            control->RemoveOptionAt(control->CountOptions()-1);
        for (std::size_t index = 0; index < items.size(); ++index)
            control->AddOptionAt(items[index].c_str(),
                                 static_cast<int32>(index),
                                 static_cast<int32>(index));
    }
}

namespace native
{
    void combo_box::apply_items() {
        auto *state = state_for(this);
        if (!state || !state->popup)
            throw std::runtime_error("Haiku: Missing combo box binding.");
        locked(state->view->Window(), [&] {
            state->suppress = true;
            replace(static_cast<native_combo_popup *>(state->popup),
                    get_items());
            state->suppress = false;
        });
    }

    void combo_box::apply_selected_index() {
        auto *state = state_for(this);
        if (!state || !state->popup)
            throw std::runtime_error("Haiku: Missing combo box binding.");
        locked(state->view->Window(), [&] {
            state->suppress = true;
            state->popup->SetValue(get_selected_index());
            state->suppress = false;
        });
    }

    void combo_box::apply_text() {
        auto *state = state_for(this);
        if (!state || !state->text)
            throw std::runtime_error("Haiku: Missing combo box binding.");
        locked(state->view->Window(), [&] {
            state->suppress = true;
            state->text->SetText(get_text().c_str());
            state->suppress = false;
        });
    }

    void combo_box::apply_style() {
        auto *state = state_for(this);
        if (!state || !state->view)
            return;
        locked(state->view->Window(), [&] {
            static_cast<native_combo_root *>(state->view)
                ->configure(get_style());
        });
    }

    void combo_box::create() const {
        if (_created) return;
        BView *parent = haiku::parent_view(get_parent());
        if (!parent || !parent->Window())
            throw std::runtime_error(
                "Haiku: combo box requires a created parent.");
        auto *self = const_cast<combo_box *>(this);
        auto *state = new haiku::haiku_combo_box;
        auto *root = new native_combo_root(
            BRect(_bounds.p.x, _bounds.p.y,
                  _bounds.x2()-1, _bounds.y2()-1), self);
        auto *popup = new native_combo_popup(root->Bounds(), self);
        auto *text = new BTextControl(root->Bounds(), "native_combo_text",
                                      "", get_text().c_str(),
                                      new BMessage(text_changed_message));
        text->SetModificationMessage(new BMessage(text_changed_message));
        state->view = root;
        state->popup = popup;
        state->text = text;
        haiku::combo_box_bindings.register_pair(self, state);
        locked(parent->Window(), [&] {
            root->AddChild(popup);
            root->AddChild(text);
            text->SetTarget(root);
            replace(popup, get_items());
            if (get_selected_index() >= 0)
                popup->SetValue(get_selected_index());
            parent->AddChild(root);
            root->configure(get_style());
        });
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        auto *state = state_for(const_cast<combo_box *>(this));
        if (!_created || !state || !state->view)
            throw std::runtime_error("Haiku: combo box is not created.");
        locked(state->view->Window(), [&] { state->view->Show(); });
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *state = state_for(self);
        self->on_native_destroy();
        if (state) {
            BWindow *window = state->view ? state->view->Window() : nullptr;
            locked(window, [&] {
                state->view->RemoveSelf();
                delete state->view;
            });
            haiku::combo_box_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native

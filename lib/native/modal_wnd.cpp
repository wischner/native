//
// Implements backend-neutral owner-modal session and result behavior.
// Native backends supply focus, stacking, and input exclusion.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/modal_wnd.h>

#include <stdexcept>
#include <utility>

namespace native
{
    modal_wnd::modal_wnd(app_wnd &owner,
                         std::string title,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : owned_wnd(
              owner, std::move(title), x, y, width, height)
        , _modal_active(false)
        , _result(dialog_result::none) {}

    modal_wnd::modal_wnd(app_wnd &owner,
                         const std::string &title,
                         const point &position,
                         const size &dimensions)
        : modal_wnd(owner,
                    title,
                    position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    modal_wnd::modal_wnd(app_wnd &owner,
                         const std::string &title,
                         const rect &bounds)
        : modal_wnd(owner, title, bounds.p, bounds.d) {}

    modal_wnd::~modal_wnd() {
        destroy();
    }

    bool modal_wnd::get_modal() const {
        return true;
    }

    bool modal_wnd::get_modal_active() const {
        return _modal_active;
    }

    dialog_result modal_wnd::get_result() const {
        return _result;
    }

    void modal_wnd::show() const {
        if (!get_created())
            throw std::logic_error(
                "A modal window must be created before show().");
        if (!get_owner() || !get_owner()->get_created())
            throw std::logic_error(
                "A modal window requires a created owner.");

        if (_modal_active) {
            app_wnd::show();
            return;
        }

        begin_modal_session();
        try {
            app_wnd::show();
        } catch (...) {
            end_modal_session();
            throw;
        }
    }

    void modal_wnd::destroy() const {
        if (_modal_active && _result == dialog_result::none)
            _result = dialog_result::cancelled;

        const bool notify = end_modal_session();
        app_wnd::destroy();

        if (notify) {
            const_cast<modal_wnd *>(this)->on_modal_close.emit(
                _result);
        }
    }

    void modal_wnd::on_native_destroy() {
        if (_modal_active && _result == dialog_result::none)
            _result = dialog_result::cancelled;

        const bool notify = end_modal_session();
        app_wnd::on_native_destroy();

        if (notify)
            on_modal_close.emit(_result);
    }

    void modal_wnd::close(dialog_result result) const {
        if (result == dialog_result::none)
            throw std::invalid_argument(
                "A closed modal window requires a final result.");

        _result = result;
        destroy();
    }

    void modal_wnd::begin_modal_session() const {
        app_wnd *owner = get_owner();
        if (!owner)
            throw std::logic_error(
                "A modal window no longer has an owner.");

        _result = dialog_result::none;
        _modal_active = true;
        owner->begin_modal(const_cast<modal_wnd *>(this));
    }

    bool modal_wnd::end_modal_session() const {
        if (!_modal_active)
            return false;

        if (app_wnd *owner = get_owner())
            owner->end_modal(const_cast<modal_wnd *>(this));
        _modal_active = false;
        return true;
    }
} // namespace native

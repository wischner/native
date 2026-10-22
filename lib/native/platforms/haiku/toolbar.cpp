//
// Hosts edge tools in Haiku's shared-kit BToolBar. Native flat BButtons,
// orientation-aware separators and layout own the complete presentation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../toolbar_peer.h"
#include <Autolock.h>
#include <Bitmap.h>
#include <Button.h>
#include <ToolBar.h>
#include <Window.h>
#include <algorithm>
#include <cmath>
#include <typeinfo>

namespace
{
    constexpr uint32 first_command = 0x6e540000;
    class native_toolbar final : public BToolBar
    {
    public:
        native_toolbar(native::toolbar &owner, orientation axis)
            : BToolBar(axis), _owner(owner) {}
        void MessageReceived(BMessage *message) override {
            const auto index = message->what - first_command;
            if (message->what >= first_command && index < _owner.get_items().size()) {
                const auto item = _owner.get_items()[index];
                if (!_owner.on_native_command(item.id))
                    SetActionPressed(message->what, item.checked);
                return;
            }
            BToolBar::MessageReceived(message);
        }
    private:
        native::toolbar &_owner;
    };
    struct toolbar_state
    {
        native_toolbar *view = nullptr;
        std::vector<native::tool_item> items;
        native::size icon_size;
        native::window_edge edge = native::window_edge::top;
    };
    bool same_items(const toolbar_state &peer, const native::toolbar &owner) {
        const auto &items = owner.get_items();
        if (!peer.view || peer.edge != owner.get_edge() || peer.items.size() != items.size() ||
            peer.icon_size.w != owner.get_icon_size().w || peer.icon_size.h != owner.get_icon_size().h)
            return false;
        for (std::size_t index = 0; index < items.size(); ++index)
            if (peer.items[index].id != items[index].id || peer.items[index].text != items[index].text ||
                peer.items[index].icon != items[index].icon || peer.items[index].kind != items[index].kind)
                return false;
        return true;
    }
    std::unique_ptr<BBitmap> bitmap(const native::img &image, native::size size) {
        auto result = std::make_unique<BBitmap>(BRect(0, 0, size.w - 1, size.h - 1), B_RGBA32);
        if (result->InitCheck() != B_OK) return {};
        auto *bytes = static_cast<uint8 *>(result->Bits());
        std::fill(bytes, bytes + result->BitsLength(), 0);
        const double scale = std::min(double(size.w) / image.w(), double(size.h) / image.h());
        const int width = std::max(1, int(image.w() * scale));
        const int height = std::max(1, int(image.h() * scale));
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto color = image.pixels()[(y * image.h() / height) * image.w() + x * image.w() / width];
                auto *pixel = bytes + (y + (size.h - height) / 2) * result->BytesPerRow() +
                    (x + (size.w - width) / 2) * 4;
                pixel[0] = color.b; pixel[1] = color.g; pixel[2] = color.r; pixel[3] = color.a;
            }
        return result;
    }
}
namespace native::detail
{
    bool toolbar_peer_active(const toolbar &owner, const canvas &surface) {
        return typeid(owner) == typeid(toolbar) && peer_state<toolbar_state>(surface);
    }
    bool refresh_toolbar_peer(toolbar &owner, canvas &surface, const std::vector<rect> &) {
        if (typeid(owner) != typeid(toolbar) || !surface.get_created()) return false;
        auto *host = haiku::canvas_bindings.object_from_handle(&surface)->view;
        BAutolock lock(host->Window());
        if (!lock.IsLocked()) return false;
        auto *peer = peer_state<toolbar_state>(surface);
        if (!peer) {
            peer = new toolbar_state;
            assign_peer_state(surface, peer);
        }
        if (!same_items(*peer, owner)) {
            if (peer->view) { peer->view->RemoveSelf(); delete peer->view; }
            peer->items = owner.get_items(); peer->icon_size = owner.get_icon_size();
            peer->edge = owner.get_edge();
            const bool vertical = owner.get_edge() == window_edge::left || owner.get_edge() == window_edge::right;
            peer->view = new native_toolbar(owner, vertical ? B_VERTICAL : B_HORIZONTAL);
            host->AddChild(peer->view);
            for (std::size_t index = 0; index < peer->items.size(); ++index) {
                const auto &item = peer->items[index];
                if (item.kind == tool_kind::separator) { peer->view->AddSeparator(); continue; }
                auto icon = item.icon ? bitmap(*item.icon, peer->icon_size) : nullptr;
                peer->view->AddAction(first_command + index, peer->view, icon.get(),
                    item.text.c_str(), item.text.c_str());
                if (item.kind != tool_kind::button)
                    peer->view->FindButton(first_command + index)->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);
            }
            peer->view->AddGlue();
        }
        const bool vertical = owner.get_edge() == window_edge::left || owner.get_edge() == window_edge::right;
        const auto minimum = peer->view->MinSize();
        const int extent = int(std::ceil(vertical ? minimum.width : minimum.height)) + 5;
        if (owner.get_extent() < extent) owner.set_extent(extent);
        peer->view->MoveTo(2, 2);
        peer->view->ResizeTo(std::max(0, int(surface.get_dimensions().w) - 5),
                             std::max(0, int(surface.get_dimensions().h) - 5));
        peer->view->Layout(true);
        for (std::size_t index = 0; index < owner.get_items().size(); ++index) {
            const auto &item = owner.get_items()[index];
            peer->view->SetActionEnabled(first_command + index, item.enabled && surface.get_input_enabled());
            peer->view->SetActionPressed(first_command + index, item.checked);
        }
        return true;
    }
}

//
// Hosts the genuine WinUI NavigationView in a Windows App SDK XAML
// island. WinUI owns navigation, optional search, icons, focus and
// selection visuals; borrowed page HWNDs and the public model stay in
// Native's existing panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "winui.h"
#include "../../picker_peer.h"
#include <appmodel.h>
#include <algorithm>
#include <bit>
#include <stdexcept>
// Win32's timer alias collides with WinUI's animation method name.
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Content.h>
#include <filesystem>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Microsoft.UI.Interop.h>

namespace
{
    using namespace winrt;
    using namespace Microsoft::UI;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Hosting;
    using namespace Microsoft::UI::Xaml::Media::Imaging;
    using namespace Microsoft::UI::Dispatching;

    // WinUI needs an Application even when Win32 owns the message loop.
    // Its metadata provider resolves the built-in controls and
    // templates.
    struct xaml_application
        : ApplicationT<xaml_application, Markup::IXamlMetadataProvider>
    {
        XamlTypeInfo::XamlControlsXamlMetaDataProvider provider;
        Markup::IXamlType
        GetXamlType(Windows::UI::Xaml::Interop::TypeName const &type) {
            return provider.GetXamlType(type);
        }
        Markup::IXamlType GetXamlType(hstring const &name) {
            return provider.GetXamlType(name);
        }
        com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() {
            return provider.GetXmlnsDefinitions();
        }
        void OnLaunched(LaunchActivatedEventArgs const &) {
            Resources().MergedDictionaries().Append(
                XamlControlsResources());
        }
    };

    struct xaml_session
    {
        HMODULE bootstrap = nullptr;
        bool bootstrapped = false;
        bool apartment = false;
        DispatcherQueueController dispatcher{nullptr};
        WindowsXamlManager manager{nullptr};
        com_ptr<xaml_application> application;
        HMODULE windowing = nullptr;
        using pretranslate = BOOL(WINAPI *)(const MSG *);
        pretranslate translate = nullptr;

        xaml_session() {
            try {
                std::vector<wchar_t> path(32768);
                const auto length = GetModuleFileNameW(
                    nullptr, path.data(), path.size());
                if (!length || length == path.size())
                    throw std::runtime_error(
                        "Cannot locate the WinUI runtime bootstrap.");
                const auto file =
                    std::filesystem::path(path.data()).parent_path() /
                    L"Microsoft.WindowsAppRuntime.Bootstrap.dll";
                bootstrap = LoadLibraryExW(
                    file.c_str(),
                    nullptr,
                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                        LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
                if (!bootstrap)
                    throw std::runtime_error(
                        "Windows App SDK bootstrap DLL is missing "
                        "beside the executable.");
                using initialize = HRESULT(WINAPI *)(
                    UINT32, PCWSTR, PACKAGE_VERSION, UINT32);
                const auto start =
                    std::bit_cast<initialize>(GetProcAddress(
                        bootstrap, "MddBootstrapInitialize2"));
                if (!start)
                    throw std::runtime_error(
                        "Unsupported Windows App SDK bootstrap DLL.");
                check_hresult(
                    start(0x00010008, L"", PACKAGE_VERSION{}, 0));
                bootstrapped = true;
                init_apartment(apartment_type::single_threaded);
                apartment = true;
                dispatcher =
                    DispatcherQueueController::CreateOnCurrentThread();
                application = make_self<xaml_application>();
                manager =
                    WindowsXamlManager::InitializeForCurrentThread();
                windowing =
                    LoadLibraryW(L"Microsoft.UI.Windowing.Core.dll");
                if (!windowing)
                    throw std::runtime_error(
                        "Cannot load WinUI input hosting.");
                translate = std::bit_cast<pretranslate>(GetProcAddress(
                    windowing, "ContentPreTranslateMessage"));
                if (!translate)
                    throw std::runtime_error(
                        "WinUI input hosting is unavailable.");
            } catch (...) {
                close();
                throw;
            }
        }
        ~xaml_session() {
            close();
        }
        void close() noexcept {
            try {
                if (manager) {
                    manager.Close();
                    manager = nullptr;
                }
                if (dispatcher) {
                    dispatcher.ShutdownQueue();
                    dispatcher = nullptr;
                }
            } catch (...) {
            }
            application = nullptr;
            translate = nullptr;
            if (windowing) {
                FreeLibrary(windowing);
                windowing = nullptr;
            }
            if (apartment) {
                uninit_apartment();
                apartment = false;
            }
            if (bootstrapped) {
                using shutdown = void(WINAPI *)();
                if (auto stop = std::bit_cast<shutdown>(GetProcAddress(
                        bootstrap, "MddBootstrapShutdown")))
                    stop();
                bootstrapped = false;
            }
            if (bootstrap) {
                FreeLibrary(bootstrap);
                bootstrap = nullptr;
            }
        }
    };
    thread_local std::unique_ptr<xaml_session> session;

    // The WinRT pixel buffer exposes its writable bytes through this
    // COM ABI.
    struct buffer_bytes : IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE Buffer(BYTE **bytes) = 0;
    };
    IconElement image_icon(const native::img &image) {
        WriteableBitmap bitmap(image.w(), image.h());
        com_ptr<buffer_bytes> access;
        const GUID iid{
            0x905a0fef,
            0xbc53,
            0x11df,
            {0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda}};
        auto buffer = bitmap.PixelBuffer();
        check_hresult(reinterpret_cast<IUnknown *>(get_abi(buffer))
                          ->QueryInterface(iid, access.put_void()));
        BYTE *pixels = nullptr;
        check_hresult(access->Buffer(&pixels));
        for (std::size_t i = 0; i < std::size_t(image.w()) * image.h();
             ++i) {
            const auto value = image.pixels()[i];
            pixels[4 * i] = unsigned(value.b) * value.a / 255;
            pixels[4 * i + 1] = unsigned(value.g) * value.a / 255;
            pixels[4 * i + 2] = unsigned(value.r) * value.a / 255;
            pixels[4 * i + 3] = value.a;
        }
        bitmap.Invalidate();
        ImageIcon icon;
        icon.Source(bitmap);
        return icon;
    }

    struct settings_state final
        : native::detail::settings_sidebar_resources
    {
        native::settings_sidebar &owner;
        DesktopWindowXamlSource source{nullptr};
        NavigationView navigation{nullptr};
        AutoSuggestBox search{nullptr};
        std::vector<native::settings_page> items;
        bool updating = false;
        bool closing = false;
        event_token selection_token{}, search_token{}, focus_token{};

        static settings_state *
        live(native::settings_sidebar *owner,
             const native::detail::window_guard &guard) {
            if (!guard.alive())
                return nullptr;
            auto *resources = native::detail::peer_state<
                native::detail::settings_sidebar_resources>(*owner);
            auto *state = static_cast<settings_state *>(resources);
            return state && !state->closing ? state : nullptr;
        }

        explicit settings_state(native::settings_sidebar &value)
            : owner(value) {
            if (!session)
                session = std::make_unique<xaml_session>();
            source = DesktopWindowXamlSource();
            source.Initialize(
                winrt::Microsoft::UI::GetWindowIdFromWindow(
                    windows::wnd_bindings.handle_from_object(&owner)));
            navigation = NavigationView();
            navigation.PaneDisplayMode(
                NavigationViewPaneDisplayMode::Left);
            navigation.CompactModeThresholdWidth(0);
            navigation.ExpandedModeThresholdWidth(0);
            navigation.IsPaneToggleButtonVisible(false);
            navigation.IsBackButtonVisible(
                NavigationViewBackButtonVisible::Collapsed);
            navigation.IsSettingsVisible(false);
            navigation.IsPaneOpen(true);
            navigation.IsTabStop(true);
            search = AutoSuggestBox();
            search.PlaceholderText(L"Search settings");
            const native::detail::window_guard guard(owner);
            auto *target = &owner;
            selection_token = navigation.SelectionChanged(
                [guard,
                 target](NavigationView const &,
                         NavigationViewSelectionChangedEventArgs const
                             &event) {
                    auto *state = live(target, guard);
                    if (!state || state->updating)
                        return;
                    auto item = event.SelectedItemContainer();
                    if (!item || !item.Tag())
                        return;
                    const int id = unbox_value<int>(item.Tag());
                    // Native selection dispatch must finish before
                    // page/window callbacks can rebuild or destroy the
                    // island.
                    native::app::post([guard, target, id] {
                        if (guard.alive())
                            target->on_native_selection(id);
                    });
                });
            search_token = search.TextChanged(
                [guard, target](
                    AutoSuggestBox const &field,
                    AutoSuggestBoxTextChangedEventArgs const &event) {
                    auto *state = live(target, guard);
                    if (!state || state->updating ||
                        event.Reason() !=
                            AutoSuggestionBoxTextChangeReason::
                                UserInput)
                        return;
                    const auto text = to_string(field.Text());
                    native::app::post([guard, target, text] {
                        if (guard.alive())
                            target->on_native_search(text);
                    });
                });
            focus_token = source.TakeFocusRequested(
                [guard, target](
                    DesktopWindowXamlSource const &,
                    DesktopWindowXamlSourceTakeFocusRequestedEventArgs const
                        &event) {
                    if (auto *state = live(target, guard)) {
                        const auto stops = state->tab_stops();
                        if (!stops.empty())
                            SetFocus(
                                event.Request().Reason() ==
                                        XamlSourceFocusNavigationReason::
                                            Last
                                    ? stops.back()
                                    : stops.front());
                    }
                });
            source.Content(navigation);
            source.SiteBridge().Show();
        }
        ~settings_state() override {
            close();
        }
        void close() noexcept override {
            if (!source)
                return;
            closing = true;
            try {
                navigation.SelectionChanged(selection_token);
                search.TextChanged(search_token);
                source.TakeFocusRequested(focus_token);
                source.Close();
            } catch (...) {
            }
            source = nullptr;
            navigation = nullptr;
            search = nullptr;
        }
        std::vector<HWND> tab_stops() const {
            std::vector<HWND> result;
            if (!owner.get_selected_item())
                return result;
            auto *page =
                owner.get_item(owner.get_selected_item()).content;
            const auto window =
                windows::wnd_bindings.handle_from_object(page);
            if (window)
                EnumChildWindows(
                    window,
                    +[](HWND child, LPARAM data) -> BOOL {
                        if ((GetWindowLongPtrW(child, GWL_STYLE) &
                             WS_TABSTOP) &&
                            IsWindowVisible(child) &&
                            IsWindowEnabled(child))
                            reinterpret_cast<std::vector<HWND> *>(data)
                                ->push_back(child);
                        return TRUE;
                    },
                    reinterpret_cast<LPARAM>(&result));
            return result;
        }
        bool tab(const MSG &message) {
            if (message.message != WM_KEYDOWN ||
                message.wParam != VK_TAB)
                return false;
            HWND focus = GetFocus();
            if (SendMessageW(focus,
                             WM_GETDLGCODE,
                             VK_TAB,
                             reinterpret_cast<LPARAM>(&message)) &
                DLGC_WANTTAB)
                return false;
            const auto stops = tab_stops();
            const auto found = std::find_if(
                stops.begin(), stops.end(), [focus](HWND window) {
                    return window == focus || IsChild(window, focus);
                });
            if (found == stops.end())
                return false;
            const bool previous = GetKeyState(VK_SHIFT) & 0x8000;
            if ((!previous && std::next(found) == stops.end()) ||
                (previous && found == stops.begin()))
                return source
                    .NavigateFocus(XamlSourceFocusNavigationRequest(
                        previous
                            ? XamlSourceFocusNavigationReason::Last
                            : XamlSourceFocusNavigationReason::First))
                    .WasFocusMoved();
            SetFocus(previous ? *std::prev(found) : *std::next(found));
            return true;
        }
        void refresh() {
            if (!source)
                return;
            updating = true;
            try {
                const auto area = owner.get_client_bounds();
                const int width =
                    std::min(owner.get_sidebar_width(), int(area.w()));
                source.SiteBridge().MoveAndResize(
                    Windows::Graphics::RectInt32{
                        area.x1(), area.y1(), width, int(area.h())});
                navigation.OpenPaneLength(width);
                navigation.IsPaneOpen(true);
                navigation.AutoSuggestBox(
                    owner.get_search_visible() ? search : nullptr);
                if (search.Text() !=
                    to_hstring(owner.get_search_text()))
                    search.Text(to_hstring(owner.get_search_text()));
                const auto visible =
                    native::detail::settings_visible_items(owner);
                const bool rebuild =
                    visible.size() != items.size() ||
                    !std::equal(visible.begin(),
                                visible.end(),
                                items.begin(),
                                [](const auto &a, const auto &b) {
                                    return a.id == b.id &&
                                           a.title == b.title &&
                                           a.section == b.section &&
                                           a.enabled == b.enabled &&
                                           a.image == b.image;
                                });
                if (rebuild) {
                    navigation.MenuItems().Clear();
                    std::string group;
                    for (const auto &page : visible) {
                        if (!page.section.empty() &&
                            page.section != group) {
                            NavigationViewItemHeader header;
                            header.Content(
                                box_value(to_hstring(page.section)));
                            navigation.MenuItems().Append(header);
                        }
                        group = page.section;
                        NavigationViewItem item;
                        item.Content(box_value(to_hstring(page.title)));
                        item.Tag(box_value(page.id));
                        item.IsEnabled(page.enabled);
                        if (page.image && page.image->w() &&
                            page.image->h())
                            item.Icon(image_icon(*page.image));
                        navigation.MenuItems().Append(item);
                    }
                    items = visible;
                }
                Windows::Foundation::IInspectable selected{nullptr};
                for (auto value : navigation.MenuItems())
                    if (auto item = value.try_as<NavigationViewItem>();
                        item && unbox_value<int>(item.Tag()) ==
                                    owner.get_selected_item())
                        selected = item;
                navigation.SelectedItem(selected);
                updating = false;
            } catch (...) {
                updating = false;
                throw;
            }
        }
    };
} // namespace
namespace windows
{
    bool pretranslate_winui(const MSG &message) {
        if (!session)
            return false;
        if (session->translate && session->translate(&message))
            return true;
        if (message.message != WM_KEYDOWN || message.wParam != VK_TAB)
            return false;
        native::wnd *target = nullptr;
        for (HWND window = message.hwnd; window && !target;
             window = GetParent(window))
            target = wnd_bindings.object_from_handle(window);
        while (target &&
               !dynamic_cast<native::settings_sidebar *>(target))
            target = target->get_parent();
        if (!target)
            return false;
        auto *state = static_cast<settings_state *>(
            native::detail::peer_state<
                native::detail::settings_sidebar_resources>(*target));
        return state && !state->closing && state->tab(message);
    }
    void shutdown_winui() noexcept {
        session.reset();
    }
} // namespace windows
namespace native::detail
{
    bool create_settings_sidebar_peer(settings_sidebar &owner) {
        try {
            std::unique_ptr<settings_sidebar_resources> resources =
                std::make_unique<settings_state>(owner);
            assign_peer_state(owner, resources.release());
            return true;
        } catch (const winrt::hresult_error &error) {
            throw std::runtime_error("WinUI settings navigation: " +
                                     winrt::to_string(error.message()));
        }
    }
    bool settings_sidebar_uses_native_search() {
        return true;
    }
    bool settings_sidebar_uses_list() {
        return false;
    }
    int settings_sidebar_navigation_padding() {
        return 0;
    }
    void refresh_settings_sidebar_peer(settings_sidebar &owner) {
        auto *resources = peer_state<settings_sidebar_resources>(owner);
        try {
            if (resources)
                static_cast<settings_state *>(resources)->refresh();
        } catch (const winrt::hresult_error &error) {
            throw std::runtime_error("WinUI settings navigation: " +
                                     winrt::to_string(error.message()));
        }
    }
} // namespace native::detail

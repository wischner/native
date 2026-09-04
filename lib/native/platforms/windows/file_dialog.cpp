//
// Implements shared Windows Common Item Dialog setup, result decoding,
// and the synchronous file-panel cleanup contract.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0700
#endif

#include "file_dialog_common.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <shlobj.h>
#include <shobjidl.h>
#include <windows.h>

#include <native/open_file_dialog.h>
#include <native/save_file_dialog.h>

#include "globals.h"

namespace
{
    // Own COM initialization for one synchronous chooser operation.
    class com_apartment
    {
    public:
        com_apartment()
            : initialized_(false) {
            const HRESULT result = CoInitializeEx(
                nullptr, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(result))
                initialized_ = true;
            else if (result != RPC_E_CHANGED_MODE)
                throw std::runtime_error(
                    "Windows: Failed to initialize COM for a file "
                    "dialog.");
        }

        com_apartment(const com_apartment &) = delete;
        com_apartment &operator=(const com_apartment &) = delete;

        ~com_apartment() {
            if (initialized_)
                CoUninitialize();
        }

    private:
        bool initialized_;
    };

    // Release one borrowed-out COM interface at scope exit.
    template <typename interface_type> class com_ptr
    {
    public:
        explicit com_ptr(interface_type *pointer = nullptr)
            : pointer_(pointer) {}

        com_ptr(const com_ptr &) = delete;
        com_ptr &operator=(const com_ptr &) = delete;

        ~com_ptr() {
            if (pointer_)
                pointer_->Release();
        }

        interface_type *get() const {
            return pointer_;
        }

        interface_type **put() {
            return &pointer_;
        }

    private:
        interface_type *pointer_;
    };

    // Convert a failed HRESULT into the backend's portable exception.
    void require_success(HRESULT result, const char *message) {
        if (FAILED(result))
            throw std::runtime_error(message);
    }

    // Join one portable wildcard group for COMDLG_FILTERSPEC.
    std::wstring join_patterns(
        const native::file_filter &filter) {
        std::wstring value;
        for (const std::string &pattern : filter.patterns) {
            if (!value.empty())
                value.push_back(L';');
            value += windows::utf8_to_wide(pattern);
        }
        return value.empty() ? L"*.*" : value;
    }

    // Apply all portable filter groups to a Common Item Dialog.
    void set_filters(IFileDialog *panel,
                     const native::file_dialog &dialog) {
        const auto &filters = dialog.get_filters();
        if (filters.empty())
            return;

        std::vector<std::wstring> names;
        std::vector<std::wstring> patterns;
        names.reserve(filters.size());
        patterns.reserve(filters.size());
        for (const native::file_filter &filter : filters) {
            names.push_back(windows::utf8_to_wide(filter.name));
            patterns.push_back(join_patterns(filter));
        }

        std::vector<COMDLG_FILTERSPEC> specifications;
        specifications.reserve(filters.size());
        for (std::size_t index = 0; index < filters.size(); ++index) {
            specifications.push_back(
                {names[index].c_str(), patterns[index].c_str()});
        }

        require_success(panel->SetFileTypes(
                            static_cast<UINT>(specifications.size()),
                            specifications.data()),
                        "Windows: Failed to set file-dialog filters.");
        require_success(panel->SetFileTypeIndex(1),
                        "Windows: Failed to select a file-dialog "
                        "filter.");
    }

    // Select an existing filesystem folder without failing the dialog.
    void set_folder(IFileDialog *panel,
                    const std::wstring &folder) {
        if (folder.empty())
            return;

        com_ptr<IShellItem> item;
        const HRESULT result = SHCreateItemFromParsingName(
            folder.c_str(), nullptr, IID_PPV_ARGS(item.put()));
        if (SUCCEEDED(result))
            panel->SetFolder(item.get());
    }

    // Split an initial file path into the dialog folder and leaf name.
    void set_initial_path(IFileDialog *panel,
                          const native::file_dialog &dialog) {
        const std::filesystem::path path = dialog.get_initial_path();
        if (path.empty())
            return;

        std::error_code error;
        if (std::filesystem::is_directory(path, error)) {
            set_folder(panel, path.wstring());
            return;
        }
        if (!path.parent_path().empty())
            set_folder(panel, path.parent_path().wstring());
        if (!path.filename().empty())
            panel->SetFileName(path.filename().c_str());
    }

    // Apply portable state and caller-specific COM dialog options.
    void configure_dialog(IFileDialog *panel,
                          const native::file_dialog &dialog,
                          FILEOPENDIALOGOPTIONS extra_options,
                          FILEOPENDIALOGOPTIONS removed_options = 0) {
        FILEOPENDIALOGOPTIONS options = 0;
        require_success(panel->GetOptions(&options),
                        "Windows: Failed to read file-dialog options.");
        options &= ~removed_options;
        options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                   FOS_NOCHANGEDIR | extra_options;
        require_success(panel->SetOptions(options),
                        "Windows: Failed to set file-dialog options.");

        const std::wstring title =
            windows::utf8_to_wide(dialog.get_title());
        require_success(panel->SetTitle(title.c_str()),
                        "Windows: Failed to set file-dialog title.");
        set_filters(panel, dialog);
        set_initial_path(panel, dialog);
    }

    // Decode one shell item as a standard filesystem path.
    std::filesystem::path path_from_item(IShellItem *item) {
        if (!item)
            return {};

        PWSTR value = nullptr;
        const HRESULT result =
            item->GetDisplayName(SIGDN_FILESYSPATH, &value);
        if (FAILED(result) || !value)
            return {};

        std::filesystem::path path(value);
        CoTaskMemFree(value);
        return path;
    }

    // Show a chooser with its owner and normalize cancellation.
    bool show_dialog(IFileDialog *panel,
                     const native::file_dialog &dialog) {
        native::app_wnd *owner = dialog.get_owner();
        HWND owner_window = owner
                                ? windows::wnd_bindings
                                      .handle_from_object(owner)
                                : nullptr;
        const HRESULT result = panel->Show(owner_window);
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            return false;
        require_success(result,
                        "Windows: Failed to show a file dialog.");
        return true;
    }
} // namespace

namespace windows
{
    file_dialog_response show_open_file_dialog(
        const native::file_dialog &dialog, bool allow_multiple) {
        com_apartment apartment;
        com_ptr<IFileOpenDialog> panel;
        require_success(
            CoCreateInstance(CLSID_FileOpenDialog,
                             nullptr,
                             CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(panel.put())),
            "Windows: Failed to create the open-file dialog.");

        FILEOPENDIALOGOPTIONS options = FOS_FILEMUSTEXIST;
        if (allow_multiple)
            options |= FOS_ALLOWMULTISELECT;
        configure_dialog(panel.get(), dialog, options);

        file_dialog_response response;
        if (!show_dialog(panel.get(), dialog))
            return response;

        com_ptr<IShellItemArray> items;
        require_success(panel.get()->GetResults(items.put()),
                        "Windows: Failed to read selected files.");
        DWORD count = 0;
        require_success(items.get()->GetCount(&count),
                        "Windows: Failed to count selected files.");
        for (DWORD index = 0; index < count; ++index) {
            com_ptr<IShellItem> item;
            require_success(items.get()->GetItemAt(index, item.put()),
                            "Windows: Failed to read a selected file.");
            const std::filesystem::path path =
                path_from_item(item.get());
            if (!path.empty())
                response.paths.push_back(path);
        }
        response.accepted = !response.paths.empty();
        return response;
    }

    file_dialog_response show_save_file_dialog(
        const native::file_dialog &dialog,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite) {
        com_apartment apartment;
        com_ptr<IFileSaveDialog> panel;
        require_success(
            CoCreateInstance(CLSID_FileSaveDialog,
                             nullptr,
                             CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(panel.put())),
            "Windows: Failed to create the save-file dialog.");

        const FILEOPENDIALOGOPTIONS overwrite_option =
            static_cast<FILEOPENDIALOGOPTIONS>(FOS_OVERWRITEPROMPT);
        configure_dialog(panel.get(),
                         dialog,
                         confirm_overwrite ? overwrite_option : 0,
                         confirm_overwrite ? 0 : overwrite_option);
        if (!suggested_name.empty()) {
            const std::wstring name =
                windows::utf8_to_wide(suggested_name);
            require_success(panel.get()->SetFileName(name.c_str()),
                            "Windows: Failed to set the suggested "
                            "filename.");
        }
        if (!default_extension.empty()) {
            std::string extension = default_extension;
            if (!extension.empty() && extension.front() == '.')
                extension.erase(extension.begin());
            const std::wstring wide =
                windows::utf8_to_wide(extension);
            require_success(
                panel.get()->SetDefaultExtension(wide.c_str()),
                "Windows: Failed to set the default extension.");
        }

        file_dialog_response response;
        if (!show_dialog(panel.get(), dialog))
            return response;

        com_ptr<IShellItem> item;
        require_success(panel.get()->GetResult(item.put()),
                        "Windows: Failed to read the selected file.");
        const std::filesystem::path path = path_from_item(item.get());
        if (!path.empty())
            response.paths.push_back(path);
        response.accepted = !response.paths.empty();
        return response;
    }

    file_dialog_response show_directory_dialog(
        const native::file_dialog &dialog, bool allow_multiple) {
        com_apartment apartment;
        com_ptr<IFileOpenDialog> panel;
        require_success(
            CoCreateInstance(CLSID_FileOpenDialog,
                             nullptr,
                             CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(panel.put())),
            "Windows: Failed to create the directory dialog.");

        FILEOPENDIALOGOPTIONS options = FOS_PICKFOLDERS;
        if (allow_multiple)
            options |= FOS_ALLOWMULTISELECT;
        configure_dialog(panel.get(), dialog, options);

        file_dialog_response response;
        if (!show_dialog(panel.get(), dialog))
            return response;
        com_ptr<IShellItemArray> items;
        require_success(panel.get()->GetResults(items.put()),
                        "Windows: Failed to read selected folders.");
        DWORD count = 0;
        require_success(items.get()->GetCount(&count),
                        "Windows: Failed to count selected folders.");
        for (DWORD index = 0; index < count; ++index) {
            com_ptr<IShellItem> item;
            require_success(items.get()->GetItemAt(index, item.put()),
                            "Windows: Failed to read a selected folder.");
            const std::filesystem::path path =
                path_from_item(item.get());
            if (!path.empty())
                response.paths.push_back(path);
        }
        response.accepted = !response.paths.empty();
        return response;
    }
} // namespace windows

namespace native
{
    void file_dialog::cancel_native_dialog() {}
} // namespace native

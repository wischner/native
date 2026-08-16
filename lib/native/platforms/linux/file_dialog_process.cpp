//
// Implements a shell-free adapter to the standard Zenity and KDialog
// desktop file choosers for Linux toolkits without a chooser widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_process.h"

#include <cerrno>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{
    // Stores one child-process exit status and its standard output.
    struct process_result
    {
        int status = 127;
        std::string output;
    };

    // Execute arguments directly and capture standard output.
    process_result run_process(
        const std::vector<std::string> &arguments) {
        std::vector<char *> argv;
        argv.reserve(arguments.size() + 1);
        for (const std::string &argument : arguments)
            argv.push_back(const_cast<char *>(argument.c_str()));
        argv.push_back(nullptr);

        int descriptors[2] = {-1, -1};
        if (pipe(descriptors) != 0)
            throw std::runtime_error(
                "Linux: Failed to create file-dialog pipe.");

        const pid_t child = fork();
        if (child < 0) {
            close(descriptors[0]);
            close(descriptors[1]);
            throw std::runtime_error(
                "Linux: Failed to start a file dialog.");
        }

        if (child == 0) {
            close(descriptors[0]);
            if (dup2(descriptors[1], STDOUT_FILENO) < 0)
                _exit(126);
            close(descriptors[1]);

            execvp(argv.front(), argv.data());
            _exit(errno == ENOENT ? 127 : 126);
        }

        close(descriptors[1]);
        process_result result;
        char buffer[4096];
        for (;;) {
            const ssize_t count =
                read(descriptors[0], buffer, sizeof(buffer));
            if (count > 0) {
                result.output.append(buffer,
                                     static_cast<std::size_t>(count));
                continue;
            }
            if (count < 0 && errno == EINTR)
                continue;
            break;
        }
        close(descriptors[0]);

        int wait_status = 0;
        while (waitpid(child, &wait_status, 0) < 0) {
            if (errno != EINTR)
                throw std::runtime_error(
                    "Linux: Failed while waiting for a file dialog.");
        }
        if (WIFEXITED(wait_status))
            result.status = WEXITSTATUS(wait_status);
        else if (WIFSIGNALED(wait_status))
            result.status = 128 + WTERMSIG(wait_status);
        return result;
    }

    // Split newline-separated chooser output into non-empty paths.
    std::vector<std::string> split_paths(std::string output) {
        std::vector<std::string> paths;
        std::size_t begin = 0;
        while (begin < output.size()) {
            const std::size_t end = output.find('\n', begin);
            std::string path = output.substr(
                begin,
                end == std::string::npos ? std::string::npos
                                         : end - begin);
            if (!path.empty() && path.back() == '\r')
                path.pop_back();
            if (!path.empty())
                paths.push_back(std::move(path));
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        return paths;
    }

    // Combine the initial directory and suggested save filename.
    std::string initial_filename(
        const native::file_dialog &dialog,
        const std::string &suggested_name) {
        std::string value = dialog.get_initial_path();
        if (suggested_name.empty())
            return value;
        if (value.empty())
            return suggested_name;
        if (value.back() != '/')
            value.push_back('/');
        value += suggested_name;
        return value;
    }

    // Encode one portable filter for Zenity's command line.
    std::string zenity_filter(const native::file_filter &filter) {
        std::string value = filter.name;
        if (!value.empty())
            value += " |";
        for (const std::string &pattern : filter.patterns) {
            value.push_back(' ');
            value += pattern;
        }
        return value;
    }

    // Encode all portable filters for KDialog's command line.
    std::string kdialog_filters(
        const std::vector<native::file_filter> &filters) {
        std::string value;
        for (const native::file_filter &filter : filters) {
            if (!value.empty())
                value.push_back('\n');
            value += filter.name;
            value += " (";
            for (std::size_t index = 0;
                 index < filter.patterns.size();
                 ++index) {
                if (index != 0)
                    value.push_back(' ');
                value += filter.patterns[index];
            }
            value.push_back(')');
        }
        return value;
    }

    // Translate a chooser exit status and output into public state.
    linux::file_dialog_response response_from_process(
        process_result result) {
        linux::file_dialog_response response;
        if (result.status == 126 || result.status == 127) {
            response.outcome =
                linux::file_dialog_outcome::unavailable;
        } else if (result.status == 0) {
            response.paths = split_paths(std::move(result.output));
            response.outcome =
                response.paths.empty()
                    ? linux::file_dialog_outcome::cancelled
                    : linux::file_dialog_outcome::accepted;
        } else {
            response.outcome =
                linux::file_dialog_outcome::cancelled;
        }
        return response;
    }

    // Run Zenity with open or save options and portable filters.
    linux::file_dialog_response run_zenity(
        const native::file_dialog &dialog,
        bool save,
        bool allow_multiple,
        const std::string &suggested_name,
        bool confirm_overwrite) {
        std::vector<std::string> arguments{
            "zenity", "--file-selection", "--title=" +
                dialog.get_title()};
        const std::string initial =
            initial_filename(dialog, suggested_name);
        if (!initial.empty())
            arguments.push_back("--filename=" + initial);
        if (save)
            arguments.push_back("--save");
        if (save && confirm_overwrite)
            arguments.push_back("--confirm-overwrite");
        if (allow_multiple) {
            arguments.push_back("--multiple");
            arguments.emplace_back("--separator=\n");
        }
        for (const native::file_filter &filter :
             dialog.get_filters()) {
            arguments.push_back(
                "--file-filter=" + zenity_filter(filter));
        }
        return response_from_process(run_process(arguments));
    }

    // Run KDialog with open or save options and portable filters.
    linux::file_dialog_response run_kdialog(
        const native::file_dialog &dialog,
        bool save,
        bool allow_multiple,
        const std::string &suggested_name) {
        std::vector<std::string> arguments{
            "kdialog", "--title", dialog.get_title()};
        if (allow_multiple) {
            arguments.emplace_back("--multiple");
            arguments.emplace_back("--separate-output");
        }
        arguments.emplace_back(save ? "--getsavefilename"
                                    : "--getopenfilename");
        arguments.push_back(
            initial_filename(dialog, suggested_name));
        const std::string filters =
            kdialog_filters(dialog.get_filters());
        if (!filters.empty())
            arguments.push_back(filters);
        return response_from_process(run_process(arguments));
    }
} // namespace

namespace linux
{
    file_dialog_response show_open_file_dialog(
        const native::file_dialog &dialog, bool allow_multiple) {
        file_dialog_response response = run_zenity(
            dialog, false, allow_multiple, std::string(), false);
        if (response.outcome != file_dialog_outcome::unavailable)
            return response;
        return run_kdialog(
            dialog, false, allow_multiple, std::string());
    }

    file_dialog_response show_save_file_dialog(
        const native::file_dialog &dialog,
        const std::string &suggested_name,
        bool confirm_overwrite) {
        file_dialog_response response = run_zenity(
            dialog,
            true,
            false,
            suggested_name,
            confirm_overwrite);
        if (response.outcome != file_dialog_outcome::unavailable)
            return response;
        return run_kdialog(dialog, true, false, suggested_name);
    }

    std::string add_default_extension(
        const std::string &path, const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;

        const std::size_t slash = path.find_last_of("/\\");
        const std::size_t dot = path.find_last_of('.');
        if (dot != std::string::npos &&
            (slash == std::string::npos || dot > slash + 1))
            return path;

        std::string result = path;
        if (extension.front() != '.')
            result.push_back('.');
        result += extension;
        return result;
    }
} // namespace linux

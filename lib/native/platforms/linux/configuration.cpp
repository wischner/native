//
// Stores typed settings in an XDG per-application text file, with ordered
// XDG_CONFIG_DIRS defaults. All Linux toolkits, including GEMix, share this
// OS-level policy. Quoted UTF-8 strings, booleans and decimal numbers keep
// the file inspectable without introducing a toolkit or schema dependency.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../configuration_file.h"

#include <charconv>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace
{
    namespace fs = std::filesystem;
    using namespace native::detail;

    std::string environment(const char *name) {
        const char *value = std::getenv(name);
        return value ? value : "";
    }

    fs::path user_path(const std::string &application) {
        fs::path root(environment("XDG_CONFIG_HOME"));
        if (!root.is_absolute()) {
            root = environment("HOME");
            if (!root.is_absolute())
                throw std::runtime_error("Configuration requires an absolute XDG_CONFIG_HOME or HOME.");
            root /= ".config";
        }
        return root / application / "settings.ini";
    }

    std::vector<fs::path> system_paths(const std::string &application) {
        auto directories = environment("XDG_CONFIG_DIRS");
        if (directories.empty()) directories = "/etc/xdg";
        std::istringstream input(directories);
        std::vector<fs::path> result;
        std::string part;
        while (std::getline(input, part, ':')) {
            fs::path root(part);
            if (root.is_absolute())
                result.push_back(root / application / "settings.ini");
        }
        return result;
    }

    std::string_view trim(std::string_view text) {
        const auto first = text.find_first_not_of(" \t\r");
        if (first == std::string_view::npos) return {};
        const auto last = text.find_last_not_of(" \t\r");
        return text.substr(first, last - first + 1);
    }

    [[noreturn]] void malformed() {
        throw std::runtime_error("Malformed configuration settings.ini.");
    }

    std::string unquote(std::string_view text) {
        if (text.size() < 2 || text.back() != '"') malformed();
        std::string result;
        for (std::size_t i = 1; i + 1 < text.size(); ++i) {
            char c = text[i];
            if (c == '"' || static_cast<unsigned char>(c) < 32) malformed();
            if (c == '\\') {
                if (++i + 1 >= text.size()) malformed();
                switch (text[i]) {
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'x': {
                    if (i + 3 >= text.size()) malformed();
                    unsigned byte = 0;
                    const char *begin = text.data() + i + 1;
                    const auto parsed = std::from_chars(begin, begin + 2, byte, 16);
                    if (parsed.ec != std::errc{} || parsed.ptr != begin + 2) malformed();
                    c = static_cast<char>(byte);
                    i += 2;
                    break;
                }
                default: malformed();
                }
            }
            result.push_back(c);
        }
        return result;
    }

    configuration_value parse_value(std::string_view text) {
        if (text.empty()) malformed();
        if (text.front() == '"') return unquote(text);
        if (text == "true") return true;
        if (text == "false") return false;
        if (text.find_first_of(".eE") != std::string_view::npos) {
            double result = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                result, std::chars_format::general);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) malformed();
            return result;
        }
        std::int64_t result = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) malformed();
        return result;
    }

    void quote(std::ostream &output, const std::string &text) {
        output << '"';
        for (const char c : text) {
            switch (c) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32 || c == 127) {
                    constexpr char digits[] = "0123456789abcdef";
                    const auto byte = static_cast<unsigned char>(c);
                    output << "\\x" << digits[byte >> 4] << digits[byte & 15];
                } else output << c;
            }
        }
        output << '"';
    }

    class xdg_configuration final : public configuration_file_backend
    {
    public:
        explicit xdg_configuration(const std::string &application)
            : configuration_file_backend(user_path(application), system_paths(application)) {}

    private:
        configuration_entries decode(std::span<const char> bytes) override {
            std::istringstream input{std::string(bytes.begin(), bytes.end())};
            configuration_entries result;
            std::string line;
            while (std::getline(input, line)) {
                auto text = trim(line);
                if (text.empty() || text.front() == '#' || text.front() == ';') continue;
                const auto equals = text.find('=');
                if (equals == std::string_view::npos) malformed();
                const std::string key(trim(text.substr(0, equals)));
                auto value = parse_value(trim(text.substr(equals + 1)));
                if (!result.emplace(key, std::move(value)).second) malformed();
            }
            return result;
        }

        std::vector<char> encode(const configuration_entries &entries) override {
            std::ostringstream output;
            output.imbue(std::locale::classic());
            output << "# Native per-user settings\n";
            for (const auto &[key, value] : entries) {
                output << key << " = ";
                if (const auto *text = std::get_if<std::string>(&value)) quote(output, *text);
                else if (const auto *flag = std::get_if<bool>(&value)) output << (*flag ? "true" : "false");
                else if (const auto *integer = std::get_if<std::int64_t>(&value)) output << *integer;
                else output << std::scientific << std::setprecision(
                    std::numeric_limits<double>::max_digits10) << std::get<double>(value);
                output << '\n';
            }
            const auto text = output.str();
            return {text.begin(), text.end()};
        }
    };
}

namespace native::detail
{
    std::shared_ptr<configuration_backend> make_configuration_backend(
        const std::string &application) {
        return std::make_shared<xdg_configuration>(application);
    }
}

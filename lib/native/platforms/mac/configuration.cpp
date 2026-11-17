//
// Uses the per-user, any-host Core Foundation preference domain for one
// application identifier. Native strings, booleans and numbers remain native
// preference values rather than a private file format inside a plist.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../configuration_backend.h"

#include <CoreFoundation/CoreFoundation.h>
#include <memory>
#include <stdexcept>

namespace
{
    using namespace native::detail;

    struct cf_release
    {
        void operator()(CFTypeRef value) const { if (value) CFRelease(value); }
    };
    using cf_value = std::unique_ptr<const void, cf_release>;

    cf_value string(const std::string &text) {
        cf_value result(CFStringCreateWithBytes(kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(text.data()), text.size(),
            kCFStringEncodingUTF8, false));
        if (!result) throw std::runtime_error("Cannot create a preference string.");
        return result;
    }

    cf_value encode(const configuration_value &value) {
        if (const auto *text = std::get_if<std::string>(&value)) return string(*text);
        if (const auto *flag = std::get_if<bool>(&value))
            return cf_value(CFRetain(*flag ? kCFBooleanTrue : kCFBooleanFalse));
        if (const auto *integer = std::get_if<std::int64_t>(&value))
            return cf_value(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, integer));
        const double number = std::get<double>(value);
        return cf_value(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &number));
    }

    configuration_value decode(CFTypeRef value) {
        const auto type = CFGetTypeID(value);
        if (type == CFStringGetTypeID()) {
            const auto text = static_cast<CFStringRef>(value);
            const CFIndex length = CFStringGetLength(text);
            if (length > static_cast<CFIndex>(configuration_string_limit))
                throw std::runtime_error("Preference string exceeds size limit.");
            std::string result(static_cast<std::size_t>(
                CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8)), '\0');
            CFIndex used = 0;
            if (CFStringGetBytes(text, CFRangeMake(0, length), kCFStringEncodingUTF8,
                0, false, reinterpret_cast<UInt8 *>(result.data()), result.size(), &used) != length)
                throw std::runtime_error("Invalid preference UTF-8 string.");
            result.resize(static_cast<std::size_t>(used));
            return result;
        }
        if (type == CFBooleanGetTypeID())
            return static_cast<bool>(CFBooleanGetValue(static_cast<CFBooleanRef>(value)));
        if (type == CFNumberGetTypeID()) {
            const auto number = static_cast<CFNumberRef>(value);
            if (CFNumberIsFloatType(number)) {
                double result = 0;
                if (CFNumberGetValue(number, kCFNumberDoubleType, &result)) return result;
            } else {
                std::int64_t result = 0;
                if (CFNumberGetValue(number, kCFNumberSInt64Type, &result)) return result;
            }
        }
        throw std::runtime_error("Unsupported native preference value type.");
    }

    class mac_configuration final : public configuration_backend
    {
    public:
        explicit mac_configuration(const std::string &application)
            : _application(string(application)) {}

        std::optional<configuration_value> read(const std::string &key) override {
            synchronize();
            const auto name = string(key);
            cf_value value(CFPreferencesCopyValue(static_cast<CFStringRef>(name.get()),
                domain(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
            if (!value) return std::nullopt;
            return decode(value.get());
        }

        void write(const std::string &key,
            const std::optional<configuration_value> &value) override {
            synchronize();
            const auto name = string(key);
            const auto property = static_cast<CFStringRef>(name.get());
            cf_value previous(CFPreferencesCopyValue(property, domain(),
                kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
            cf_value encoded = value ? encode(*value) : cf_value{};
            if (value && !encoded) throw std::bad_alloc();
            CFPreferencesSetValue(property, encoded.get(), domain(),
                kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
            try {
                synchronize();
            } catch (...) {
                // Do not leave the failed candidate in CF's process cache.
                CFPreferencesSetValue(property, previous.get(), domain(),
                    kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
                throw;
            }
        }

    private:
        CFStringRef domain() const { return static_cast<CFStringRef>(_application.get()); }
        void synchronize() const {
            if (!CFPreferencesSynchronize(domain(), kCFPreferencesCurrentUser,
                                         kCFPreferencesAnyHost))
                throw std::runtime_error("Cannot synchronize application preferences.");
        }
        cf_value _application;
    };
}

namespace native::detail
{
    std::shared_ptr<configuration_backend> make_configuration_backend(
        const std::string &application) {
        return std::make_shared<mac_configuration>(application);
    }
}

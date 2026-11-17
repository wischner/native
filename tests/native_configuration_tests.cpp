//
// Tests the public settings value contract against an instrumented private
// backend: conversion, default ownership, cache expiry, immediate writes,
// failure propagation and immutable application/key binding.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/configuration.h>
#include <native/app.h>
#include "../lib/native/configuration_backend.h"

#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    using namespace native;
    using namespace std::chrono_literals;
    using native::detail::configuration_value;

    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    template<typename Exception = std::exception, typename Work>
    void rejects(Work work) {
        bool caught = false;
        try { work(); } catch (const Exception &) { caught = true; }
        expect(caught, "expected configuration operation to throw");
    }

    class memory_backend final : public detail::configuration_backend
    {
    public:
        std::map<std::string, configuration_value> values;
        int reads = 0;
        int writes = 0;
        bool fail_read = false;
        bool fail_write = false;

        std::optional<configuration_value> read(const std::string &key) override {
            ++reads;
            if (fail_read) throw std::runtime_error("simulated read failure");
            auto found = values.find(key);
            return found == values.end() ? std::optional<configuration_value>{} : found->second;
        }
        void write(const std::string &key,
            const std::optional<configuration_value> &value) override {
            ++writes;
            if (fail_write) throw std::runtime_error("simulated write failure");
            if (value) values.insert_or_assign(key, *value);
            else values.erase(key);
        }
    };
    std::map<std::string, std::shared_ptr<memory_backend>> stores;

    void checks() {
        rejects<std::logic_error>([] { configuration name("db.name"); });
        char executable[] = "/opt/bin/Mail App.exe";
        char *arguments[] = {executable, nullptr};
        app::argc = 1;
        app::argv = arguments;
        expect(app::get_name() == "Mail App.exe", "default name is argv[0]'s filename");
        configuration<int> automatic("launch.count", 0);
        automatic = 7;
        expect(std::get<std::int64_t>(stores.at("Mail App.exe")->values.at("launch.count")) == 7,
               "configuration needs no explicit application setup");
        for (const auto &invalid : {std::string{}, std::string("../outside"),
                 std::string("bad\\name"), std::string("."), std::string(".."),
                 std::string("bad\nname"), std::string("a\0b", 3),
                 std::string("\xc0\xaf"), std::string(256, 'a')})
            rejects<std::invalid_argument>([&] { app::set_name(invalid); });
        expect(app::get_name() == "Mail App.exe", "invalid names preserve application identity");
        app::set_name("Mail café");
        expect(app::get_name() == "Mail café", "explicit application name preserves UTF-8 and case");
        app::set_name("org.native.first");
        expect(automatic.get_value() == 7, "existing automatic binding keeps its namespace");
        configuration name("db.name", "contacts");
        auto backend = stores.at("org.native.first");
        static_assert(std::is_same_v<decltype(name), configuration<std::string>>);
        configuration<int> port("db.port", 5432);
        configuration<bool> enabled("db.enabled", true);
        configuration<double> scale("ui.scale", 1.25);
        configuration empty("db.empty");
        expect(backend->reads == 0 && backend->writes == 0, "construction is lazy");
        std::string text = name;
        int number = port;
        expect(text == "contacts" && number == 5432 && enabled &&
               scale.get_value() == 1.25 && empty.get_value().empty(), "typed defaults read naturally");
        expect(backend->writes == 0, "defaults are never silently persisted");
        name = "mail";
        port = 6000;
        enabled = false;
        scale = 2.5;
        expect(std::get<std::string>(backend->values.at("db.name")) == "mail" &&
               std::get<std::int64_t>(backend->values.at("db.port")) == 6000,
               "assignment persists immediately");
        std::ostringstream printed;
        printed << name << ':' << port;
        expect(printed.str() == "mail:6000", "streaming reads typed values");
        backend->values["db.port"] = std::int64_t{7000};
        const int before = backend->reads;
        expect(port.get_value() == 7000 && port.get_value() == 7000 &&
               backend->reads == before + 2, "zero timeout always rereads");

        port.set_timeout(1h);
        expect(port.get_value() == 7000, "first cached read loads storage");
        const int cached_reads = backend->reads;
        backend->values["db.port"] = std::int64_t{8000};
        expect(port.get_value() == 7000 && backend->reads == cached_reads,
               "cache hit performs no backend read");
        auto alias = port;
        expect(alias.get_value() == 7000 && backend->reads == cached_reads,
               "copied handles share their binding cache");
        expect(port.reload() == 8000, "reload bypasses a valid cache");
        alias = 8100;
        expect(port.get_value() == 8100, "assignment updates the shared cache");
        port.set_timeout(5ms);
        expect(port.get_value() == 8100, "changing timeout invalidates cache");
        backend->values["db.port"] = std::int64_t{8200};
        std::this_thread::sleep_for(20ms);
        expect(port.get_value() == 8200, "elapsed timeout rereads storage");
        port.set_timeout(1h).get_value();
        backend->fail_read = true;
        rejects([&] { port.reload(); });
        rejects([&] { port.get_value(); });
        backend->fail_read = false;
        expect(port.get_value() == 8200, "read failures do not poison cache recovery");
        backend->fail_write = true;
        rejects([&] { port = 9000; });
        expect(port.get_value() == 8200, "failed writes preserve cached value");
        backend->fail_write = false;
        port.reset();
        expect(port.get_value() == 5432, "reset restores fallback on next read");
        rejects<std::invalid_argument>([&] { port.set_timeout(-1ms); });
        expect(port.get_timeout() == 1h, "invalid timeout preserves policy");
        rejects<std::invalid_argument>([] { configuration<int> bad("db..port"); });
        rejects<std::invalid_argument>([] { configuration<int> bad("DB.port"); });
        rejects<std::invalid_argument>([] {
            configuration<int> bad("a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q");
        });
        rejects<std::invalid_argument>([] { configuration<int> bad("db.x", 1, -1ms); });
        rejects<std::invalid_argument>([&] { name = std::string("a\0b", 3); });
        rejects<std::invalid_argument>([&] { name = std::string("\xc0\xaf"); });
        rejects<std::invalid_argument>([&] { scale = std::numeric_limits<double>::infinity(); });
        rejects<std::invalid_argument>([&] { scale = std::nan(""); });

        backend->values["db.port"] = std::string("1234");
        rejects([&] { port.reload(); });
        backend->values["db.port"] = std::numeric_limits<std::int64_t>::max();
        rejects<std::out_of_range>([&] { port.reload(); });
        configuration<unsigned int> unsigned_value("db.port");
        backend->values["db.port"] = std::int64_t{-1};
        rejects<std::out_of_range>([&] { unsigned_value.get_value(); });
        configuration<unsigned long long> large("db.large");
        rejects<std::out_of_range>([&] { large = std::numeric_limits<unsigned long long>::max(); });
        configuration<float> small("ui.scale");
        backend->values["ui.scale"] = std::numeric_limits<double>::max();
        rejects<std::out_of_range>([&] { small.get_value(); });

        app::set_name("org.native.second");
        configuration destination("db.name", "second");
        destination = name;
        expect(destination.get_key() == "db.name" && destination.get_value() == "mail",
               "handle assignment copies a value without rebinding");
        name = "first-only";
        expect(destination.get_value() == "mail", "assigned destination stays in its own store");
        configuration later("db.name");
        expect(later.get_value() == "mail" && name.get_value() == "first-only",
               "changing app name does not redirect existing bindings");

        std::atomic<bool> failed = false;
        std::vector<std::jthread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&, i] {
                for (int n = 0; n < 20; ++n) {
                    try { alias = i; (void)port.get_value(); }
                    catch (...) { failed = true; }
                }
            });
        }
        workers.clear();
        expect(!failed, "shared setting reads/writes are thread-safe");
    }
}

namespace native::detail
{
    std::shared_ptr<configuration_backend> make_configuration_backend(const std::string &application) {
        auto &store = stores[application];
        if (!store) store = std::make_shared<memory_backend>();
        return store;
    }
}

int main() {
    try {
        checks();
        std::cout << "Configuration value checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

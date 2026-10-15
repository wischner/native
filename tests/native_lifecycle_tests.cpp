//
// Reproduces allocation rollback and callback-driven resource/object
// deletion. Fault injection covers shared acquisition without needing a
// native desktop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>

namespace
{
    int allocations_before_failure = -1;
}

// Fail one selected allocation after the inert window has been constructed.
void *operator new(std::size_t size) {
    if (allocations_before_failure == 0) {
        allocations_before_failure = -1;
        throw std::bad_alloc();
    }
    if (allocations_before_failure > 0)
        --allocations_before_failure;
    if (void *memory = std::malloc(size ? size : 1))
        return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept {
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace
{
    // Verify portable callbacks without creating toolkit resources.
    class inert_window final : public native::app_wnd
    {
    public:
        inert_window() : app_wnd("lifecycle", 0, 0, 2, 2) {}
        ~inert_window() override {
            destroy();
        }
        int creates = 0;
        bool fail_native = false;
        bool fail_cursor = false;
        bool dispatch_resize = false;
        std::function<void()> bounds_changed;
        void on_native_paint(native::wnd_paint_event event) override {
            native::wnd::on_native_paint(event);
        }

    protected:
        void on_bounds_changed() override {
            const auto callback = bounds_changed;
            if (callback) callback();
        }
        void create_native() override {
            ++creates;
            if (fail_native)
                throw std::runtime_error("native create failed");
        }
        void destroy_native() override {}
        void show_native() override {}
        void apply_cursor() override {
            if (fail_cursor)
                throw std::runtime_error("cursor failed");
        }
        void apply_bounds() override {}
        void apply_position() override {}
        void apply_dimensions() override {
            if (dispatch_resize)
                on_native_resize(get_dimensions());
        }
    };
    class inert_modal final : public native::modal_wnd
    {
    public:
        explicit inert_modal(native::app_wnd &owner)
            : modal_wnd(owner, "modal") {}
        ~inert_modal() override {
            destroy();
        }

    protected:
        void create_native() override {}
        void show_native() override {
            begin_modal_session();
        }
        void destroy_native() override {}
        void apply_cursor() override {}
    };
    void check(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    // Every shared acquisition failure must leave a retryable object.
    void allocation_rollback() {
        for (int index = 0; index < 32; ++index) {
            inert_window window;
            allocations_before_failure = index;
            bool failed = false;
            try {
                window.create();
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            allocations_before_failure = -1;
            if (!failed)
                return;
            check(!window.get_created(),
                  "allocation failure left created flag");
            window.create();
            check(window.get_created() && window.creates == 1,
                  "creation retry failed");
        }
        throw std::runtime_error(
            "allocation probe never reached successful creation");
    }
    // Protected hook failures follow the same rollback/retry transaction.
    void hook_rollback() {
        for (bool cursor : {false, true}) {
            inert_window window;
            window.fail_native = !cursor;
            window.fail_cursor = cursor;
            try {
                window.create();
            } catch (const std::runtime_error &) {
            }
            check(!window.get_created(), "hook failure left created flag");
            window.fail_native = window.fail_cursor = false;
            window.create();
            check(window.get_created(), "hook failure prevented retry");
        }
    }
    // No shared continuation or older signal may borrow the deleted owner.
    void callback_deletion() {
        auto window = std::make_unique<inert_window>();
        window->on_wnd_create.connect([&] {
            window.reset();
            return false;
        });
        window->create();
        check(!window, "create callback did not delete window");
        window = std::make_unique<inert_window>();
        window->create();
        native::img target(2, 2);
        window->on_wnd_paint.connect([&](auto) {
            window.reset();
            return false;
        });
        window->on_native_paint({native::rect(0, 0, 2, 2), target.get_gpx()});
        check(!window, "paint callback did not delete window");
        window = std::make_unique<inert_window>();
        window->create();
        window->dispatch_resize = true;
        window->on_wnd_resize.connect([&](auto) {
            window.reset();
            return false;
        });
        window->set_dimensions(native::size(3, 3));
        check(!window, "resize callback did not delete window");
        window = std::make_unique<inert_window>();
        window->create();
        window->bounds_changed = [&] { window.reset(); };
        window->set_border_sides(native::border_sides::none);
        check(!window, "border callback did not delete window");
        auto editor = std::make_unique<native::code_edit>();
        editor->on_change.connect([&](const auto &) {
            editor.reset();
            return false;
        });
        editor->on_native_text("changed");
        check(!editor, "source change callback did not delete editor");
        auto image = std::make_unique<native::img>(2, 2);
        {
            auto saved = image->get_gpx().save_state();
            image.reset();
        }
    }
    // Completion observes a fully released generation and may recreate or
    // delete it.
    void modal_completion() {
        inert_window root;
        root.create();
        auto dialog = std::make_unique<inert_modal>(root);
        dialog->create();
        dialog->show();
        unsigned completions = 0;
        dialog->on_modal_close.connect([&](auto) {
            ++completions;
            check(!dialog->get_created(),
                  "modal completion preceded teardown");
            if (completions == 1) {
                dialog->create();
                dialog->show();
            } else
                dialog.reset();
            return false;
        });
        dialog->close(native::dialog_result::accepted);
        check(dialog && dialog->get_created() && completions == 1,
              "returning old teardown destroyed recreated modal");
        dialog->close(native::dialog_result::cancelled);
        check(!dialog && completions == 2 && !root.get_active_modal(),
              "modal completion deletion left stale owner state");
    }

} // namespace
int main() {
    try {
        allocation_rollback();
        hook_rollback();
        callback_deletion();
        modal_completion();
        std::cout << "Window rollback and callback lifetime passed.\n";
        return 0;
    } catch (const std::exception &error) {
        allocations_before_failure = -1;
        std::cerr << error.what() << '\n';
        return 1;
    }
}

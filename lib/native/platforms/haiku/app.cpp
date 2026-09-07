//
// Implements the Haiku application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>
#include <Application.h>
#include <Handler.h>
#include <Messenger.h>
#include <mutex>
#include <iostream>
#include "globals.h"
#include "../../post_backend.h"

namespace
{
    std::mutex posted_mutex;
    BMessenger posted_target;
    constexpr uint32 posted_message = 'npst';

    // The messenger queues work on BApplication's looper from any thread.
    void wake_posted_work() {
        const std::lock_guard<std::mutex> lock(posted_mutex);
        if (posted_target.IsValid()) posted_target.SendMessage(posted_message);
    }

    class posted_receiver final : public BHandler
    {
    public:
        void MessageReceived(BMessage *message) override {
            if (message->what != posted_message) { BHandler::MessageReceived(message); return; }
            native::detail::drain_posted_work();
            if (native::app::main_wnd() && !native::app::main_wnd()->get_created())
                be_app->PostMessage(B_QUIT_REQUESTED);
        }
    };
}

namespace native
{

    int app::main_loop() {
        if (!haiku::global_app) {
            std::cerr << "Haiku: No BApplication instance available!"
                      << std::endl;
            return 1;
        }

        posted_receiver receiver;
        haiku::global_app->AddHandler(&receiver);
        {
            const std::lock_guard<std::mutex> lock(posted_mutex);
            posted_target = BMessenger(&receiver, haiku::global_app);
        }
        detail::set_loop_wake(wake_posted_work);
        detail::drain_posted_work();
        if (!app::main_wnd() || app::main_wnd()->get_created())
            haiku::global_app->Run();
        detail::set_loop_wake(nullptr);
        {
            const std::lock_guard<std::mutex> lock(posted_mutex);
            posted_target = BMessenger();
        }
        haiku::global_app->RemoveHandler(&receiver);
        delete haiku::global_app;
        haiku::global_app = nullptr;

        return 0;
    }

} // namespace native

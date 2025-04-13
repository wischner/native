#pragma once

#include <Application.h>
#include <Window.h>
#include <native.h>
#include <bindings.h>

namespace haiku
{
    extern BApplication *global_app;
    extern native::bindings<BWindow *, native::wnd *> wnd_bindings;
}

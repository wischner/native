#include "native.h"

int program(int argc, char *argv[])
{
    return native::app::run(native::app_wnd("Hello World!"));
}
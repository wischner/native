#include "nice.hpp"

using namespace nice;

void program() {
    app::run(
        app_wnd("Hello World!", { 640, 400 })
    );
}
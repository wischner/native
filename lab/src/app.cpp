//
// app.cpp
// 
// Class encapsulating application entry point.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 05.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    int app::ret_code = 0;
    int app::argc = 0;
    char **app::argv = nullptr;
    bool app::primary_ = false;
    app_instance app::instance_;

    app_instance app::instance() {
        return instance_;
    }

    void app::instance(app_instance instance) {
        instance_ = instance;
    }

    std::string app::name() {
        return std::filesystem::path(argv[0]).stem().string();
    }
//{{END.DEF}}

}
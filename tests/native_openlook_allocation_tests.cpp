//
// Reproduces SDK-only allocation behavior without linking Native.
// Controls isolate text selection, notebook and drop-interest teardown.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/openmenu.h>
#include <cstdlib>
int main(int argc, char **argv) {
    const int scenario = argc > 1 ? std::atoi(argv[1]) : 0;
    xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, nullptr);
    Frame frame = static_cast<Frame>(xv_create(XV_NULL, FRAME, nullptr));
    Panel panel = static_cast<Panel>(xv_create(frame, PANEL, nullptr));
    if (scenario == 1)
        xv_create(panel, PANEL_TEXT, PANEL_VALUE, "text", nullptr);
    if (scenario == 2) {
        Menu menu =
            static_cast<Menu>(xv_create(XV_NULL, MENU_COMMAND_MENU, nullptr));
        Menu_item item = static_cast<Menu_item>(xv_create(
            XV_NULL, MENUITEM, MENU_RELEASE, MENU_STRING, "item", nullptr));
        xv_set(menu, MENU_APPEND_ITEM, item, nullptr);
        xv_destroy(menu);
    }
    xv_destroy(frame);
}

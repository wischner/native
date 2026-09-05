//
// Exercises real Win32 peers, default native painting, child clipping,
// editor notifications, splitter capture, and owner-modal exclusion.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "../lib/native/platforms/windows/globals.h"

#include <iostream>
#include <stdexcept>
#include <uxtheme.h>

namespace
{
    class decorated_button final : public native::button
    {
    public:
        decorated_button() : native::button("Derived", 120, 10, 100, 28) {}
        int paints = 0;

    protected:
        void draw_background(native::gpx &graphics,
                             native::theme &appearance,
                             const native::rect &bounds,
                             const native::theme::state &state) override {
            ++paints;
            native::button::draw_background(
                graphics, appearance, bounds, state);
        }
    };

    void expect(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    HWND handle(native::wnd &window) {
        return windows::wnd_bindings.handle_from_object(&window);
    }

    void attach(native::wnd &child, native::wnd &parent) {
        child.set_parent(&parent);
        child.create();
        child.show();
    }

    void pump() {
        MSG message{};
        int count = 0;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message != WM_QUIT) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            expect(++count < 10000, "unbounded repaint/message backlog");
        }
    }

    LRESULT custom_draw(native::wnd &child) {
        NMLVCUSTOMDRAW drawing{};
        drawing.nmcd.hdr.hwndFrom = handle(child);
        drawing.nmcd.hdr.code = NM_CUSTOMDRAW;
        drawing.nmcd.dwDrawStage = CDDS_PREPAINT;
        drawing.nmcd.hdc = GetDC(handle(child));
        const LRESULT result = SendMessageW(GetParent(handle(child)),
            WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&drawing));
        ReleaseDC(handle(child), drawing.nmcd.hdc);
        return result;
    }

    void run() {
        native::app_wnd root("Windows peer regression", 30, 30, 650, 550);
        root.on_wnd_paint.connect([](native::wnd_paint_event event) {
            for (int x = 0; x < 10; ++x)
                event.g.set_pen(1).set_ink(native::rgba(x * 20, 50, 80, 255))
                    .draw_line(native::point(600 + x, 1),
                               native::point(600 + x, 20));
            return true;
        });
        root.create();
        root.show();
        native::button button("Activate", 10, 10, 100, 28);
        native::check check("Editing", 10, 45, 100, 24);
        native::radio radio("Compact", 120, 45, 100, 24);
        native::text_edit edit("", native::text_edit_mode::multi_line,
                               10, 80, 200, 60);
        native::list list({"First", "Second"}, 230, 10, 130, 80);
        native::tree_view tree({}, 370, 10, 200, 130);
        native::icon_view icons({}, 230, 100, 130, 80);
        native::table_view table(10, 190, 560, 120);
        native::combo_box combo({"First", "Second"},
            native::combo_box_style::editable, 10, 150, 200, 24);
        for (native::wnd *child : std::vector<native::wnd *>{&button,
                &check, &radio, &edit, &list, &tree, &icons, &table,
                &combo})
            attach(*child, root);
        decorated_button decorated;
        attach(decorated, root);
        expect(decorated.paints > 0,
               "derived controls retain protected drawing overrides");
        auto appearance = native::theme::create(root.get_gpx());
        expect(SendMessageW(handle(list), LB_GETITEMHEIGHT, 0, 0) ==
                   appearance->defaults().list_item_height,
               "theme list rows use the native LISTBOX height");

        expect((GetWindowLongPtrW(handle(button), GWL_STYLE) & BS_TYPEMASK)
                   == BS_PUSHBUTTON, "button must use native painting");
        expect((GetWindowLongPtrW(handle(check), GWL_STYLE) & BS_TYPEMASK)
                   == BS_CHECKBOX, "check must use native painting");
        expect((GetWindowLongPtrW(handle(radio), GWL_STYLE) & BS_TYPEMASK)
                   == BS_RADIOBUTTON, "radio must use native painting");
        // An empty Tree-View need not have opened its lazy theme handle.
        // Query the active visual-style service, not that private cache.
        HTHEME visual = OpenThemeData(handle(button), L"Button");
        expect(visual != nullptr,
               "native visual-style service must be available");
        CloseThemeData(visual);
        expect(custom_draw(tree) == CDRF_DODEFAULT,
               "stock tree must retain native drawing");
        expect(custom_draw(icons) == CDRF_DODEFAULT,
               "stock icon view must retain native drawing");
        NMCUSTOMDRAW header{};
        header.hdr.hwndFrom = ListView_GetHeader(handle(table));
        header.hdr.code = NM_CUSTOMDRAW;
        header.dwDrawStage = CDDS_PREPAINT;
        expect(SendMessageW(handle(root), WM_NOTIFY, 0,
                   reinterpret_cast<LPARAM>(&header)) == CDRF_DODEFAULT,
               "stock table header must retain native drawing");

        int clicks = 0;
        button.on_click.connect([&clicks] { ++clicks; return true; });
        SendMessageW(handle(button), BM_CLICK, 0, 0);
        expect(clicks == 1, "native button click must emit once");
        SendMessageW(handle(check), BM_CLICK, 0, 0);
        expect(check.get_checked(), "native check must update C++ state");
        SendMessageW(handle(check), BM_CLICK, 0, 0);
        expect(!check.get_checked(), "native check must toggle off");
        SendMessageW(handle(radio), BM_CLICK, 0, 0);
        expect(radio.get_selected(), "native radio must update C++ state");
        SetFocus(handle(edit));
        SendMessageW(handle(edit), WM_CHAR, 'A', 0);
        SendMessageW(handle(edit), WM_CHAR, 'B', 0);
        expect(edit.get_text() == "AB", "EDIT must receive keyboard input");
        edit.select_all();
        edit.copy();
        edit.set_text("");
        edit.paste();
        expect(edit.get_text() == "AB", "native editor clipboard round trip");
        SendMessageW(handle(combo), CB_SETCURSEL, 1, 0);
        SendMessageW(handle(root), WM_COMMAND,
            MAKEWPARAM(0, CBN_SELCHANGE),
            reinterpret_cast<LPARAM>(handle(combo)));
        expect(combo.get_selected_index() == 1 &&
                   combo.get_text() == "Second", "combo selection routing");

        native::list left({"Left"}, 0, 0, 100, 60);
        native::list right({"Right"}, 0, 0, 100, 60);
        native::split_view split(left, right,
            native::split_orientation::horizontal, 10, 330, 550, 80);
        attach(split, root);
        const auto grip = split.get_splitter_bounds();
        const float before = split.get_ratio();
        SendMessageW(handle(split), WM_LBUTTONDOWN, MK_LBUTTON,
                     MAKELPARAM(grip.p.x + 1, 20));
        SendMessageW(handle(split), WM_MOUSEMOVE, MK_LBUTTON,
                     MAKELPARAM(grip.p.x + 55, 20));
        SendMessageW(handle(split), WM_LBUTTONUP, 0,
                     MAKELPARAM(grip.p.x + 55, 20));
        expect(split.get_ratio() > before, "splitter drag changes ratio");
        expect(GetCapture() != handle(split), "splitter releases capture");

        native::canvas surface(10, 430, 100, 50);
        surface.on_wnd_paint.connect([](native::wnd_paint_event event) {
            event.g.clear(native::rgba(20, 160, 40, 255));
            return true;
        });
        attach(surface, root);
        pump();
        const DWORD objects = GetGuiResources(GetCurrentProcess(),
                                              GR_GDIOBJECTS);
        for (int pass = 0; pass < 100; ++pass) {
            root.invalidate();
            UpdateWindow(handle(root));
        }
        HDC dc = GetDC(handle(surface));
        expect(GetPixel(dc, 20, 20) == RGB(20, 160, 40),
               "parent repaint must not erase child pixels");
        ReleaseDC(handle(surface), dc);
        expect(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS)
                   <= objects + 10, "repaints must not leak GDI objects");
        pump();

        native::modeless_wnd sibling(root, "Modeless", 80, 80, 200, 100);
        sibling.create();
        sibling.show();
        expect(GetWindow(handle(sibling), GW_OWNER) == nullptr,
               "modeless peers must not be pinned above their C++ owner");
        SetWindowPos(handle(root), HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
        bool owner_above = false;
        for (HWND window = GetTopWindow(nullptr); window;
             window = GetWindow(window, GW_HWNDNEXT)) {
            if (window == handle(root)) {
                owner_above = true;
                break;
            }
            if (window == handle(sibling))
                break;
        }
        expect(owner_above, "main window can cover modeless window");
        native::modal_wnd modal(root, "Modal", 100, 100, 200, 100);
        modal.create();
        modal.show();
        expect(!IsWindowEnabled(handle(root)), "modal disables owner");
        expect(!IsWindowEnabled(handle(sibling)), "modal disables siblings");
        modal.close(native::dialog_result::cancelled);
        expect(IsWindowEnabled(handle(root)) &&
                   IsWindowEnabled(handle(sibling)),
               "modal close restores owner and siblings");
        sibling.destroy();
        root.destroy();
    }

    void test_grid_tabs_and_chrome() {
        native::app_wnd root("Grid, tabs and chrome", 40, 40, 600, 500);
        root.create();
        root.show();
        native::table_store store({
            {1, {{1, {"First"}}, {2, {"A"}}}},
            {2, {{1, {"Second"}}, {2, {"B"}}}}},
            {{1, "Group", 0, 2, true, true}});
        native::table_view table(10, 10, 360, 130);
        table.set_data_mode(native::table_data_mode::materialized);
        table.set_columns({{1, "Name", 150}, {2, "Value", 150}});
        table.set_model(&store);
        attach(table, root);
        const auto row_edge = [](native::table_view &table, int item = 0) {
            table.invalidate();
            UpdateWindow(handle(table));
            RECT row{};
            expect(ListView_GetItemRect(handle(table), item, &row, LVIR_BOUNDS),
                   "native grouped row bounds");
            HDC dc = GetDC(handle(table));
            const COLORREF color = GetPixel(dc, 60, row.bottom - 1);
            ReleaseDC(handle(table), dc);
            return color;
        };
        table.set_grid_lines(native::table_grid_lines::both);
        expect(row_edge(table) == GetSysColor(COLOR_3DSHADOW),
               "grouped native rows must show requested grid edges");
        table.set_grid_lines(native::table_grid_lines::none);
        expect(row_edge(table) != GetSysColor(COLOR_3DSHADOW),
               "turning grid lines off removes the supplement");
        table.set_grid_lines(native::table_grid_lines::horizontal);
        expect(row_edge(table) == GetSysColor(COLOR_3DSHADOW),
               "horizontal-only grid lines remain supported");
        table.destroy();

        native::table_store virtual_store;
        for (int index = 1; index <= 50; ++index)
            virtual_store.add_row({static_cast<native::table_row_id>(index),
                {{1, {"Virtual row"}}, {2, {"Value"}}}});
        native::table_view virtual_table(10, 10, 360, 130);
        virtual_table.set_data_mode(native::table_data_mode::virtualized);
        virtual_table.set_columns({{1, "Name", 150}, {2, "Value", 150}});
        virtual_table.set_model(&virtual_store);
        attach(virtual_table, root);
        expect((GetWindowLongPtrW(handle(virtual_table), GWL_STYLE) &
                   LVS_OWNERDATA) != 0, "grid regression uses owner-data rows");
        for (auto lines : {native::table_grid_lines::both,
                native::table_grid_lines::none,
                native::table_grid_lines::horizontal,
                native::table_grid_lines::vertical}) {
            virtual_table.set_grid_lines(lines);
            const bool horizontal = lines == native::table_grid_lines::both ||
                lines == native::table_grid_lines::horizontal;
            expect((row_edge(virtual_table) == GetSysColor(COLOR_3DSHADOW))
                       == horizontal, "virtual horizontal grid toggling");
            RECT row{}, column{};
            ListView_GetItemRect(handle(virtual_table), 0, &row, LVIR_BOUNDS);
            HWND header = ListView_GetHeader(handle(virtual_table));
            Header_GetItemRect(header, 0, &column);
            MapWindowPoints(header, handle(virtual_table),
                reinterpret_cast<POINT *>(&column), 2);
            HDC dc = GetDC(handle(virtual_table));
            const COLORREF edge = GetPixel(dc, column.right - 1, row.top + 3);
            ReleaseDC(handle(virtual_table), dc);
            const bool vertical = lines == native::table_grid_lines::both ||
                lines == native::table_grid_lines::vertical;
            expect((edge == GetSysColor(COLOR_3DSHADOW)) == vertical,
                   "virtual vertical grid toggling");
        }
        virtual_table.set_grid_lines(native::table_grid_lines::both);
        ListView_EnsureVisible(handle(virtual_table), 40, FALSE);
        expect(row_edge(virtual_table, 40) == GetSysColor(COLOR_3DSHADOW),
               "virtual grid survives scrolling to newly requested rows");

        native::list first({"First page"}, 0, 0, 100, 40);
        native::list second({"Second page"}, 0, 0, 100, 40);
        native::tab_view tabs(10, 160, 300, 140);
        tabs.add_item("Left", first);
        tabs.add_item("Second", second);
        attach(tabs, root);
        for (auto placement : {native::tab_placement::top,
                native::tab_placement::bottom, native::tab_placement::left,
                native::tab_placement::right}) {
            tabs.set_tab_placement(placement);
            UpdateWindow(handle(tabs));
            expect(GetWindowTheme(handle(tabs)) == nullptr,
                   "all four edges use the same native classic renderer");
            expect((GetWindowLongPtrW(handle(tabs), GWL_STYLE) &
                       TCS_OWNERDRAWFIXED) == 0, "tabs are not owner drawn");
            const auto content = tabs.get_content_bounds();
            expect(content.d.w > 0 && content.d.h > 0 &&
                       content.p.x >= 4 && content.p.y >= 4,
                   "native page insets preserve the frame");
            tabs.set_selected_index(1);
            expect(second.get_created(), "all tab edges switch native pages");
            tabs.set_selected_index(0);
        }

        for (auto placement : {native::tab_placement::left,
                               native::tab_placement::right}) {
            tabs.set_tab_placement(placement);
            HWND window = handle(tabs);
            InvalidateRect(window, nullptr, TRUE);
            UpdateWindow(window);
            HDC dc = GetDC(window);
            HGDIOBJ previous = SelectObject(dc,
                reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
            for (int index = 0; index < 2; ++index) {
                RECT item{};
                TabCtrl_GetItemRect(window, index, &item);
                SIZE text{};
                GetTextExtentPoint32W(dc, index == 0 ? L"Left" : L"Second",
                                      index == 0 ? 4 : 6, &text);
                const int padding = item.bottom - item.top - text.cx;
                expect(padding >= 4 && padding <= 16,
                       "short vertical labels have compact native padding");
            }
            SelectObject(dc, previous);
            ReleaseDC(window, dc);
        }

        native::status_bar status(root, 24);
        status.set_text("Reserved status strip");
        native::button overflow("Overlapping absolute control", 10, 310,
                                330, 140);
        attach(overflow, root);
        for (int height : {420, 360, 480, 330}) {
            root.set_dimensions(native::size(600, height));
            root.invalidate();
            UpdateWindow(handle(root));
            HWND bar = FindWindowExW(handle(root), nullptr,
                                     STATUSCLASSNAMEW, nullptr);
            expect(bar != nullptr, "native status bar exists");
            expect(GetWindow(handle(root), GW_CHILD) == bar,
                   "status strip stays above document controls after resize");
            expect((GetWindowLongPtrW(handle(overflow), GWL_STYLE) &
                       WS_CLIPSIBLINGS) != 0,
                   "native controls cannot repaint through the status strip");
            const auto bounds = status.get_bounds();
            const POINT point{20, bounds.p.y + 10};
            expect(ChildWindowFromPointEx(handle(root), point,
                       CWP_SKIPINVISIBLE) == bar,
                   "status strip owns input in its reserved area");
            overflow.invalidate();
            UpdateWindow(handle(overflow));
            pump();
        }
        root.destroy();
    }
}

int main() {
    try {
        run();
        test_grid_tabs_and_chrome();
        std::cout << "Windows native-peer regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

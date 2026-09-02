# lib/native/platforms/windows/CMakeLists.txt

target_sources(native PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/theme.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wnd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/app.cpp
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/font.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_LIST_DIR}/img.cpp
    ${CMAKE_CURRENT_LIST_DIR}/image_codec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/globals.cpp
    ${CMAKE_CURRENT_LIST_DIR}/menu.cpp
    ${CMAKE_CURRENT_LIST_DIR}/message_box.cpp
    ${CMAKE_CURRENT_LIST_DIR}/button.cpp
    ${CMAKE_CURRENT_LIST_DIR}/combo_box.cpp
    ${CMAKE_CURRENT_LIST_DIR}/check.cpp
    ${CMAKE_CURRENT_LIST_DIR}/radio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/list.cpp
    ${CMAKE_CURRENT_LIST_DIR}/accordion.cpp
    ${CMAKE_CURRENT_LIST_DIR}/icon_view.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tree_view.cpp
    ${CMAKE_CURRENT_LIST_DIR}/table_view.cpp
    ${CMAKE_CURRENT_LIST_DIR}/clipboard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/text_edit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/code_edit.cpp
    ${CMAKE_CURRENT_LIST_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/directory_dialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/save_file_dialog.cpp
)

# Windows windowing, graphics, registry, shell, COM, and codec libraries.
target_link_libraries(native PRIVATE
    user32
    gdi32
    msimg32
    advapi32
    shell32
    ole32
    gdiplus
    comctl32
    uxtheme
)

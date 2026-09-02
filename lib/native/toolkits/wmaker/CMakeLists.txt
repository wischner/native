# Configure the Linux Window Maker backend through WINGs and Xlib.
find_package(PkgConfig REQUIRED)
pkg_check_modules(WINGS REQUIRED WINGs)
pkg_check_modules(XRANDR REQUIRED xrandr)
find_package(X11 REQUIRED)

set(LINUX_PLATFORM_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/linux)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/combo_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/directory_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collections.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tab_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/split_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/message_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${LINUX_PLATFORM_SOURCE_DIR}/x_image.cpp
)

target_include_directories(native PRIVATE
    ${X11_INCLUDE_DIR}
    ${WINGS_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
)

target_link_directories(native PUBLIC
    ${WINGS_LIBRARY_DIRS}
)

target_compile_options(native PRIVATE
    ${WINGS_CFLAGS_OTHER}
)

target_link_libraries(native PRIVATE
    ${X11_LIBRARIES}
    ${WINGS_LIBRARIES}
    ${XRANDR_LIBRARIES}
)

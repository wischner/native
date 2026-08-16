# --- Find packages: Athena, Xt, pixman, X11, and Xrandr. ------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(XAW REQUIRED xaw7)
pkg_check_modules(XT REQUIRED xt)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
pkg_check_modules(XRANDR REQUIRED xrandr)
find_package(X11 REQUIRED)

# --- Define target. -------------------------------------------------
set(LINUX_PLATFORM_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/linux)
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.cpp
    ${LINUX_PLATFORM_SOURCE_DIR}/file_dialog_process.cpp
    ${LINUX_PLATFORM_SOURCE_DIR}/x_image.cpp
)

# --- Add libs. ------------------------------------------------------
target_include_directories(native PRIVATE
    ${X11_INCLUDE_DIR}
    ${XAW_INCLUDE_DIRS}
    ${XT_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
)
target_link_libraries(native PRIVATE
    ${X11_LIBRARIES}
    ${XAW_LIBRARIES}
    ${XT_LIBRARIES}
    ${PIXMAN_LIBRARIES}
    ${XRANDR_LIBRARIES}
)

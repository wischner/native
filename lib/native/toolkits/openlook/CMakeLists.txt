# Configure the Linux OPEN LOOK backend through XView, OLGX, and Xlib.
find_package(PkgConfig REQUIRED)
pkg_check_modules(XVIEW QUIET xview)
pkg_check_modules(OLGX QUIET olgx)
pkg_check_modules(XRANDR REQUIRED xrandr)
find_package(X11 REQUIRED)

# Linux build images publish XView and OLGX through pkg-config. Tribblix
# installs the same development interfaces in the system include and library
# directories without .pc files, so retain a native CMake discovery path for
# that supported runtime.
if(NOT XVIEW_FOUND)
    find_path(XVIEW_INCLUDE_DIR
        NAMES xview/xview.h
        REQUIRED)
    find_library(XVIEW_LIBRARY
        NAMES xview
        REQUIRED)
    set(XVIEW_INCLUDE_DIRS ${XVIEW_INCLUDE_DIR})
    set(XVIEW_LIBRARIES ${XVIEW_LIBRARY})
    get_filename_component(XVIEW_LIBRARY_DIR
        ${XVIEW_LIBRARY} DIRECTORY)
    set(XVIEW_LIBRARY_DIRS ${XVIEW_LIBRARY_DIR})
endif()

if(NOT OLGX_FOUND)
    find_path(OLGX_INCLUDE_DIR
        NAMES olgx/olgx.h
        REQUIRED)
    find_library(OLGX_LIBRARY
        NAMES olgx
        REQUIRED)
    set(OLGX_INCLUDE_DIRS ${OLGX_INCLUDE_DIR})
    set(OLGX_LIBRARIES ${OLGX_LIBRARY})
    get_filename_component(OLGX_LIBRARY_DIR
        ${OLGX_LIBRARY} DIRECTORY)
    set(OLGX_LIBRARY_DIRS ${OLGX_LIBRARY_DIR})
endif()

set(LINUX_PLATFORM_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/linux)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collections.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window_position.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xview_init.cpp
    ${LINUX_PLATFORM_SOURCE_DIR}/x_image.cpp
)

target_include_directories(native PRIVATE
    ${X11_INCLUDE_DIR}
    ${XVIEW_INCLUDE_DIRS}
    ${OLGX_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
)

target_link_directories(native PUBLIC
    ${XVIEW_LIBRARY_DIRS}
    ${OLGX_LIBRARY_DIRS}
)

target_link_libraries(native PRIVATE
    ${XVIEW_LIBRARIES}
    ${OLGX_LIBRARIES}
    ${X11_LIBRARIES}
    ${XRANDR_LIBRARIES}
)

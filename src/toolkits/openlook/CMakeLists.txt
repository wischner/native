message(STATUS "Configuring OpenLook Toolkit...")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
pkg_check_modules(XRANDR REQUIRED xrandr)
find_package(X11 REQUIRED)
pkg_check_modules(XT REQUIRED xt)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../x11/img.cpp
)

target_include_directories(native PRIVATE
    ${X11_INCLUDE_DIR}
    ${PIXMAN_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
    ${XT_INCLUDE_DIRS}
)

target_link_libraries(native PRIVATE
    ${X11_LIBRARIES}
    ${PIXMAN_LIBRARIES}
    ${XRANDR_LIBRARIES}
    ${XT_LIBRARIES}
)

target_compile_definitions(native PRIVATE OPENLOOK)

message(STATUS "OpenLook Toolkit successfully configured!")

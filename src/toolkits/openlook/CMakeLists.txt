message(STATUS "Configuring OpenLook Toolkit...")

find_package(PkgConfig REQUIRED)
pkg_check_modules(XRANDR REQUIRED xrandr)
pkg_check_modules(XT REQUIRED xt)
find_package(X11 REQUIRED)

if(NOT TARGET native)
    message(FATAL_ERROR "Target 'native' does not exist!")
endif()

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
)

target_include_directories(native PRIVATE
    /usr/openwin/include
    ${X11_INCLUDE_DIR}
    ${XRANDR_INCLUDE_DIRS}
    ${XT_INCLUDE_DIRS}
)

target_link_libraries(native PRIVATE
    ${X11_LIBRARIES}
    ${X11_Xext_LIB}
    ${XRANDR_LIBRARIES}
    ${XT_LIBRARIES}
    /usr/openwin/lib/libxview.so
    /usr/openwin/lib/libolgx.so
    Xmu
    Xpm
    m
    util
    tirpc
)

target_compile_definitions(native PRIVATE OPENLOOK)
target_link_options(native PUBLIC
    "-Wl,--disable-new-dtags"
    "-Wl,-rpath,${CMAKE_BINARY_DIR}/runtime-libs"
)

add_custom_target(openlook-runtime-libs ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/runtime-libs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different /usr/openwin/lib/libxview.so.3 ${CMAKE_BINARY_DIR}/runtime-libs/libxview.so.3
    COMMAND ${CMAKE_COMMAND} -E copy_if_different /usr/openwin/lib/libolgx.so.3 ${CMAKE_BINARY_DIR}/runtime-libs/libolgx.so.3
)
add_dependencies(native openlook-runtime-libs)

message(STATUS "OpenLook Toolkit successfully configured!")

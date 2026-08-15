# globals.cpp contains native Objective-C types but keeps the architectural
# globals.cpp/globals.h naming convention.
set_property(
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    DIRECTORY ${PROJECT_SOURCE_DIR}/lib/native
    PROPERTY COMPILE_OPTIONS "-x;objective-c++"
)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/control_paint_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.mm
    globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/native_window.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/native_app.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/font.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/button.mm
)

target_link_libraries(native PRIVATE
    "-framework AppKit"
    "-framework Foundation"
    "-framework CoreFoundation"
)

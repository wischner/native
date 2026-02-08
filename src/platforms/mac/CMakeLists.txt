target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/NativeWindow.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/NativeApp.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
)

target_link_libraries(native
    "-framework AppKit"
    "-framework Foundation"
    "-framework CoreFoundation"
)

# globals.cpp contains native Objective-C types but keeps the architectural
# globals.cpp/globals.h naming convention.
set_property(
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    DIRECTORY ${PROJECT_SOURCE_DIR}/lib/native
    PROPERTY COMPILE_OPTIONS "-x;objective-c++"
)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.mm
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
    ${CMAKE_CURRENT_SOURCE_DIR}/image_codec.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/message_box.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/button.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/combo_box.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/check.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/list.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/accordion.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/surfaces.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/tab_view.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/split_view.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/icon_view.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_view.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/code_edit.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/directory_dialog.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.mm
)

target_link_libraries(native PRIVATE
    "-framework AppKit"
    "-framework Foundation"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework ImageIO"
    "-framework UniformTypeIdentifiers"
)

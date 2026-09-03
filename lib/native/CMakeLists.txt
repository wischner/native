add_library(native STATIC)

target_compile_features(native PUBLIC cxx_std_20)
target_link_libraries(native PUBLIC native_developer_options)

# CMake does not add an Objective-C++ standard flag for every compiler it
# supports. Enforce the architectural C++20 requirement for those sources.
if(CMAKE_OBJCXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(native PRIVATE
        $<$<COMPILE_LANGUAGE:OBJCXX>:-std=c++20>
    )
endif()

target_include_directories(native
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add backend-neutral native source files.
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/portable_font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/portable_font_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/installed_fonts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/software_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/software_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rotated_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/non_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ruler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/status_bar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tab_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/split_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/owned_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modeless_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modal_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/directory_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_document.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/combo_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_render.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/control_render_access.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/accordion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/icon_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collection_render.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_search.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_visible_rows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_render.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layout.cpp
)

if(NOT WIN32)
    target_sources(native PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/status_bar_peer_default.cpp
    )
endif()

# Configure Unix toolkit libraries before the shared Linux codec libraries.
# This matches native XView client link order on Tribblix; its notifier
# interposes select and depends on that order to reach the libc system call
# from inside the notification loop.
if(NOT WIN32 AND NOT HAIKU AND NOT APPLE)
    add_subdirectory(toolkits)
endif()

add_subdirectory(platforms)

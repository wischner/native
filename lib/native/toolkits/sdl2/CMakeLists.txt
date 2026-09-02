set(LINUX_PLATFORM_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/linux)
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window_position.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/message_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/combo_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collections.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/split_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard_x11.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit_render.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/directory_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.cpp
    ${LINUX_PLATFORM_SOURCE_DIR}/file_dialog_process.cpp
)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_TTF QUIET IMPORTED_TARGET SDL2_ttf)
endif()
find_package(X11 REQUIRED)
target_include_directories(native PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)
if(TARGET PkgConfig::SDL2_TTF)
    target_link_libraries(native PRIVATE
        ${SDL2_LIBRARIES}
        ${X11_LIBRARIES}
        PkgConfig::SDL2_TTF
    )
    target_compile_definitions(native PRIVATE HAVE_SDL2_TTF)
elseif(SDL2_ttf_FOUND)
    target_link_libraries(native PRIVATE
        ${SDL2_LIBRARIES}
        ${X11_LIBRARIES}
        SDL2_ttf
    )
    target_compile_definitions(native PRIVATE HAVE_SDL2_TTF)
else()
    message(STATUS "SDL2_ttf headers not found; using bitmap fallback")
    target_link_libraries(native PRIVATE
        ${SDL2_LIBRARIES}
        ${X11_LIBRARIES}
    )
endif()

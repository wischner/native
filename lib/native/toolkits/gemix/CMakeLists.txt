message(STATUS "Configuring GEMix toolkit backend...")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GEMIX_AES REQUIRED gemix-aes)
pkg_check_modules(GEMIX_VDI REQUIRED gemix-vdi)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/message_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/combo_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collections.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/surfaces.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/split_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/table_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/text_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/directory_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/open_file_dialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/save_file_dialog.cpp
)

target_include_directories(native PRIVATE
    ${GEMIX_AES_INCLUDE_DIRS}
    ${GEMIX_VDI_INCLUDE_DIRS}
)

target_link_libraries(native PRIVATE
    ${GEMIX_AES_LIBRARIES}
    ${GEMIX_VDI_LIBRARIES}
)

message(STATUS "GEMix toolkit configured with native AES/VDI dependencies only.")

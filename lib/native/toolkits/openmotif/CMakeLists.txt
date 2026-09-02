message(STATUS "Configuring OpenMotif Toolkit...")

find_package(PkgConfig REQUIRED)

pkg_check_modules(XT REQUIRED xt)

find_package(X11 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/scripts/cmake")
find_package(Motif REQUIRED)

# Diagnostics
message(STATUS "Xt include dirs: ${XT_INCLUDE_DIRS}")
message(STATUS "Xt lib dirs: ${XT_LIBRARY_DIRS}")
message(STATUS "Xt libs: ${XT_LIBRARIES}")

if(NOT TARGET native)
    message(FATAL_ERROR "Target 'native' does not exist!")
endif()

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme_support.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/message_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/combo_box.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collections.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/icon_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tab_view.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platforms/linux/x_image.cpp
)

# Conditionally include directories if needed
if(XT_INCLUDE_DIRS)
    target_include_directories(native PRIVATE
        ${XT_INCLUDE_DIRS}
    )
else()
    message(STATUS "No additional Xt include dirs needed (default include path works).")
endif()

# Link libraries
# Xt's pkg-config contract already includes X11. Listing X11 again is
# harmless with GNU ld but produces duplicate-library warnings on Tribblix.
target_link_libraries(native PRIVATE
    ${MOTIF_LIBRARIES}
    ${XT_LIBRARIES}
)

message(STATUS "OpenMotif Toolkit successfully configured!")

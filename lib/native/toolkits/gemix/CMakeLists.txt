message(STATUS "Configuring GEMix toolkit backend...")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GEMIX_AES REQUIRED gemix-aes)
pkg_check_modules(GEMIX_VDI REQUIRED gemix-vdi)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/control_paint_backend.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
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

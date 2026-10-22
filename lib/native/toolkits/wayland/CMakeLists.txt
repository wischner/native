find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET GLOBAL gtk4>=4.14)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(SOURCEVIEW REQUIRED IMPORTED_TARGET GLOBAL
    gtksourceview-5>=5.12)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
target_link_libraries(native PRIVATE
    PkgConfig::GTK4 PkgConfig::FONTCONFIG
    PkgConfig::SOURCEVIEW PkgConfig::XKBCOMMON)
target_compile_definitions(native PRIVATE
    HAVE_FONTCONFIG NATIVE_GTK_WAYLAND
    GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_14
    GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_14)
foreach(source globals wnd app app_wnd screen controls text_edit surfaces pickers
    tree_view table_view code_edit icon_view index_model input keyboard
    drawing gpx_wnd gpx_img img font theme theme_snapshot theme_widgets theme_metrics menu dialogs clipboard)
    target_sources(native PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/${source}.cpp)
endforeach()
if(NOT TOOLKIT STREQUAL "ELEMENTARY")
    target_sources(native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/desktop.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/picker_widgets.cpp)
endif()

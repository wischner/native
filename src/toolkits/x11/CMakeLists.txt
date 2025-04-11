# --- Find packages: pixman, x11. ------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
find_package(X11 REQUIRED)


# --- Define target. -------------------------------------------------
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
)


# --- Add libs. ------------------------------------------------------
target_include_directories(native PRIVATE
    ${X11_INCLUDE_DIR}
    ${PIXMAN_INCLUDE_DIRS}
)
target_link_libraries(native PRIVATE
    ${X11_LIBRARIES}
    ${PIXMAN_LIBRARIES}
)

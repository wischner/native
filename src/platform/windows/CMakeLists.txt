# src/platform/windows/CMakeLists.txt

target_sources(native PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/screen.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wnd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/app.cpp
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)

# Windows-specific libraries (user32 for windowing, gdi32 for graphics)
target_link_libraries(native PRIVATE
    user32
    gdi32
    shell32
)

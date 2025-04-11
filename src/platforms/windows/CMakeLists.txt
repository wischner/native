# src/platform/windows/CMakeLists.txt

target_sources(native PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/screen.cpp
)

# Windows-specific libraries (user32 for windowing, gdi32 for graphics)
target_link_libraries(native PRIVATE
    user32
    gdi32
    shell32
)

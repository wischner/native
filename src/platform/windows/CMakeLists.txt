# src/platform/win32/CMakeLists.txt

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/entry.cpp
)

# Optional: Link additional Windows libraries if you need them
target_link_libraries(native PRIVATE
    user32
    gdi32
)
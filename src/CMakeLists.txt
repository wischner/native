add_library(native STATIC)

target_include_directories(native PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

# Add generic native source files
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
)

add_subdirectory(platform)
add_subdirectory(toolkits)
add_library(native STATIC)

target_include_directories(native PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
)

add_subdirectory(platform)
add_subdirectory(toolkits)
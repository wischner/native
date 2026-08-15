target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_codec.cpp
)

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

target_link_libraries(native PRIVATE PNG::PNG JPEG::JPEG)

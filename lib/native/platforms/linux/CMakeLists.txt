target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/glsl_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_codec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem.cpp
)

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

target_link_libraries(native PRIVATE PNG::PNG JPEG::JPEG)

if(NOT TOOLKIT STREQUAL "SDL2" AND NOT TOOLKIT STREQUAL "GEMIX")
    target_sources(native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../toolkits/x_keyboard.cpp)
endif()

target_link_libraries(native PRIVATE ${CMAKE_DL_LIBS})

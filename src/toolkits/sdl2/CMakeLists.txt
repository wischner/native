target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
)

find_package(SDL2 REQUIRED)
target_include_directories(native PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(native PRIVATE ${SDL2_LIBRARIES})

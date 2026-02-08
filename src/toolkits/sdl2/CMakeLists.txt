target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.cpp
)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
target_include_directories(native PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(native PRIVATE ${SDL2_LIBRARIES} SDL2_ttf)

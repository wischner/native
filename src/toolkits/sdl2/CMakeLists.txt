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
find_package(SDL2_ttf QUIET)
target_include_directories(native PRIVATE ${SDL2_INCLUDE_DIRS})
if(SDL2_ttf_FOUND)
    target_link_libraries(native PRIVATE ${SDL2_LIBRARIES} SDL2_ttf)
    target_compile_definitions(native PRIVATE HAVE_SDL2_TTF)
else()
    message(STATUS "SDL2_ttf not found — draw_text will be a no-op")
    target_link_libraries(native PRIVATE ${SDL2_LIBRARIES})
endif()

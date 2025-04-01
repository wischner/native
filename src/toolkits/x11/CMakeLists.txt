target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
)

find_package(X11 REQUIRED)
target_include_directories(native PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(native PRIVATE ${X11_LIBRARIES})

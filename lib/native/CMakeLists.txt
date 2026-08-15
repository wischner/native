add_library(native STATIC)

target_compile_features(native PUBLIC cxx_std_20)
target_link_libraries(native PUBLIC native_developer_options)

# CMake does not add an Objective-C++ standard flag for every compiler it
# supports. Enforce the architectural C++20 requirement for those sources.
if(CMAKE_OBJCXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(native PRIVATE
        $<$<COMPILE_LANGUAGE:OBJCXX>:-std=c++20>
    )
endif()

target_include_directories(native
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add backend-neutral native source files.
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layout.cpp
)

add_subdirectory(platforms)

if(NOT WIN32 AND NOT HAIKU AND NOT APPLE)
    add_subdirectory(toolkits)
endif()

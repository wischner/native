# examples/CMakeLists.txt

add_executable(app-example main.cpp)
target_link_libraries(app-example PRIVATE native)

# Use WinMain-style entry point when building with MSVC
if (MSVC)
    set_target_properties(app-example PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
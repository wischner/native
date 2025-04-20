# examples/CMakeLists.txt

add_executable(painter-example main.cpp)
target_link_libraries(painter-example PRIVATE native)

# Use WinMain-style entry point when building with MSVC
if (MSVC)
    set_target_properties(painter-example PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
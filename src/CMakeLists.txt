# Embed the independent portable image-shader demo; no runtime asset path.
file(READ ${PROJECT_SOURCE_DIR}/tests/data/shaders/crt.nshader native_crt_package)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/data/shaders/crt.nshader)
configure_file(shader_demo_data.h.in shader_demo_data.h @ONLY)

if(APPLE)
    # A macOS GUI program must be a bundle so LaunchServices treats it as a
    # foreground application when LLDB starts it from a remote shell.
    add_executable(vision MACOSX_BUNDLE
        main.cpp
        infinity_window.cpp
        vision_mouse_shaders.cpp
        vision_retro_terminal.cpp
        terminal_screen.cpp
        vision_services.cpp
        vision_window.cpp
        vision_windows.cpp
        vision_properties.cpp
    )
    set_target_properties(vision PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Vision"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.wischner.native.vision"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
else()
    add_executable(vision
        main.cpp
        infinity_window.cpp
        vision_mouse_shaders.cpp
        vision_retro_terminal.cpp
        terminal_screen.cpp
        vision_services.cpp
        vision_window.cpp
        vision_windows.cpp
        vision_properties.cpp
    )
endif()

target_include_directories(vision PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(vision PRIVATE native)

# Vision is a GUI application on Windows. CMake selects the appropriate
# linker option for both MinGW (-mwindows) and Microsoft toolchains.
if(WIN32)
    set_target_properties(vision PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Asset edits trigger publication even when no application C++ changed.
file(GLOB_RECURSE native_terminal_assets CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/retro-terminal/*)
set_property(TARGET vision APPEND PROPERTY LINK_DEPENDS ${native_terminal_assets})

# External attributed shader/font assets travel beside the test application.
add_custom_command(TARGET vision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/retro-terminal
        $<TARGET_FILE_DIR:vision>/retro-terminal)

if(APPLE)
    # A macOS GUI program must be a bundle so LaunchServices treats it as a
    # foreground application when LLDB starts it from a remote shell.
    add_executable(vision MACOSX_BUNDLE
        main.cpp
        vision_services.cpp
        vision_window.cpp
        vision_windows.cpp
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
        vision_services.cpp
        vision_window.cpp
        vision_windows.cpp
    )
endif()

target_link_libraries(vision PRIVATE native)

# Vision is a GUI application on Windows. CMake selects the appropriate
# linker option for both MinGW (-mwindows) and Microsoft toolchains.
if(WIN32)
    set_target_properties(vision PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Collect all the source files for the program
set(SRC_FILES
    main.cpp
    globals.cpp
    app.cpp
    screen.cpp
    wnd.cpp
    app_wnd.cpp
    NativeWindow.cpp
)
# Create the executable for the main application
target_sources(native PRIVATE ${SRC_FILES})

# Haiku system includes and libraries
find_path(HAIKU_INCLUDE_DIR
    NAMES app/Application.h
    PATHS /boot/system/develop/headers/be
)

find_library(HAIKU_LIB_BE
    NAMES be
    PATHS /boot/system/develop/lib
)

find_library(HAIKU_LIB_TRACKER
    NAMES tracker
    PATHS /boot/system/develop/lib
)

# Include the Haiku system headers
target_include_directories(native
    PRIVATE ${HAIKU_INCLUDE_DIR}
)

# Link against the BeAPI (Haiku's UI API) and Tracker library (needed for BFilePanel)
target_link_libraries(native
    PRIVATE
        ${HAIKU_LIB_BE}
        ${HAIKU_LIB_TRACKER}
)
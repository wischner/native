# Collect all the source files for the program
set(SRC_FILES
    control_paint_backend.cpp
    main.cpp
    globals.cpp
    app.cpp
    screen.cpp
    wnd.cpp
    app_wnd.cpp
    NativeWindow.cpp
    font.cpp
    gpx_wnd.cpp
    gpx_img.cpp
    img.cpp
    menu.cpp
    button.cpp
)
# Create the executable for the main application
target_sources(native PRIVATE ${SRC_FILES})

find_library(HAIKU_LIB_BE
    NAMES be
    REQUIRED
)

find_library(HAIKU_LIB_TRACKER
    NAMES tracker
    REQUIRED
)

# Link against the BeAPI and Tracker libraries.
target_link_libraries(native
    PRIVATE
        ${HAIKU_LIB_BE}
        ${HAIKU_LIB_TRACKER}
)

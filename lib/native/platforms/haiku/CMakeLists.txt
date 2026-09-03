# Collect all the source files for the program
set(SRC_FILES
    theme.cpp
    main.cpp
    globals.cpp
    app.cpp
    screen.cpp
    wnd.cpp
    app_wnd.cpp
    native_window.cpp
    font.cpp
    gpx_wnd.cpp
    gpx_img.cpp
    img.cpp
    image_codec.cpp
    menu.cpp
    message_box.cpp
    button.cpp
    combo_box.cpp
    check.cpp
    radio.cpp
    list.cpp
    tab_view.cpp
    split_view.cpp
    collection_view.cpp
    panel.cpp
    canvas.cpp
    accordion.cpp
    icon_view.cpp
    tree_view.cpp
    table_view.cpp
    clipboard.cpp
    text_edit.cpp
    code_edit.cpp
    file_dialog.cpp
    directory_dialog.cpp
    open_file_dialog.cpp
    save_file_dialog.cpp
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

find_library(HAIKU_LIB_TRANSLATION
    NAMES translation
    REQUIRED
)

find_path(HAIKU_COLUMN_LIST_INCLUDE
    NAMES ColumnListView.h
    PATH_SUFFIXES private/interface
    REQUIRED
)

find_library(HAIKU_LIB_COLUMN_LIST
    NAMES columnlistview
    REQUIRED
)

target_include_directories(native PRIVATE
    ${HAIKU_COLUMN_LIST_INCLUDE}
)

# Link against the BeAPI and Tracker libraries.
target_link_libraries(native
    PRIVATE
        ${HAIKU_LIB_BE}
        ${HAIKU_LIB_TRACKER}
        ${HAIKU_LIB_TRANSLATION}
        ${HAIKU_LIB_COLUMN_LIST}
)

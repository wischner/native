# src/platform/CMakeLists.txt

if(WIN32)
    add_subdirectory(windows)
elseif(HAIKU)
    add_subdirectory(haiku)
elseif(UNIX AND NOT APPLE)
    add_subdirectory(linux)
endif()

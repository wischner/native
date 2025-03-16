# src/platform/CMakeLists.txt

if(WIN32)
    add_subdirectory(windows)
elseif(UNIX AND NOT APPLE)
    #add_subdirectory(linux)
elseif(HAIKU)
    #add_subdirectory(haiku)
endif()
set(TOOLKIT "X11" CACHE STRING "Linux toolkit backend")
set_property(CACHE TOOLKIT PROPERTY STRINGS
    X11 SDL2 MOTIF OPENLOOK WMAKER GEMIX ELEMENTARY)

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/emulated_theme.cpp
)

if(TOOLKIT STREQUAL "SDL2")
    add_subdirectory(sdl2)
elseif(TOOLKIT STREQUAL "GEMIX")
    add_subdirectory(gemix)
elseif(TOOLKIT STREQUAL "MOTIF")
    add_subdirectory(openmotif)
elseif(TOOLKIT STREQUAL "OPENLOOK")
    add_subdirectory(openlook)
elseif(TOOLKIT STREQUAL "WMAKER")
    add_subdirectory(wmaker)
elseif(TOOLKIT STREQUAL "X11")
    add_subdirectory(x11)
elseif(TOOLKIT STREQUAL "ELEMENTARY")
    add_subdirectory(elementary)
else()
    message(FATAL_ERROR "Unknown toolkit: ${TOOLKIT}")
endif()

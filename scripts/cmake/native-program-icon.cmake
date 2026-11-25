# Publishes a desktop program icon beside an executable built with Native.
# The GEMix desktop draws icons/<executable stem>.rsc, resolved through the
# GEM resource search path; upstream GEM installs its own program icons in
# <datadir>/gem/icons. Other backends take program icons from their own
# packaging, so the function adds no build step for them.
#
# MIT License (see: LICENSE)
# Copyright (C) 2026 Tomaz Stih

include_guard(GLOBAL)
include(GNUInstallDirs)

# native_add_program_icon(<target> <png>)
#
# On GEMix, convert <png> with the SDK's resgen into a monochrome GEM icon
# resource at <build>/share/gem/icons/<name>.rsc and install it under
# <datadir>/gem/icons. <name> is the target's OUTPUT_NAME, or the target
# name. Supply 32x32 art: resgen treats alpha below 32 as transparent and
# any channel below 224 as black.
function(native_add_program_icon target png)
    if(NOT TOOLKIT STREQUAL "GEMIX")
        return()
    endif()
    find_program(NATIVE_GEM_RESGEN resgen
        HINTS /opt/gemix/bin /opt/gemix/sdk/bin REQUIRED)
    get_target_property(name ${target} OUTPUT_NAME)
    if(NOT name)
        set(name ${target})
    endif()
    get_filename_component(source "${png}" ABSOLUTE)
    set(directory "${CMAKE_BINARY_DIR}/share/gem/icons")
    set(output "${directory}/${name}.rsc")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${directory}"
        COMMAND ${NATIVE_GEM_RESGEN} icons -o "${output}" "${source}"
        DEPENDS "${source}"
        COMMENT "Generating GEM program icon ${name}.rsc"
        VERBATIM)
    add_custom_target(${target}_program_icon ALL DEPENDS "${output}")
    add_dependencies(${target} ${target}_program_icon)
    install(FILES "${output}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gem/icons")
endfunction()

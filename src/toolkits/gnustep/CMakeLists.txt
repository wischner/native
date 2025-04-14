# --- Remove puto warnings -------------------------------------------
target_compile_options(native PRIVATE
    -Wno-subobject-linkage
)

# --- Fetch flags from gnustep-config --------------------------------
execute_process(
    COMMAND bash -c ". /usr/share/GNUstep/Makefiles/GNUstep.sh && gnustep-config --objc-flags"
    OUTPUT_VARIABLE OBJC_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND bash -c ". /usr/share/GNUstep/Makefiles/GNUstep.sh && gnustep-config --gui-libs"
    OUTPUT_VARIABLE OBJC_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(OBJC_CFLAGS)
separate_arguments(OBJC_LIBS)

# --- Add ObjC++ sources ---------------------------------------------
set(GNUSTEP_OBJCXX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.mm
)

target_sources(native PRIVATE ${GNUSTEP_OBJCXX_SOURCES})

# --- Apply ObjC++ flags only to OBJCXX sources -----------------------
target_compile_options(native PRIVATE
    $<$<COMPILE_LANGUAGE:OBJCXX>:${OBJC_CFLAGS}>
)

# Optional: include paths manually (usually already in OBJC_CFLAGS)
target_include_directories(native PRIVATE
    /usr/include/GNUstep
    /usr/local/include/GNUstep
    /home/tomaz/GNUstep/Library/Headers
)

# Apply linking flags to target
target_link_libraries(native PRIVATE ${OBJC_LIBS})

# Tell CMake these are Objective-C++ files
set_source_files_properties(${GNUSTEP_OBJCXX_SOURCES} PROPERTIES LANGUAGE OBJCXX)

# Optional macro for code guards
target_compile_definitions(native PRIVATE GNUSTEP)

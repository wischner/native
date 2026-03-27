message(STATUS "Configuring GNUstep Toolkit...")

# GNUstep ObjC runtime tends to emit this warning for mixed C++/ObjC++ code.
target_compile_options(native PRIVATE -Wno-subobject-linkage)

# Fetch flags from gnustep-config inside the GNUstep environment.
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

separate_arguments(OBJC_CFLAGS UNIX_COMMAND "${OBJC_CFLAGS}")
separate_arguments(OBJC_LIBS UNIX_COMMAND "${OBJC_LIBS}")

set(GNUSTEP_OBJCXX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/screen.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/app_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/globals.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/font.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_wnd.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/gpx_img.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/menu.mm
)

set(GNUSTEP_CPP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/img.cpp
)

target_sources(native PRIVATE ${GNUSTEP_OBJCXX_SOURCES} ${GNUSTEP_CPP_SOURCES})

target_compile_options(native PRIVATE
    $<$<COMPILE_LANGUAGE:OBJCXX>:${OBJC_CFLAGS}>
)

target_include_directories(native PRIVATE
    /usr/include/GNUstep
    /usr/local/include/GNUstep
)

target_link_libraries(native PRIVATE ${OBJC_LIBS})

set_source_files_properties(${GNUSTEP_OBJCXX_SOURCES} PROPERTIES LANGUAGE OBJCXX)
target_compile_definitions(native PRIVATE GNUSTEP)

message(STATUS "GNUstep Toolkit successfully configured!")

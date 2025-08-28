# cmake/native_examples.cmake
# Usage:
#   native_example(<target> <dir> [GUI] [SOURCES file1.cpp;file2.cpp])
#
# - <dir> is the example folder (we'll glob *.c,*.cpp,*.mm,*.m,*.rc if SOURCES not given)
# - GUI sets WIN32 subsystem automatically on Windows/MinGW (no effect elsewhere)

function(native_example target dir)
  set(options GUI)
  set(oneValueArgs)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(NEX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Collect sources if not provided
  if(NOT NEX_SOURCES)
    file(GLOB EX_SOURCES CONFIGURE_DEPENDS
      "${dir}/*.c" "${dir}/*.cpp" "${dir}/*.mm" "${dir}/*.m" "${dir}/*.rc")
  else()
    set(EX_SOURCES ${NEX_SOURCES})
  endif()

  # WIN32 subsystem only matters on Windows; harmless elsewhere but we’ll gate it anyway
  set(exe_kind)
  if(NEX_GUI AND platform STREQUAL "windows")
    set(exe_kind WIN32)   # maps to -mwindows on MinGW, /SUBSYSTEM:WINDOWS on MSVC
  endif()

  add_executable(${target} ${exe_kind} ${EX_SOURCES})
  target_link_libraries(${target} PRIVATE native)
  target_include_directories(${target} PRIVATE "${CMAKE_SOURCE_DIR}/include")

  # If your native lib doesn’t already export Win32 libs PUBLIC,
  # uncomment the next line to be extra-safe for MinGW/MSVC:
  # if(platform STREQUAL "windows")
  #   target_link_libraries(${target} PRIVATE user32 gdi32 shell32)
  # endif()
endfunction()

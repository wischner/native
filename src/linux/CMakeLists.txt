# src/linux/CMakeLists.txt

# 1) Add all non-recursive platform sources (INCLUDING main.cpp)
file(GLOB LINUX_SRCS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
if(LINUX_SRCS)
  target_sources(native PRIVATE ${LINUX_SRCS})
endif()

# 2) Dispatch to the selected toolkit (x11, motif, sdl2, ...)
if(NOT DEFINED toolkit OR toolkit STREQUAL "")
  message(FATAL_ERROR "linux: 'toolkit' not set (derived from variant)")
endif()

set(_toolkit_dir "${CMAKE_CURRENT_SOURCE_DIR}/${toolkit}")
if(EXISTS "${_toolkit_dir}/CMakeLists.txt")
  add_subdirectory("${_toolkit_dir}")
else()
  message(FATAL_ERROR "linux: unsupported toolkit='${toolkit}' (missing ${_toolkit_dir}/CMakeLists.txt)")
endif()

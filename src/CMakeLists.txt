# single library target
add_library(native STATIC)

# public + private includes
target_include_directories(native
  PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# common (non-recursive) sources directly under src/
file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
if(COMMON_SOURCES)
  target_sources(native PRIVATE ${COMMON_SOURCES})
endif()

# sanity
if(NOT DEFINED platform OR platform STREQUAL "")
  message(FATAL_ERROR "internal: 'platform' not set (root must derive from variant)")
endif()
if(NOT DEFINED toolkit OR toolkit STREQUAL "")
  message(FATAL_ERROR "internal: 'toolkit' not set (root must derive from variant)")
endif()

# enter platform subdir (e.g. src/linux, src/windows, src/macos, src/haiku, src/gnustep)
set(_plat_dir "${CMAKE_CURRENT_SOURCE_DIR}/${platform}")
if(EXISTS "${_plat_dir}/CMakeLists.txt")
  add_subdirectory("${_plat_dir}")
else()
  message(FATAL_ERROR "No platform directory for '${platform}' at: ${_plat_dir}")
endif()

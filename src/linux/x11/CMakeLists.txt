# src/linux/x11/CMakeLists.txt

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
pkg_check_modules(XRANDR REQUIRED xrandr)
find_package(X11 REQUIRED)

# Build defs visible only to this library
target_compile_definitions(native PRIVATE TOOLKIT_X11=1)

# Export include paths & link libs PUBLIC so executables inherit them
target_include_directories(native PUBLIC
  ${X11_INCLUDE_DIR}
  ${PIXMAN_INCLUDE_DIRS}
  ${XRANDR_INCLUDE_DIRS}
)

target_link_libraries(native PUBLIC
  ${X11_LIBRARIES}
  ${PIXMAN_LIBRARIES}
  ${XRANDR_LIBRARIES}
)

# Extra CFLAGS from pkg-config shouldn't leak to dependents
target_compile_options(native PRIVATE
  ${PIXMAN_CFLAGS_OTHER}
  ${XRANDR_CFLAGS_OTHER}
)

# Add this toolkit's sources (non-recursive)
file(GLOB X11_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
if(NOT X11_SOURCES)
  message(FATAL_ERROR "x11: no sources found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()
target_sources(native PRIVATE ${X11_SOURCES})

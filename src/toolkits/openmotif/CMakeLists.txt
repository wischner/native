message(STATUS "Configuring OpenMotif Toolkit...")

find_package(PkgConfig REQUIRED)

pkg_check_modules(XT REQUIRED xt)

find_package(X11 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
find_package(Motif REQUIRED)

# Diagnostics
message(STATUS "Xt include dirs: ${XT_INCLUDE_DIRS}")
message(STATUS "Xt lib dirs: ${XT_LIBRARY_DIRS}")
message(STATUS "Xt libs: ${XT_LIBRARIES}")

if(NOT TARGET native)
    message(FATAL_ERROR "Target 'native' does not exist!")
endif()

target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Conditionally include directories if needed
if(XT_INCLUDE_DIRS)
    target_include_directories(native PRIVATE
        ${XT_INCLUDE_DIRS}
    )
else()
    message(STATUS "No additional Xt include dirs needed (default include path works).")
endif()

# Link libraries
target_link_libraries(native PRIVATE
    ${X11_LIBRARIES}
    ${MOTIF_LIBRARIES}
    ${XT_LIBRARIES}
)

message(STATUS "OpenMotif Toolkit successfully configured!")

cmake_minimum_required(VERSION 3.16)

# Build dockerized current variant by default on plain `make`
option(docker_build_by_default "Build current docker variant on default 'all' target" ON)

# Expect 'variant' from root (e.g., linux-x11, windows-win32, haiku-beapi)
if(NOT DEFINED variant OR variant STREQUAL "")
  message(FATAL_ERROR "docker/: 'variant' is not set. Pass -Dvariant=<platform>-<toolkit> at configure time.")
endif()
string(TOLOWER "${variant}" variant)
string(REPLACE "-" "_" variant_us "${variant}")

# Tags / dirs
set(DOCKER_TAG_BASE    "native-build:base"   CACHE STRING "Base image tag")
set(DOCKER_TAG_PREFIX  "native-build"        CACHE STRING "Prefix for variant image tags")

# Host uid/gid (avoid shell expansion in recipes)
execute_process(COMMAND id -u OUTPUT_VARIABLE HOST_UID OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND id -g OUTPUT_VARIABLE HOST_GID OUTPUT_STRIP_TRAILING_WHITESPACE)

# Helper to build an image
function(docker_image target dockerfile tag)
  add_custom_target(${target}
    COMMAND docker build
            -f "${dockerfile}"
            -t "${tag}"
            "${CMAKE_SOURCE_DIR}"
    VERBATIM
    USES_TERMINAL
    COMMENT "Building Docker image: ${tag}"
  )
endfunction()

# Base image
set(_base_df "${CMAKE_SOURCE_DIR}/docker/base.Dockerfile")
if(NOT EXISTS "${_base_df}")
  message(FATAL_ERROR "Missing base Dockerfile: ${_base_df}")
endif()
docker_image(docker_image_base "${_base_df}" "${DOCKER_TAG_BASE}")

# Variant image (expects docker/<variant>.Dockerfile)
set(_variant_df "${CMAKE_SOURCE_DIR}/docker/${variant}.Dockerfile")
if(NOT EXISTS "${_variant_df}")
  message(FATAL_ERROR "Missing variant Dockerfile: ${_variant_df}")
endif()

set(DOCKER_TAG_VARIANT "${DOCKER_TAG_PREFIX}:${variant}")
set(BDIR_VARIANT       "build-${variant}")

set(img_target "docker_image_${variant_us}")
docker_image(${img_target} "${_variant_df}" "${DOCKER_TAG_VARIANT}")
add_dependencies(${img_target} docker_image_base)

# ---------------------- per-variant extras ----------------------
# Only pass toolchains when we actually have source (avoid "unused var" warnings)
set(_src_cmake "${CMAKE_SOURCE_DIR}/src/CMakeLists.txt")
set(EXTRA_CONFIG_ARGS "")

if(EXISTS "${_src_cmake}")
  if(variant MATCHES "^windows-")
    set(_tc "${CMAKE_SOURCE_DIR}/cmake/toolchains/mingw-w64.cmake")
    if(NOT EXISTS "${_tc}")
      message(FATAL_ERROR "Missing Windows toolchain file: ${_tc}")
    endif()
    set(EXTRA_CONFIG_ARGS "-DCMAKE_TOOLCHAIN_FILE=/work/cmake/toolchains/mingw-w64.cmake")
  elseif(variant MATCHES "^haiku-")
    set(_tc "${CMAKE_SOURCE_DIR}/cmake/toolchains/haiku.cmake")
    if(NOT EXISTS "${_tc}")
      message(FATAL_ERROR "Missing Haiku toolchain file: ${_tc}")
    endif()
    set(EXTRA_CONFIG_ARGS "-DCMAKE_TOOLCHAIN_FILE=/work/cmake/toolchains/haiku.cmake")
  endif()
endif()

# Inner build command (disable docker targets inside to avoid recursion)
set(DOCKER_BUILD_CMD
  "cmake -S . -B ${BDIR_VARIANT} -G \"Unix Makefiles\" -Dvariant=${variant} -Denable_docker=OFF -DCMAKE_BUILD_TYPE=Debug ${EXTRA_CONFIG_ARGS} && cmake --build ${BDIR_VARIANT} -j"
)

# Build target: docker_build_<variant>
if(docker_build_by_default)
  add_custom_target("docker_build_${variant_us}" ALL
    COMMAND docker run --rm
            -v "${CMAKE_SOURCE_DIR}:/work"
            -w /work
            -u "${HOST_UID}:${HOST_GID}"
            -e HOME=/work
            "${DOCKER_TAG_VARIANT}"
            bash -lc "${DOCKER_BUILD_CMD}"
    VERBATIM
    USES_TERMINAL
    DEPENDS ${img_target}
    COMMENT "Configure & build variant ${variant} inside Docker"
  )
else()
  add_custom_target("docker_build_${variant_us}"
    COMMAND docker run --rm
            -v "${CMAKE_SOURCE_DIR}:/work"
            -w /work
            -u "${HOST_UID}:${HOST_GID}"
            -e HOME=/work
            "${DOCKER_TAG_VARIANT}"
            bash -lc "${DOCKER_BUILD_CMD}"
    VERBATIM
    USES_TERMINAL
    DEPENDS ${img_target}
    COMMENT "Configure & build variant ${variant} inside Docker"
  )
endif()

# Convenience alias
add_custom_target(docker_build_current
  DEPENDS "docker_build_${variant_us}"
)

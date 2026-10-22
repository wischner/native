# Amiga runtime qualification is a standalone project in amiga_toolchain/,
# invoked by docker-amiga-check before a Native backend can be configured.
# It must not be registered as an existing backend's runtime test.
add_executable(native_core_tests
    native_core_tests.cpp
    ${PROJECT_SOURCE_DIR}/lib/native/connection.cpp
    ${PROJECT_SOURCE_DIR}/lib/native/file_filter_match.cpp
    ${PROJECT_SOURCE_DIR}/lib/native/geometry.cpp
)

target_include_directories(native_core_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/lib/native
)

target_link_libraries(native_core_tests PRIVATE
    native_developer_options
)

add_test(NAME native_core_tests COMMAND native_core_tests)

if(APPLE)
    add_executable(native_mac_runtime_tests native_mac_runtime_tests.mm
        native_mac_presentation_checks.mm
        native_mac_navigation_checks.mm
        ${PROJECT_SOURCE_DIR}/src/vision_properties.cpp
        ${PROJECT_SOURCE_DIR}/src/vision_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/navigation_icons.cpp)
    target_include_directories(native_mac_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_mac_runtime_tests PRIVATE
        native native_developer_options "-framework AppKit")
    add_test(NAME native_mac_runtime_tests COMMAND native_mac_runtime_tests)
    set_tests_properties(native_mac_runtime_tests PROPERTIES TIMEOUT 30)
endif()

if(UNIX AND TOOLKIT STREQUAL "OPENLOOK")
    add_executable(native_openlook_runtime_tests native_openlook_runtime_tests.cpp)
    target_include_directories(native_openlook_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_openlook_runtime_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_openlook_runtime_tests COMMAND native_openlook_runtime_tests)
    set_tests_properties(native_openlook_runtime_tests PROPERTIES TIMEOUT 30)
endif()

if(UNIX AND TOOLKIT STREQUAL "MOTIF")
    add_executable(native_motif_runtime_tests native_motif_runtime_tests.cpp)
    target_include_directories(native_motif_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_motif_runtime_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_motif_runtime_tests COMMAND native_motif_runtime_tests)
    set_tests_properties(native_motif_runtime_tests PROPERTIES TIMEOUT 30)
endif()

if(WIN32)
    add_executable(native_windows_runtime_tests native_windows_runtime_tests.cpp)
    target_include_directories(native_windows_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_compile_definitions(native_windows_runtime_tests PRIVATE
        UNICODE _UNICODE _WIN32_WINNT=0x0601)
    target_link_libraries(native_windows_runtime_tests PRIVATE
        native native_developer_options uxtheme)
    add_test(NAME native_windows_runtime_tests COMMAND native_windows_runtime_tests)
    set_tests_properties(native_windows_runtime_tests PROPERTIES TIMEOUT 30)
endif()

add_executable(native_window_api_tests
    native_window_api_tests.cpp
)

target_link_libraries(native_window_api_tests PRIVATE
    native
    native_developer_options
)

if(UNIX AND TOOLKIT STREQUAL "GEMIX" AND GEMIX_USE_GEMD)
    set(gemd_runner bash ${PROJECT_SOURCE_DIR}/scripts/gemix/gemd-session.sh)
endif()
add_test(NAME native_window_api_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_window_api_tests>)

add_executable(native_inspector_tests native_inspector_tests.cpp)
target_link_libraries(native_inspector_tests PRIVATE native native_developer_options)
add_test(NAME native_inspector_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_inspector_tests>)

add_executable(native_settings_tests native_settings_tests.cpp)
target_link_libraries(native_settings_tests PRIVATE native native_developer_options)
target_include_directories(native_settings_tests PRIVATE
    $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
if(APPLE)
    set_source_files_properties(native_settings_tests.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(native_settings_tests PRIVATE "-framework AppKit")
elseif(HAIKU)
    target_include_directories(native_settings_tests SYSTEM PRIVATE ${HAIKU_SHARED_INCLUDE})
elseif(TOOLKIT STREQUAL "WAYLAND" OR TOOLKIT STREQUAL "ELEMENTARY")
    target_link_libraries(native_settings_tests PRIVATE PkgConfig::GTK4)
    target_compile_definitions(native_settings_tests PRIVATE NATIVE_SETTINGS_GTK)
endif()
add_test(NAME native_settings_model_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_settings_tests>)
add_test(NAME native_settings_runtime_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_settings_tests> --runtime)
set_tests_properties(native_settings_model_tests native_settings_runtime_tests PROPERTIES
    TIMEOUT 30 ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;ASAN_OPTIONS=detect_leaks=0;GEM_RASTA_INVERSE=off;GEM_RASTA_FRAMEBUFFER=/tmp/native-settings.fb;GEM_RASTA_PORT=5018;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900")

if(NOT APPLE AND NOT TOOLKIT STREQUAL "GEMIX"
   AND NOT TOOLKIT STREQUAL "WAYLAND" AND NOT TOOLKIT STREQUAL "ELEMENTARY")
    add_executable(native_inspector_runtime_tests
        native_inspector_runtime_tests.cpp inspector_input.cpp)
    target_include_directories(native_inspector_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_compile_definitions(native_inspector_runtime_tests PRIVATE INSPECTOR_${TOOLKIT})
    target_link_libraries(native_inspector_runtime_tests PRIVATE
        native native_developer_options ${CMAKE_DL_LIBS})
    add_test(NAME native_inspector_runtime_tests COMMAND native_inspector_runtime_tests)
    set_tests_properties(native_inspector_runtime_tests PROPERTIES TIMEOUT 30
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()

add_executable(native_table_model_tests
    native_table_model_tests.cpp
)

target_link_libraries(native_table_model_tests PRIVATE
    native
    native_developer_options
)

add_test(NAME native_table_model_tests COMMAND native_table_model_tests)

add_executable(native_code_document_tests
    native_code_document_tests.cpp
)

target_include_directories(native_code_document_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/native
)

target_link_libraries(native_code_document_tests PRIVATE
    native
    native_developer_options
)

add_test(NAME native_code_document_tests
    COMMAND native_code_document_tests)

add_executable(native_collection_runtime_tests
    native_collection_runtime_tests.cpp
)

if(UNIX AND TOOLKIT STREQUAL "GEMIX")
    if(GEMIX_USE_GEMD)
        find_program(GEMIX_XVFB_RUN xvfb-run REQUIRED)
        find_program(GEMIX_PYTHON python3 REQUIRED)
        add_test(NAME native_gemix_session_tests
            COMMAND ${GEMIX_XVFB_RUN} -a ${GEMIX_PYTHON}
                ${CMAKE_CURRENT_SOURCE_DIR}/native_gemix_session_tests.py
                ${PROJECT_SOURCE_DIR}/scripts/gemix/rasta-session.sh
                $<TARGET_FILE:vision>
                ${CMAKE_CURRENT_BINARY_DIR}/rasta-desktop.png)
        set_tests_properties(native_gemix_session_tests PROPERTIES TIMEOUT 40)
    endif()
    add_executable(native_gemix_input_tests native_gemix_input_tests.cpp)
    target_include_directories(native_gemix_input_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_gemix_input_tests PRIVATE native native_developer_options)
    add_test(NAME native_gemix_input_tests
        COMMAND ${gemd_runner} $<TARGET_FILE:native_gemix_input_tests>)
    set_tests_properties(native_gemix_input_tests PROPERTIES TIMEOUT 30 ENVIRONMENT
        "GEM_RASTA_FRAMEBUFFER=/tmp/native-gemix-input-tests.fb;GEM_RASTA_PORT=5012;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900;GEM_RASTA_INVERSE=off;ASAN_OPTIONS=detect_leaks=0")
    add_executable(native_gemix_runtime_tests native_gemix_runtime_tests.cpp)
    target_include_directories(native_gemix_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_gemix_runtime_tests PRIVATE native native_developer_options)
    if(GEMIX_USE_GEMD)
        add_test(NAME native_gemix_runtime_tests
            COMMAND ${gemd_runner} $<TARGET_FILE:native_gemix_runtime_tests>)
    else()
        add_test(NAME native_gemix_runtime_tests COMMAND native_gemix_runtime_tests)
    endif()
    set_tests_properties(native_gemix_runtime_tests PROPERTIES ENVIRONMENT
        "GEM_RASTA_FRAMEBUFFER=/tmp/native-gemix-tests.fb;GEM_RASTA_PORT=5001;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900;GEM_RASTA_INVERSE=off;ASAN_OPTIONS=detect_leaks=0")
    set_tests_properties(native_window_api_tests PROPERTIES ENVIRONMENT
        "GEM_RASTA_FRAMEBUFFER=/tmp/native-gemix-api-tests.fb;GEM_RASTA_PORT=5001;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900;GEM_RASTA_INVERSE=off;ASAN_OPTIONS=detect_leaks=0")
    set_tests_properties(native_gemix_runtime_tests native_window_api_tests
        PROPERTIES TIMEOUT 30)
endif()

target_link_libraries(native_collection_runtime_tests PRIVATE
    native
    native_developer_options
)
target_include_directories(native_collection_runtime_tests PRIVATE
    $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)

if(APPLE OR (UNIX AND (TOOLKIT STREQUAL "MOTIF" OR TOOLKIT STREQUAL "OPENLOOK")))
    add_test(NAME native_collection_runtime_tests
        COMMAND native_collection_runtime_tests)
    set_tests_properties(native_collection_runtime_tests PROPERTIES
        TIMEOUT 30 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

if(UNIX AND TOOLKIT STREQUAL "X11")
    add_executable(native_x11_runtime_tests native_x11_runtime_tests.cpp)
    target_include_directories(native_x11_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_x11_runtime_tests PRIVATE native native_developer_options
        ${CMAKE_DL_LIBS})
    add_test(NAME native_x11_runtime_tests COMMAND native_x11_runtime_tests)
    set_tests_properties(native_x11_runtime_tests PROPERTIES TIMEOUT 20
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    add_executable(native_x11_scrollbar_tests native_x11_scrollbar_tests.cpp)
    target_include_directories(native_x11_scrollbar_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_x11_scrollbar_tests PRIVATE native native_developer_options
        ${CMAKE_DL_LIBS})
    add_test(NAME native_x11_scrollbar_tests COMMAND native_x11_scrollbar_tests)
    set_tests_properties(native_x11_scrollbar_tests PROPERTIES TIMEOUT 20
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

if(UNIX AND NOT APPLE AND TOOLKIT STREQUAL "SDL2")
    add_executable(native_canvas_input_tests native_canvas_input_tests.cpp)
    target_include_directories(native_canvas_input_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_canvas_input_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_canvas_input_tests COMMAND native_canvas_input_tests)
    set_tests_properties(native_canvas_input_tests PROPERTIES
        TIMEOUT 20 ENVIRONMENT
        "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

    add_executable(native_sdl_presentation_tests
        native_sdl_presentation_tests.cpp)
    target_include_directories(native_sdl_presentation_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_sdl_presentation_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_sdl_presentation_tests
        COMMAND native_sdl_presentation_tests)
    set_tests_properties(native_sdl_presentation_tests PROPERTIES
        TIMEOUT 15 ENVIRONMENT
        "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

    add_executable(native_sdl_composition_tests
        native_sdl_composition_tests.cpp)
    target_include_directories(native_sdl_composition_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_sdl_composition_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_sdl_composition_tests
        COMMAND native_sdl_composition_tests)
    set_tests_properties(native_sdl_composition_tests PROPERTIES
        TIMEOUT 15 ENVIRONMENT
        "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

    add_test(NAME native_collection_runtime_tests
        COMMAND native_collection_runtime_tests)
    set_tests_properties(native_collection_runtime_tests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;ASAN_OPTIONS=detect_leaks=0")

    add_executable(native_surface_runtime_tests
        native_surface_runtime_tests.cpp
    )

    target_link_libraries(native_surface_runtime_tests PRIVATE
        native
        native_developer_options
    )

    add_test(NAME native_surface_runtime_tests
        COMMAND native_surface_runtime_tests)
    set_tests_properties(native_surface_runtime_tests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;ASAN_OPTIONS=detect_leaks=0")

    add_executable(native_modal_runtime_tests
        native_modal_runtime_tests.cpp
    )

    target_link_libraries(native_modal_runtime_tests PRIVATE
        native
        native_developer_options
    )

    add_test(NAME native_modal_runtime_tests
        COMMAND native_modal_runtime_tests)
    set(native_lsan_options
        "suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan-suppressions.txt")
    set_tests_properties(native_modal_runtime_tests PROPERTIES
        ENVIRONMENT
            "SDL_VIDEODRIVER=dummy;LSAN_OPTIONS=${native_lsan_options}")
endif()

foreach(infinity_test native_infinity_tests native_audio_queue_tests native_process_tests)
    add_executable(${infinity_test} ${infinity_test}.cpp)
    target_include_directories(${infinity_test} PRIVATE ${PROJECT_SOURCE_DIR}/lib/native)
    target_link_libraries(${infinity_test} PRIVATE native native_developer_options)
    add_test(NAME ${infinity_test} COMMAND ${infinity_test})
    set_tests_properties(${infinity_test} PROPERTIES TIMEOUT 30)
endforeach()

if(TOOLKIT STREQUAL "SDL2")
    add_executable(native_keyboard_runtime_tests native_keyboard_runtime_tests.cpp)
    target_include_directories(native_keyboard_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_keyboard_runtime_tests PRIVATE native native_developer_options)
    add_test(NAME native_keyboard_runtime_tests COMMAND native_keyboard_runtime_tests)
    set_tests_properties(native_keyboard_runtime_tests PROPERTIES TIMEOUT 20 ENVIRONMENT
        "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy")
endif()

add_executable(native_mouse_shader_tests native_mouse_shader_tests.cpp)
target_include_directories(native_mouse_shader_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/native)
target_compile_definitions(native_mouse_shader_tests PRIVATE
    NATIVE_TEST_DATA="${PROJECT_SOURCE_DIR}/tests/data")
target_link_libraries(native_mouse_shader_tests PRIVATE native native_developer_options)
add_test(NAME native_mouse_shader_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_mouse_shader_tests>)

add_executable(native_terminal_screen_tests
    native_terminal_screen_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/terminal_screen.cpp)
target_include_directories(native_terminal_screen_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/lib/native)
target_compile_definitions(native_terminal_screen_tests PRIVATE
    NATIVE_TERMINAL_ASSETS="${PROJECT_SOURCE_DIR}/src/assets/retro-terminal")
target_link_libraries(native_terminal_screen_tests PRIVATE native native_developer_options)
add_test(NAME native_terminal_screen_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_terminal_screen_tests>)

if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_test(NAME native_retro_terminal_smoke
        COMMAND ${gemd_runner} $<TARGET_FILE:vision> --retro-terminal --terminal-smoke)
    set_tests_properties(native_retro_terminal_smoke PROPERTIES TIMEOUT 30
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()

# Driver fixtures exercise context ownership without a desktop or GPU.
if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_library(native_glsl_context_fixture SHARED
        native_glsl_context_fixture.cpp)
    target_link_libraries(native_glsl_context_fixture PRIVATE
        native_developer_options)
    set_target_properties(native_glsl_context_fixture PROPERTIES
        OUTPUT_NAME EGL SOVERSION 1
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/glsl-fixture")
    add_custom_command(TARGET native_glsl_context_fixture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libEGL.so.1
            "${CMAKE_CURRENT_BINARY_DIR}/glsl-fixture/libGLX.so.0")
    add_executable(native_glsl_context_tests native_glsl_context_tests.cpp)
    target_include_directories(native_glsl_context_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/native)
    target_link_libraries(native_glsl_context_tests PRIVATE
        native native_glsl_context_fixture native_developer_options)
    if(TOOLKIT STREQUAL "WAYLAND" OR TOOLKIT STREQUAL "ELEMENTARY")
        target_compile_definitions(native_glsl_context_tests PRIVATE NATIVE_TEST_WAYLAND_ONLY)
    endif()
    add_test(NAME native_glsl_context_tests COMMAND native_glsl_context_tests)
    set_tests_properties(native_glsl_context_tests PROPERTIES TIMEOUT 15
        ENVIRONMENT
            "LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/glsl-fixture;ASAN_OPTIONS=detect_leaks=0")
endif()

if(WIN32 OR APPLE)
    add_executable(native_desktop_glsl_context_tests
        native_desktop_glsl_context_tests.cpp)
    target_include_directories(native_desktop_glsl_context_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/native)
    target_link_libraries(native_desktop_glsl_context_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_desktop_glsl_context_tests
        COMMAND native_desktop_glsl_context_tests)
    set_tests_properties(native_desktop_glsl_context_tests PROPERTIES
        TIMEOUT 15 SKIP_RETURN_CODE 77)
endif()

add_executable(native_lifecycle_tests native_lifecycle_tests.cpp)
target_link_libraries(native_lifecycle_tests PRIVATE native native_developer_options)
add_test(NAME native_lifecycle_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_lifecycle_tests>)

add_executable(native_glsl_renderer_tests native_glsl_renderer_tests.cpp)
target_include_directories(native_glsl_renderer_tests PRIVATE ${PROJECT_SOURCE_DIR}/lib/native)
target_link_libraries(native_glsl_renderer_tests PRIVATE native native_developer_options)
add_test(NAME native_glsl_renderer_tests
    COMMAND $<TARGET_FILE:native_glsl_renderer_tests>)
set_tests_properties(native_glsl_renderer_tests PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)

# These SDK-only controls prove the narrow exceptions used by native runtime
# tests. Native ownership leaks, including XView menu items, remain visible.
if(TOOLKIT STREQUAL "MOTIF" OR TOOLKIT STREQUAL "OPENLOOK")
    if(TOOLKIT STREQUAL "MOTIF")
        set(allocation_toolkit motif)
        set(allocation_libraries Xm Xt X11)
        set(allocation_runtime native_motif_runtime_tests)
    else()
        set(allocation_toolkit openlook)
        set(allocation_libraries xview olgx X11)
        set(allocation_runtime native_openlook_runtime_tests)
    endif()
    set(allocation_probe native_${allocation_toolkit}_allocation_tests)
    add_executable(${allocation_probe} ${allocation_probe}.cpp)
    target_include_directories(${allocation_probe} PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    if(TOOLKIT STREQUAL "OPENLOOK")
        target_link_directories(${allocation_probe} PRIVATE /usr/openwin/lib)
    endif()
    target_link_libraries(${allocation_probe} PRIVATE
        native_developer_options ${allocation_libraries})
    foreach(scenario RANGE 0 2)
        add_test(NAME ${allocation_probe}_${scenario}
            COMMAND ${allocation_probe} ${scenario})
        set_tests_properties(${allocation_probe}_${scenario} PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=fast_unwind_on_malloc=0;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan-${allocation_toolkit}-suppressions.txt")
    endforeach()
    set_tests_properties(${allocation_runtime} PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=fast_unwind_on_malloc=0;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan-${allocation_toolkit}-suppressions.txt")
endif()

find_package(Threads REQUIRED)
add_executable(native_worker_shutdown_tests native_worker_shutdown_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/worker_shutdown.cpp)
target_link_libraries(native_worker_shutdown_tests PRIVATE native_developer_options Threads::Threads)
add_test(NAME native_worker_shutdown_tests COMMAND native_worker_shutdown_tests)
set_tests_properties(native_worker_shutdown_tests PROPERTIES TIMEOUT 15)

add_executable(native_close_runtime_tests native_close_runtime_tests.cpp)
if(APPLE)
    set_source_files_properties(native_close_runtime_tests.cpp PROPERTIES LANGUAGE OBJCXX)
endif()
target_include_directories(native_close_runtime_tests PRIVATE
    $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
target_compile_definitions(native_close_runtime_tests PRIVATE NATIVE_CLOSE_${TOOLKIT})
if(TOOLKIT STREQUAL "WAYLAND" OR TOOLKIT STREQUAL "ELEMENTARY")
    target_link_libraries(native_close_runtime_tests PRIVATE PkgConfig::GTK4)
endif()
target_link_libraries(native_close_runtime_tests PRIVATE native native_developer_options)
add_test(NAME native_close_runtime_tests
    COMMAND ${gemd_runner} $<TARGET_FILE:native_close_runtime_tests>)
set_tests_properties(native_close_runtime_tests PROPERTIES TIMEOUT 20)
if(TOOLKIT STREQUAL "SDL2")
    set_tests_properties(native_close_runtime_tests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan-suppressions.txt")
endif()

# Mesa's software driver retains tiny unload bookkeeping records after dlclose.
# Keep its code mapped during this real-driver regression so those SDK records
# retain their real roots/symbols. Native allocations remain under LeakSanitizer;
# no leak suppression or detect_leaks=0 is applied to the renderer test.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING AND
   NATIVE_ENABLE_DEVELOPER_CHECKS AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_file(native_test_mesa_driver swrast_dri.so
        PATHS /usr/lib/x86_64-linux-gnu/dri /usr/lib64/dri /usr/lib/dri
        NO_DEFAULT_PATH)
    if(native_test_mesa_driver)
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
            OUTPUT_VARIABLE native_test_asan_library OUTPUT_STRIP_TRAILING_WHITESPACE)
        set_tests_properties(native_glsl_renderer_tests PROPERTIES
            ENVIRONMENT "LD_PRELOAD=${native_test_asan_library}:${native_test_mesa_driver}")
    endif()
endif()

if(TOOLKIT STREQUAL "WMAKER")
    add_executable(native_wmaker_repaint_tests native_wmaker_repaint_tests.cpp)
    target_include_directories(native_wmaker_repaint_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_wmaker_repaint_tests PRIVATE
        native native_developer_options)
    add_test(NAME native_wmaker_repaint_tests COMMAND native_wmaker_repaint_tests)
    set_tests_properties(native_wmaker_repaint_tests PROPERTIES TIMEOUT 20)
endif()

if(TOOLKIT STREQUAL "WMAKER")
    add_executable(native_wmaker_allocation_tests native_wmaker_allocation_tests.cpp)
    target_include_directories(native_wmaker_allocation_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_directories(native_wmaker_allocation_tests PRIVATE ${WINGS_LIBRARY_DIRS})
    target_link_libraries(native_wmaker_allocation_tests PRIVATE
        native_developer_options ${WINGS_LIBRARIES} X11)
    add_test(NAME native_wmaker_allocation_tests COMMAND native_wmaker_allocation_tests)
endif()

if(HAIKU)
    add_executable(native_haiku_glsl_context_tests native_haiku_glsl_context_tests.cpp)
    target_include_directories(native_haiku_glsl_context_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/native)
    target_link_libraries(native_haiku_glsl_context_tests PRIVATE native native_developer_options)
    add_test(NAME native_haiku_glsl_context_tests COMMAND native_haiku_glsl_context_tests)
    set_tests_properties(native_haiku_glsl_context_tests PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    # Mesa 22.0.5 llvmpipe's Haiku JIT crashes; softpipe is verified.
    set_tests_properties(native_haiku_glsl_context_tests native_glsl_renderer_tests
        PROPERTIES ENVIRONMENT "GALLIUM_DRIVER=softpipe")

    add_executable(native_haiku_dispatch_tests native_haiku_dispatch_tests.cpp)
    target_link_libraries(native_haiku_dispatch_tests PRIVATE native native_developer_options)
    add_test(NAME native_haiku_dispatch_tests COMMAND native_haiku_dispatch_tests)
    set_tests_properties(native_haiku_dispatch_tests PROPERTIES TIMEOUT 30)
endif()

if(TOOLKIT STREQUAL "WMAKER")
    set_tests_properties(native_close_runtime_tests native_wmaker_allocation_tests PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=fast_unwind_on_malloc=0;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan-wmaker-suppressions.txt")
endif()

if(TOOLKIT STREQUAL "WAYLAND" OR TOOLKIT STREQUAL "ELEMENTARY")
    add_executable(native_wayland_runtime_tests native_wayland_runtime_tests.cpp
        native_gtk_theme_tests.cpp)
    target_include_directories(native_wayland_runtime_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_wayland_runtime_tests PRIVATE native
        native_developer_options PkgConfig::GTK4 PkgConfig::SOURCEVIEW)
    if(TOOLKIT STREQUAL "ELEMENTARY")
        target_link_libraries(native_wayland_runtime_tests PRIVATE PkgConfig::GRANITE)
        target_compile_definitions(native_wayland_runtime_tests PRIVATE NATIVE_TEST_ELEMENTARY)
    endif()
    add_test(NAME native_wayland_runtime_tests COMMAND native_wayland_runtime_tests)
    add_test(NAME native_collection_runtime_tests COMMAND native_collection_runtime_tests)
    set_tests_properties(native_collection_runtime_tests PROPERTIES TIMEOUT 30)
    set_tests_properties(native_wayland_runtime_tests PROPERTIES TIMEOUT 20)
endif()

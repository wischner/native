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
    add_executable(native_gemix_input_tests native_gemix_input_tests.cpp)
    target_include_directories(native_gemix_input_tests PRIVATE
        $<TARGET_PROPERTY:native,INCLUDE_DIRECTORIES>)
    target_link_libraries(native_gemix_input_tests PRIVATE native native_developer_options)
    add_test(NAME native_gemix_input_tests
        COMMAND ${gemd_runner} $<TARGET_FILE:native_gemix_input_tests>)
    set_tests_properties(native_gemix_input_tests PROPERTIES TIMEOUT 30 ENVIRONMENT
        "GEM_RASTA_FRAMEBUFFER=/tmp/native-gemix-input-tests.fb;GEM_RASTA_PORT=5012;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900;GEM_RASTA_INVERSE=on;ASAN_OPTIONS=detect_leaks=0")
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
        "GEM_RASTA_FRAMEBUFFER=/tmp/native-gemix-tests.fb;GEM_RASTA_PORT=5001;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900;GEM_RASTA_INVERSE=on;ASAN_OPTIONS=detect_leaks=0")
    set_tests_properties(native_window_api_tests PROPERTIES ENVIRONMENT
        "GEM_RASTA_FRAMEBUFFER=/tmp/native-gemix-api-tests.fb;GEM_RASTA_PORT=5001;GEM_VDI_WIDTH=904;GEM_VDI_HEIGHT=900;GEM_RASTA_INVERSE=on;ASAN_OPTIONS=detect_leaks=0")
    set_tests_properties(native_gemix_runtime_tests native_window_api_tests
        PROPERTIES TIMEOUT 30)
endif()

target_link_libraries(native_collection_runtime_tests PRIVATE
    native
    native_developer_options
)

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

add_executable(native_core_tests
    native_core_tests.cpp
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

add_test(NAME native_window_api_tests COMMAND native_window_api_tests)

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

target_link_libraries(native_collection_runtime_tests PRIVATE
    native
    native_developer_options
)

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

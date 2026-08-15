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

# Keep the target distinct from Windows resource compiler variables/tools.
add_executable(native_rc main.cpp)
set_target_properties(native_rc PROPERTIES OUTPUT_NAME rc)
target_link_libraries(native_rc PRIVATE native)

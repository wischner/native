# examples/CMakeLists.txt
include(${CMAKE_SOURCE_DIR}/cmake/native_examples.cmake)

# Examples that show a window are GUI apps; pass GUI.
native_example(app-example     ${CMAKE_CURRENT_SOURCE_DIR}/01_app_example     GUI)
native_example(painter-example ${CMAKE_CURRENT_SOURCE_DIR}/02_painter_example GUI)

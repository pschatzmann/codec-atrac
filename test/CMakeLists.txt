add_executable(test_atrac test_atrac.cpp)
target_link_libraries(test_atrac PRIVATE codec-atrac)
target_compile_definitions(test_atrac PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME atrac COMMAND test_atrac)

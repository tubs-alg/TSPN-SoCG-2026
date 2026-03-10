find_package(GTest REQUIRED)
include(GoogleTest)

# Collect all test source files
file(GLOB TEST_SOURCES "test_*.cpp")

add_executable(cpp_tests ${TEST_SOURCES})
target_link_libraries(cpp_tests PRIVATE
  GTest::gtest_main
  ${Boost_LIBRARIES}
  ${CGAL_TARGET}
  tspn_core
)
target_compile_options(
  cpp_tests PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>")
target_compile_options(
  cpp_tests PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wpedantic>")
target_compile_options(
  cpp_tests PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wextra>")

gtest_discover_tests(cpp_tests)

add_executable(
  doctests doctest.cpp
)
target_include_directories(doctests PRIVATE ../include)
target_link_libraries(doctests PRIVATE doctest::doctest)
target_link_libraries(doctests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(doctests PRIVATE ${CGAL_TARGET})
target_link_libraries(doctests PRIVATE tspn)
target_compile_options(
  doctests PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>")
target_compile_options(
  doctests PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wpedantic>")
target_compile_options(
  doctests PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wextra>")

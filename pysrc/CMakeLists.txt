# This file defines the structure of the python-binding.

# ~~~
# Dependencies via CPM ~~~~~~~~~~~~~~~~~

# Repeat the following part for every binding-module you want to build. If the
# different bindings need to interact with each other/share data, it may be
# easier (and cheaper) to have just a single binding. A single binding define as
# many classes and functions as you want.

# ~~~
# Create a new Python-module ~~~~~~~~~~~~~~~~~~~~~
find_package(pybind11 REQUIRED)
pybind11_add_module(
  _tspn_bindings # target name with leading `_` as this will be a protected
                  # module.
  tspn_bnb2/core/_tspn_bindings.cpp) # the file with the API.
# link your C++-library to the API.
target_link_libraries(_tspn_bindings PRIVATE tspn)
# link other dependencies
target_link_libraries(_tspn_bindings PRIVATE fmt::fmt ${CGAL_TARGET}
                                              gurobi::gurobi)
# enable compilation warnings
target_compile_options(
  _tspn_bindings PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>")
target_compile_definitions(
  _tspn_bindings PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES)

# This definition is important for skbuild to copy the binary to the right
# position in the package. !!! This part is critical and a wrong configuration
# leads to bad packages !!! without an error. Use a path relative to the project
# root (or !!! `cmake_install_dir`[setup.py], if used).
install(TARGETS _tspn_bindings DESTINATION ./pysrc/tspn_bnb2/core)
# ~~~
